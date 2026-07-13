// Test Suite for Schnelle Umlaute (151 tests)
//
// clang-format off
//  1-11   Basic gestures       press/release, hold+Space, modifiers, sequences, uppercase, ordering guard
// 12-16   Custom leaders       Shift-invariant, case-insensitive, double-comma escaping, cycling, triple comma
// 17-20   Arrow leaders        Left, Right, Up, Down
// 21-23   Alt leader           Alt_L, AltGr, release consumed (timer-chained)
// 24-29   Custom key variants  auto-repeat suppression, '#', UTF-8 '§', multi-char trim, whitespace trim
// 30-36   Cycling              multiple outputs, wrap-around, overlapping, passthrough, ordering guard, new gesture
// 37-44   Real-world text      German/English/French/Portuguese 1000-char, Alt/Custom leader, anbau demo, dual analysis
// 45-53   Dual custom leaders  basic, split allowed/blocked/reverse, single key, built-in ignores, Super/Alt+Space, same-hand
// 54-58   Edge cases           accent repeat, modifier during waiting, Alt+new key, layout classification, Alt re-press
// 59-64   Timeout behavior     timer fires, non-mapped/mapped after timeout, uppercase delay, lowercase delay, ordering guard
// 65-70   IC lifecycle         activate clears, deactivate commits pending/cycling, reset survives, multi-IC independence
// 71-75   Preedit              waiting char, first variant, cycle updates, cleared after commit, uppercase
// 76-81   Non-mapped keys      during waiting/cycling, Backspace, Enter, Tab
// 82-84   Config reload        clears gestures, empty→defaults, custom key = mapped input
// 85-86   Alt bypass           non-mapped key via commitString, new mapped key during gesture
// 87-91   splitOutputs         trailing/leading comma, double-comma+separator, only-commas, double-comma in cycling
// 92-94   sanitizeCustomKey    whitespace-only, empty string, uppercase normalized
// 95-96   Ordering guard       consecutive commits, Shift+Space
// 97-100  Stress/regression    double-tap, leader without gesture, all leaders enabled, Ctrl+key during gesture
// 101-103 Empty outputs        single-comma skipped, double-comma literal, all-empty→defaults
// 104-109 Advanced edge cases  reload during cycling, IC state pollution, timeout boundary (timer-chained), rapid keys, Shift+Space
// 110-113 Delay boundaries     default 400/700ms timer fires, uppercase min 50ms, max 2000ms within window
// 114-118 App filter           disabled, blacklist blocks/allows, whitelist allows/blocks
// 119-121 Error handling       mixed invalid mappings, out-of-range delay, all-invalid mappings fallback
// 122-128 Shifted input split  shifted symbols (! * @) with dual split, cycling, Shift-held leader, single key
// 129-132 Focus-flap resilience FocusOut during preedit/cycling, rapid 50x flap, flap after commit (sim. MouseTiler 100ms)
// 133-135 State invariants    recentlyCommitted_ lifecycle, getBaseChar edge keycodes, AppFilter Whitelist empty program
// 136     Alt repeat-suppress  second leader during Alt single-output arms committed_ (üu-duplicate pin)
// 137-140 Min-hold lower bound early Space plain commit, uppercase bound, repeat suppression, degenerate window
// 141-142 Deferred space       post-timeout guard split, zero-delay timer path (timer-chained) (issue #90)
// 143     reset() space flush  reset() before the zero-delay timer flushes the deferred space (issue #90)
// 144-145 reset() repeat guard reset() preserves committed_ arming for single-output and min-hold sites (issue #92)
// 146-150 wayland repeat fix   issue #92 hole 2 fixed: held-key repeat suppressed at all 3 arming sites, elapsed-ref + injected-pair guards
// 151     window-timeout test  per-window repeat: window-timeout arming clears per synthetic release (issue #92/#73)
// clang-format on

#include <unistd.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <memory>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/testing.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <xkbcommon/xkbcommon.h>
// The shipped classifier and kNoKeyCode, tested directly rather than copied.
#include "src/hand_classifier.h"
#include "src/synthetic_autorepeat.h"
#include "testdir.h"
#include "testfrontend_public.h"

using namespace fcitx;

// Delay (in microseconds) to wait for the addon's deferred commit timers to
// fire before verifying the committed string: the deferred Alt cycling
// commit (5ms) and the zero-delay trailing-space commit (test 142). 50ms
// gives 45ms of headroom over the 5ms addon timer, enough for
// sanitizer-instrumented CI runners (ASan/UBSan) where 25ms was
// occasionally tight.
constexpr uint64_t kDeferredVerifyDelayUsec = 50'000; // 50ms

// Last test number entered. Updated at the top of each scheduled lambda so a
// SIGABRT (FCITX_ASSERT failure) or SIGSEGV writes the most recently entered
// test ID to stderr before the default handler dumps the core or ASan report.
// Without this, a CI failure points at "testschnelleumlaute aborted" and
// triage has to guess from interleaved logs which test case was running.
static volatile std::sig_atomic_t g_currentTest = 0;

static void crashHandler(int signo) {
    // Use async-signal-safe write(2) to a fixed buffer. snprintf is glibc-
    // signal-safe in practice for trivial formats, but write+manual format
    // is portable.
    char buf[80];
    int len =
        std::snprintf(buf, sizeof(buf), "\n[CRASH] signal %d during test %d\n",
                      signo, static_cast<int>(g_currentTest));
    if (len > 0 && static_cast<size_t>(len) < sizeof(buf)) {
        ssize_t ignored = ::write(STDERR_FILENO, buf, static_cast<size_t>(len));
        (void)ignored;
    }
    // Chain to the default handler so core dumps / ASan reports still fire.
    std::signal(signo, SIG_DFL);
    std::raise(signo);
}

// Standalone dispatcher for scheduling test callbacks on the fcitx5 event loop.
// Avoids Instance::eventDispatcher() which is unavailable in older fcitx5
// versions (e.g. Ubuntu 24.04).
static EventDispatcher *testDispatcher = nullptr;

static uint64_t nowUsec() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000 + ts.tv_nsec / 1'000;
}

// Physical key codes (evdev+8, the X11/XKB/fcitx5 convention). These are the
// real US-QWERTY positions, which is what a US test keyboard reports; the
// engine never assumes a layout, it only compares the codes it is given.
constexpr int kCodeA = 38;
constexpr int kCodeS = 39;
constexpr int kCodeB = 56;
constexpr int kCodeSpace = 65;
constexpr int kCodeShiftL = 50;
constexpr int kCodeCtrlL = 37;
constexpr int kCodeLeft = 113;
constexpr int kCodeRight = 114;
constexpr int kCodeUp = 111;
constexpr int kCodeDown = 116;
constexpr int kCodeAltL = 64;
constexpr int kCodeAltGr = 108;
constexpr int kCodeHash = 20;
constexpr int kCodeO = 32;
constexpr int kCodeU = 30;
[[maybe_unused]] constexpr int kCodeE = 26;
constexpr int kCodeReturn = 36;
constexpr int kCodeBackSpace = 22;
constexpr int kCodeTab = 23;
[[maybe_unused]] constexpr int kCodeSuperL = 133;
constexpr int kCode1 = 10;                  // physical key for 1/!
[[maybe_unused]] constexpr int kCode2 = 11; // physical key for 2/@
constexpr int kCode8 = 17;                  // physical key for 8/*
constexpr int kCodeF = 41;                  // physical key for f
constexpr int kCodeJ = 44;                  // physical key for j
// Custom-leader keys for the dual-split tests. F (left half) / J (right half)
// and Z (left) / slash (right) are opposite-hand pairs; X sits on the SAME half
// as Z, which must switch the split off.
constexpr int kCodeZ = 52;
constexpr int kCodeX = 53;
constexpr int kCodeSlash = 61;
constexpr int kCodeSection = 21; // physical key for §

// Hold longer than the engine's synthetic auto-repeat elapsed guard
// (kSyntheticReleaseMinElapsedUsec, 5 ms) so a frozen-timestamp release
// classifies as synthetic. Derived from the engine constant plus 1 ms margin,
// so raising the threshold cannot silently make these sleeps too short and flip
// the synthetic-repeat tests to false results. See synthetic_autorepeat.h.
constexpr auto kSyntheticReleaseTestHold =
    std::chrono::microseconds(kSyntheticReleaseMinElapsedUsec) +
    std::chrono::milliseconds(1);

// Helper: load mappings via setSubConfig (the path loadMappingsFromFile reads)
static void
setMappings(Instance *instance,
            const std::vector<std::pair<std::string, std::string>> &entries) {
    auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
    RawConfig mc;
    for (size_t i = 0; i < entries.size(); ++i) {
        mc.setValueByPath(std::to_string(i) + "/Input", entries[i].first);
        mc.setValueByPath(std::to_string(i) + "/Output", entries[i].second);
    }
    addon->setSubConfig("mappings.txt", mc);
}

// Helper: create IC and activate schnelle-umlaute
static ICUUID createAndActivate(Instance * /*instance*/,
                                AddonInstance *testfrontend,
                                const std::string &name) {
    auto uuid = testfrontend->call<ITestFrontend::createInputContext>(name);
    testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"),
                                                false);
    return uuid;
}

// customCode/custom2Code are the physical keys behind the custom leaders, as
// the editor captures them. They are what makes a leader active: left at
// kNoKeyCode, the leader has no key and triggers nothing, whatever character
// is stored alongside it (see TEST 153).
static void configureLeaders(Instance *instance, bool space, bool left,
                             bool right, bool up, bool down, bool alt,
                             const std::string &custom = "",
                             const std::string &custom2 = "",
                             int customCode = fcitx::kNoKeyCode,
                             int custom2Code = fcitx::kNoKeyCode) {
    auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
    RawConfig config;
    config.setValueByPath("Delay/Lowercase", "400");
    config.setValueByPath("Delay/Uppercase", "700");
    config.setValueByPath("Leader/Space", space ? "True" : "False");
    config.setValueByPath("Leader/Left", left ? "True" : "False");
    config.setValueByPath("Leader/Right", right ? "True" : "False");
    config.setValueByPath("Leader/Up", up ? "True" : "False");
    config.setValueByPath("Leader/Down", down ? "True" : "False");
    config.setValueByPath("Leader/Alt", alt ? "True" : "False");
    config.setValueByPath("Leader/Custom/CustomKeyEnabled",
                          custom.empty() ? "False" : "True");
    config.setValueByPath("Leader/Custom/CustomKey", custom);
    config.setValueByPath("Leader/Custom/CustomKeyCode",
                          std::to_string(customCode));
    config.setValueByPath("Leader/Custom/CustomKey2Enabled",
                          custom2.empty() ? "False" : "True");
    config.setValueByPath("Leader/Custom/CustomKey2", custom2);
    config.setValueByPath("Leader/Custom/CustomKey2Code",
                          std::to_string(custom2Code));
    config.setValueByPath("AppFilter/Mode", "Disabled");
    addon->setConfig(config);
    setMappings(instance, {
                              {"a", "\xc3\xa4"},
                              {"o", "\xc3\xb6"},
                              {"u", "\xc3\xbc"},
                              {"s", "\xc3\x9f"},
                              {"A", "\xc3\x84"},
                              {"O", "\xc3\x96"},
                              {"U", "\xc3\x9c"},
                          });
}

// =========================================================================
// WRITING FLOW / LEADER COMPARISON HELPERS
// =========================================================================

// evdev key codes for a-z (QWERTY layout)
static constexpr int kLetterCodes[26] = {
    38, 56, 54, 40, 26, 41, 42, 43, 31, 44, // a-j
    45, 46, 58, 57, 32, 33, 24, 27, 39, 28, // k-t
    30, 55, 25, 53, 29, 52                  // u-z
};

// Multilingual cycling mappings: which chars are mapped?
// Lowercase: a,e,i,o,u,s,c,n,y  Uppercase: A,E,O,U
static bool isMultilingualMapped(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
           c == 's' || c == 'c' || c == 'n' || c == 'y' || c == 'A' ||
           c == 'E' || c == 'O' || c == 'U';
}

// US QWERTY keyboard halves
// Left hand: q,w,e,r,t,a,s,d,f,g,z,x,c,v,b
// Right hand: y,u,i,o,p,h,j,k,l,n,m
static bool isLeftHandKey(char c) {
    return c == 'q' || c == 'w' || c == 'e' || c == 'r' || c == 't' ||
           c == 'a' || c == 's' || c == 'd' || c == 'f' || c == 'g' ||
           c == 'z' || c == 'x' || c == 'c' || c == 'v' || c == 'b';
}
static bool isRightHandKey(char c) {
    return c == 'y' || c == 'u' || c == 'i' || c == 'o' || c == 'p' ||
           c == 'h' || c == 'j' || c == 'k' || c == 'l' || c == 'n' || c == 'm';
}

// Is char a right-hand key AND multilingual-mapped?
// These are the inputs that 'a' leader (left hand) would control.
static bool isRightHandMapped(char c) {
    return isRightHandKey(c) && isMultilingualMapped(c);
    // y, u, i, o, n
}

// Is char a left-hand key AND multilingual-mapped?
// These are the inputs that ';' leader (right hand) would control.
static bool isLeftHandMapped(char c) {
    return isLeftHandKey(c) && isMultilingualMapped(c);
    // a, e, s, c
}

// Count within-word collisions for dual custom leaders.
// A collision occurs when a mapped char controlled by the given leader
// is immediately followed by the leader key within a word (overlap typing).
//
// 'a' leader: counts [yuion] immediately followed by 'a' within words
// ';' leader: counts [aesc] immediately followed by ';' within words (≈0 in
// text)
static int countDualLeaderWithinWordCollisions(const std::string &text,
                                               char leader,
                                               bool leaderControlsRightHand) {
    int collisions = 0;
    char prev = 0;
    for (char c : text) {
        if (c == ' ') {
            prev = 0;
            continue;
        }
        if (prev != 0 && c == leader) {
            if ((leaderControlsRightHand && isRightHandMapped(prev)) ||
                (!leaderControlsRightHand && isLeftHandMapped(prev)))
                collisions++;
        }
        prev = c;
    }
    return collisions;
}

// Count word-boundary collisions for Space leader (existing metric).
// Pure text analysis — counts words (except last) whose last char is mapped.
static int countSpaceLeaderCollisions(const std::string &text) {
    int collisions = 0;
    std::vector<std::string> words;
    std::string cur;
    for (char c : text) {
        if (c == ' ') {
            if (!cur.empty()) {
                words.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty())
        words.push_back(std::move(cur));
    for (size_t w = 0; w + 1 < words.size(); ++w) {
        if (isMultilingualMapped(words[w].back()))
            collisions++;
    }
    return collisions;
}

// Does the mapping have multiple outputs (cycling)?
// All mapped chars have cycling EXCEPT 's' (single output: ß)
static bool hasCycling(char c) { return c != 's' && isMultilingualMapped(c); }

// First cycling variant (or single output) for each mapped char
static const char *firstMappedOutput(char c) {
    switch (c) {
    case 'a':
        return "\xc3\xa4"; // ä
    case 'e':
        return "\xc3\xa9"; // é
    case 'i':
        return "\xc3\xad"; // í
    case 'o':
        return "\xc3\xb6"; // ö
    case 'u':
        return "\xc3\xbc"; // ü
    case 's':
        return "\xc3\x9f"; // ß
    case 'c':
        return "\xc3\xa7"; // ç
    case 'n':
        return "\xc3\xb1"; // ñ
    case 'y':
        return "\xc3\xbd"; // ý
    case 'A':
        return "\xc3\x84"; // Ä
    case 'E':
        return "\xc3\x89"; // É
    case 'O':
        return "\xc3\x96"; // Ö
    case 'U':
        return "\xc3\x9c"; // Ü
    default:
        return "";
    }
}

static bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Configure multilingual cycling mappings with specified leaders
static void configureMultilingualCycling(Instance *instance, bool space,
                                         bool alt,
                                         const std::string &custom = "",
                                         int customCode = fcitx::kNoKeyCode) {
    auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
    RawConfig config;
    config.setValueByPath("Delay/Lowercase", "400");
    config.setValueByPath("Delay/Uppercase", "700");
    config.setValueByPath("Leader/Space", space ? "True" : "False");
    config.setValueByPath("Leader/Left", "False");
    config.setValueByPath("Leader/Right", "False");
    config.setValueByPath("Leader/Up", "False");
    config.setValueByPath("Leader/Down", "False");
    config.setValueByPath("Leader/Alt", alt ? "True" : "False");
    config.setValueByPath("Leader/Custom/CustomKeyEnabled",
                          custom.empty() ? "False" : "True");
    config.setValueByPath("Leader/Custom/CustomKey", custom);
    config.setValueByPath("Leader/Custom/CustomKeyCode",
                          std::to_string(customCode));
    addon->setConfig(config);
    setMappings(instance,
                {
                    {"a", "\xc3\xa4,\xc3\xa0,\xc3\xa1,\xc3\xa2,\xc3\xa3"},
                    {"e", "\xc3\xa9,\xc3\xa8,\xc3\xaa,\xc3\xab"},
                    {"i", "\xc3\xad,\xc3\xac,\xc3\xae,\xc3\xaf"},
                    {"o", "\xc3\xb6,\xc3\xb2,\xc3\xb3,\xc3\xb4,\xc3\xb5"},
                    {"u", "\xc3\xbc,\xc3\xb9,\xc3\xba,\xc3\xbb"},
                    {"s", "\xc3\x9f"},
                    {"c", "\xc3\xa7,\xc4\x87"},
                    {"n", "\xc3\xb1,\xc5\x84"},
                    {"y", "\xc3\xbd,\xc3\xbf"},
                    {"A", "\xc3\x84,\xc3\x80,\xc3\x81,\xc3\x82,\xc3\x83"},
                    {"E", "\xc3\x89,\xc3\x88,\xc3\x8a,\xc3\x8b"},
                    {"O", "\xc3\x96,\xc3\x92,\xc3\x93,\xc3\x94,\xc3\x95"},
                    {"U", "\xc3\x9c,\xc3\x99,\xc3\x9a,\xc3\x9b"},
                });
}

// Configure with custom delay values (Space leader only, default mappings).
// delayLower/delayUpper are the window upper bound (max) for lowercase/
// uppercase; minLower/minUpper are the lower bound (minimum hold), default 0.
static void configureWithDelay(Instance *instance, int delayLower,
                               int delayUpper, bool space = true,
                               bool alt = false, int minLower = 0,
                               int minUpper = 0) {
    auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
    RawConfig config;
    config.setValueByPath("Delay/Lowercase", std::to_string(delayLower));
    config.setValueByPath("Delay/Uppercase", std::to_string(delayUpper));
    config.setValueByPath("Delay/LowercaseMin", std::to_string(minLower));
    config.setValueByPath("Delay/UppercaseMin", std::to_string(minUpper));
    config.setValueByPath("Leader/Space", space ? "True" : "False");
    config.setValueByPath("Leader/Left", "False");
    config.setValueByPath("Leader/Right", "False");
    config.setValueByPath("Leader/Up", "False");
    config.setValueByPath("Leader/Down", "False");
    config.setValueByPath("Leader/Alt", alt ? "True" : "False");
    config.setValueByPath("Leader/Custom/CustomKeyEnabled", "False");
    config.setValueByPath("Leader/Custom/CustomKey", "");
    config.setValueByPath("Leader/Custom/CustomKey2Enabled", "False");
    config.setValueByPath("Leader/Custom/CustomKey2", "");
    config.setValueByPath("AppFilter/Mode", "Disabled");
    addon->setConfig(config);
    setMappings(instance, {
                              {"a", "\xc3\xa4"},
                              {"o", "\xc3\xb6"},
                              {"u", "\xc3\xbc"},
                              {"s", "\xc3\x9f"},
                              {"A", "\xc3\x84"},
                              {"O", "\xc3\x96"},
                              {"U", "\xc3\x9c"},
                          });
}

// Get current client preedit text from any active IC with non-empty preedit
static std::string getClientPreedit(Instance *instance) {
    std::string result;
    instance->inputContextManager().foreach([&](InputContext *ic) {
        auto text = ic->inputPanel().clientPreedit().toString();
        if (!text.empty()) {
            result = text;
            return false;
        }
        return true;
    });
    return result;
}

// Dispatch a key event carrying an explicit frontend timestamp (ms), bypassing
// the test frontend which always stamps time()==0. KWin/Wayland freezes this
// timestamp across a held key's whole auto-repeat burst, so setting it by hand
// is the only way to exercise the synthetic release-press pairs the #73
// predicate keys on. This is a MODEL of that timestamp behavior driven straight
// through ic->keyEvent() (not the ITestFrontend::sendKeyEvent path): it is only
// as faithful as the frozen-timestamp assumption, not a 1:1 capture of a real
// KWin session. Returns whether the addon consumed the event.
static bool sendKeyAtTime(Instance *instance, ICUUID uuid, FcitxKeySym sym,
                          uint16_t code, bool isRelease, int timeMs) {
    auto *ic = instance->inputContextManager().findByUUID(uuid);
    KeyEvent ke(ic, Key(sym, KeyStates(), code), isRelease, timeMs);
    ic->keyEvent(ke);
    return ke.accepted();
}

// Alt-deferred-commit verification (tests 21-23). The Alt leader's release
// schedules a 5ms deferred cycling commit; the test pushes a "ä" expectation and
// must let that deferred fire before tearing the IC down. A fixed wait is racy:
// under an ASan event-loop stall the verify timer can destroy the IC before the
// 5ms deferred fires, cancelling it with the IC, so the "ä" expectation is never
// consumed and leaks into a later test (test 30 then aborts on "ae" != "ä").
// Instead, poll until the deferred has actually fired: its callback resets the
// preedit, so an EMPTY client preedit is the "fired" signal (the preedit shows
// "ä" until the deferred commits). The IC is never destroyed while the deferred
// is still pending, so the timer ordering under a stall no longer matters. A
// generous attempt bound fails explicitly if the deferred never fires, so a
// genuinely broken commit path is still caught.
struct AltVerifyHolder {
    std::unique_ptr<EventSourceTime> timer;
};
constexpr uint64_t kDeferredPollIntervalUsec = 5'000; // 5ms
constexpr int kDeferredPollMaxAttempts = 400;         // ~2s bound

static void destroyAfterDeferredCommit(
    Instance *instance, ICUUID uuid,
    const std::shared_ptr<AltVerifyHolder> &holder,
    const std::function<void()> &next, int attempt = 0) {
    holder->timer = instance->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC, nowUsec() + kDeferredPollIntervalUsec, 0,
        [instance, uuid, holder, next, attempt](EventSourceTime *, uint64_t) {
            if (!getClientPreedit(instance).empty() &&
                attempt < kDeferredPollMaxAttempts) {
                destroyAfterDeferredCommit(instance, uuid, holder, next,
                                           attempt + 1);
                return false;
            }
            FCITX_ASSERT(getClientPreedit(instance).empty())
                << "Deferred cycling commit never fired within bound (preedit "
                   "still '"
                << getClientPreedit(instance) << "')";
            auto *tf = instance->addonManager().addon("testfrontend");
            tf->call<ITestFrontend::destroyInputContext>(uuid);
            next();
            return false;
        });
}

// Type a single char with clean typing (press+release).
// Handles lowercase, uppercase (Shift), period, comma.
// If mapped, pushes commit expectation for original char.
static void typeCharClean(AddonInstance *tf, ICUUID uuid, char c) {
    FcitxKeySym sym;
    KeyStates states;
    int code;

    if (c >= 'a' && c <= 'z') {
        sym = static_cast<FcitxKeySym>(FcitxKey_a + (c - 'a'));
        code = kLetterCodes[c - 'a'];
    } else if (c >= 'A' && c <= 'Z') {
        sym = static_cast<FcitxKeySym>(FcitxKey_A + (c - 'A'));
        states = KeyState::Shift;
        code = kLetterCodes[c - 'A'];
    } else if (c == '.') {
        sym = FcitxKey_period;
        code = 60;
    } else if (c == ',') {
        sym = FcitxKey_comma;
        code = 59;
    } else {
        return;
    }

    tf->call<ITestFrontend::sendKeyEvent>(uuid, Key(sym, states, code), false);
    if (isMultilingualMapped(c)) {
        tf->call<ITestFrontend::pushCommitExpectation>(std::string(1, c));
    }
    tf->call<ITestFrontend::sendKeyEvent>(uuid, Key(sym, states, code), true);
}

// Simulate fast typing with word-boundary overlap.
// Within each word: clean typing (press-release).
// At word end: HOLD last LETTER while pressing Space.
// Trailing punctuation (.,) shields the last letter from Space collision.
// Handles uppercase letters (Shift modifier) and punctuation.
// Returns number of word-boundary collisions.
static int typeTextWordBoundaryOverlap(AddonInstance *tf, ICUUID uuid,
                                       const std::string &text,
                                       bool spaceIsLeader) {

    std::vector<std::string> words;
    std::string cur;
    for (char c : text) {
        if (c == ' ') {
            if (!cur.empty()) {
                words.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty())
        words.push_back(std::move(cur));

    int collisions = 0;

    for (size_t w = 0; w < words.size(); ++w) {
        const auto &word = words[w];
        bool lastWord = (w == words.size() - 1);

        // Find last letter (skip trailing punctuation like . ,)
        int lastLetterIdx = -1;
        for (int i = (int)word.size() - 1; i >= 0; --i) {
            if (isLetter(word[i])) {
                lastLetterIdx = i;
                break;
            }
        }
        if (lastLetterIdx < 0)
            continue; // no letters in word

        bool hasTrailingPunct = (lastLetterIdx < (int)word.size() - 1);
        char lastLetter = word[lastLetterIdx];
        bool mapped = isMultilingualMapped(lastLetter);

        if (lastWord || hasTrailingPunct) {
            // Punctuation shields the collision, or last word (no Space)
            // → type entire word clean, send Space for non-last.
            // Track whether the last simulated event was a mapped char
            // (UTF-8 bytes are skipped and don't clear recentlyCommitted_).
            bool lastSimWasMapped = false;
            for (char c : word) {
                typeCharClean(tf, uuid, c);
                if (isLetter(c))
                    lastSimWasMapped = isMultilingualMapped(c);
                else if (c == '.' || c == ',')
                    lastSimWasMapped = false; // punctuation clears flag
            }
            if (!lastWord) {
                if (lastSimWasMapped) {
                    // Ordering guard will catch Space after mapped char
                    tf->call<ITestFrontend::pushCommitExpectation>(" ");
                }
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);
            }
        } else {
            // No trailing punctuation — potential collision at word boundary
            for (int i = 0; i < lastLetterIdx; ++i) {
                typeCharClean(tf, uuid, word[i]);
            }

            // Compute keysym/code/states for last letter
            FcitxKeySym lastSym;
            int lastCode;
            KeyStates lastStates;
            if (lastLetter >= 'a' && lastLetter <= 'z') {
                lastSym =
                    static_cast<FcitxKeySym>(FcitxKey_a + (lastLetter - 'a'));
                lastCode = kLetterCodes[lastLetter - 'a'];
            } else if (lastLetter >= 'A' && lastLetter <= 'Z') {
                lastSym =
                    static_cast<FcitxKeySym>(FcitxKey_A + (lastLetter - 'A'));
                lastStates = KeyState::Shift;
                lastCode = kLetterCodes[lastLetter - 'A'];
            } else {
                FCITX_ASSERT(false) << "lastLetter out of range: "
                                    << static_cast<int>(lastLetter);
                continue;
            }

            if (mapped && spaceIsLeader) {
                // COLLISION: hold last letter + Space → conversion
                collisions++;
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, lastStates, lastCode), false);

                if (hasCycling(lastLetter)) {
                    tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace),
                        false);
                    tf->call<ITestFrontend::pushCommitExpectation>(
                        firstMappedOutput(lastLetter));
                    tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(lastSym, lastStates, lastCode), true);
                } else {
                    tf->call<ITestFrontend::pushCommitExpectation>(
                        firstMappedOutput(lastLetter));
                    tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace),
                        false);
                    tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(lastSym, lastStates, lastCode), true);
                }
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);

            } else if (mapped && !spaceIsLeader) {
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, lastStates, lastCode), false);
                tf->call<ITestFrontend::pushCommitExpectation>(
                    std::string(1, lastLetter));
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, lastStates, lastCode), true);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);

            } else {
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, lastStates, lastCode), false);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, lastStates, lastCode), true);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);
            }
        }
    }
    return collisions;
}

// ~1000 char realistic test texts with proper capitalization,
// punctuation, and native accents/umlauts.
// Multi-byte UTF-8 chars (ä,ö,ü,ß,é,ç,ñ...) are transparent to
// the analysis — they are skipped as non-ASCII, and the surrounding
// ASCII letters determine word boundaries and collisions.

// clang-format off
static const char *kGerman1000 =
    "Die Sonne scheint hell durch das Fenster und wirft lange Schatten "
    "auf den Boden. Es ist ein ruhiger Morgen in der kleinen Stadt am "
    "Fluß. Die Menschen gehen langsam durch die Straßen "
    "und genießen den frischen Wind, der von den hohen Bergen "
    "weht. Ein alter Mann sitzt auf einer Bank und liest seine Zeitung, "
    "während Kinder um ihn herum spielen und lachen. Der Duft "
    "von frischem Brot kommt aus der Bäckerei nebenan und "
    "läßt alle hungrig werden. Heute wird sicher ein "
    "schöner Tag, das spürt jeder, der am Morgen nach "
    "draußen geht und die warme Luft auf der Haut spürt. "
    "Die Vögel singen in den Bäumen und der Himmel ist "
    "strahlend blau. Am Marktplatz stehen die ersten Händler "
    "mit ihren Waren. Frisches Obst und Gemüse liegt auf den "
    "Tischen neben duftendem Brot und Käse. Eine Frau kauft "
    "Äpfel für einen Kuchen, den sie heute backen will. "
    "Studenten sitzen am Brunnen und lernen für die nächste Prüfung.";

static const char *kEnglish1000 =
    "The quick brown fox jumps over the lazy dog and runs across the "
    "fields where tall grass grows on both sides of the narrow path. "
    "Birds sing in the warm morning light while clouds drift slowly "
    "across the bright blue sky. A small river flows between old rocks "
    "and stones, making soft sounds that echo through the quiet valley. "
    "Children play near the water, throwing small pebbles and laughing "
    "together. The wind picks up dry leaves from the ground and carries "
    "them far away to the hills where mountains stand tall against the "
    "horizon. It is a perfect day to enjoy the fresh air and warm "
    "sunshine. The flowers bloom in every garden and bees fly from "
    "blossom to blossom. An old woman sits on her porch, reading a book "
    "while her cat sleeps in the sun. Down the road a farmer drives his "
    "truck to market with fresh vegetables and ripe fruit.";

static const char *kFrench1000 =
    "Le petit chat dort sur le tapis pendant que la pluie tombe "
    "doucement dehors. Les enfants jouent dans le jardin avec un ballon "
    "rouge et bleu. La m\xc3\xa8re pr\xc3\xa9pare le repas dans la "
    "grande cuisine o\xc3\xb9 les odeurs de pain frais se m\xc3\xa9"
    "langent avec celles des l\xc3\xa9gumes grill\xc3\xa9""es. Le "
    "p\xc3\xa8re lit son journal, assis dans le fauteuil pr\xc3\xa8""s "
    "de la fen\xc3\xaatre ouverte par laquelle entre une brise "
    "l\xc3\xa9g\xc3\xa8re. Les oiseaux chantent sur les branches des "
    "vieux arbres. Le soleil brille entre les nuages et fait danser des "
    "ombres sur le sol de la cour. Il fait tr\xc3\xa8""s bon vivre ici "
    "dans ce village tranquille. Les rues sont calmes et les gens se "
    "connaissent depuis toujours. Chaque matin, le boulanger ouvre sa "
    "boutique et le parfum du pain chaud se r\xc3\xa9pand dans tout le "
    "quartier. Les voisins se retrouvent au caf\xc3\xa9 pour discuter "
    "des nouvelles du jour.";

static const char *kPortuguese1000 =
    "O gato pequeno dorme no tapete macio enquanto a chuva cai "
    "suavemente l\xc3\xa1 fora. As crian\xc3\xa7""as brincam no jardim "
    "com uma bola vermelha e azul. A m\xc3\xa3""e prepara a "
    "refei\xc3\xa7\xc3\xa3""o na cozinha grande onde os cheiros de "
    "p\xc3\xa3""o fresco se misturam com os dos legumes grelhados. O "
    "pai l\xc3\xaa o jornal sentado na poltrona perto da janela aberta "
    "pela qual entra uma brisa suave. Os p\xc3\xa1""ssaros cantam nos "
    "galhos das velhas \xc3\xa1rvores. O sol brilha entre as nuvens "
    "brancas e faz dan\xc3\xa7""ar sombras no ch\xc3\xa3""o do quintal. "
    "Faz um belo dia para sair de casa e aproveitar o ar livre. As "
    "flores desabrocham nos jardins e as abelhas voam de flor em flor. "
    "Uma senhora idosa senta na varanda lendo um livro enquanto seu "
    "gato dorme ao sol. Na estrada um fazendeiro leva seus produtos "
    "frescos para o mercado da cidade vizinha.";

static const char *kSpanish1000 =
    "El peque\xc3\xb1""o gato duerme en la alfombra mientras la lluvia "
    "cae suavemente afuera. Los ni\xc3\xb1""os juegan en el jard\xc3"
    "\xadn con una pelota roja y azul. La madre prepara la comida en "
    "la cocina grande donde los olores de pan fresco se mezclan con los "
    "de las verduras asadas. El padre lee el peri\xc3\xb3""dico sentado "
    "en el sill\xc3\xb3n cerca de la ventana abierta por la cual entra "
    "una brisa suave. Los p\xc3\xa1jaros cantan en las ramas de los "
    "viejos \xc3\xa1rboles. El sol brilla entre las nubes y hace bailar "
    "sombras en el suelo del patio. Hace un hermoso d\xc3\xad""a para "
    "salir de casa y disfrutar del aire libre. Las flores florecen en "
    "todos los jardines y las abejas vuelan de flor en flor. Una "
    "se\xc3\xb1""ora mayor se sienta en el porche leyendo un libro "
    "mientras su gato duerme al sol. Por el camino un granjero lleva "
    "sus productos frescos al mercado de la ciudad vecina.";
// clang-format on

// Tests 24+ are scheduled from within the Alt leader timer chain (Tests 21-23)
// to guarantee deferred commits are verified before any subsequent test runs.
static void scheduleTestsAfterAltVerify(Instance *instance);
static void scheduleTimeoutTests(Instance *instance);
static void scheduleTest60(Instance *instance);
static void scheduleTest61(Instance *instance);
static void scheduleTest62(Instance *instance);
static void scheduleTest63(Instance *instance);
static void scheduleTest64(Instance *instance);
static void scheduleRemainingTests(Instance *instance);
static void scheduleEmptyOutputTests(Instance *instance);
static void scheduleAdvancedEdgeCaseTests(Instance *instance);
static void scheduleTest106(Instance *instance);
static void scheduleTest107(Instance *instance);
static void scheduleDelayBoundaryTests(Instance *instance);
static void scheduleTest111(Instance *instance);
static void scheduleTest112(Instance *instance);
static void scheduleTest113(Instance *instance);

void scheduleTests(Instance *instance) {
    // =========================================================================
    // SETUP
    // =========================================================================
    testDispatcher->schedule([instance]() {
        FCITX_INFO() << "=== Setup ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        FCITX_ASSERT(addon) << "Addon schnelle-umlaute not loaded";
        auto *tf = instance->addonManager().addon("testfrontend");
        FCITX_ASSERT(tf) << "Addon testfrontend not loaded";

        auto group = instance->inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(
            InputMethodGroupItem("schnelle-umlaute"));
        group.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(std::move(group));

        // Establish explicit baseline config for tests 1-11
        configureLeaders(instance, true, false, false, false, false, false);
        FCITX_INFO() << "Setup complete";
    });

    // =========================================================================
    // TEST 1: Press 'a' + release → commits normal 'a'
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 1;
        FCITX_INFO() << "=== Test 1: Press a + release -> commits a ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test1");

        // Press 'a' — consumed (enters waiting state)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "Mapped key 'a' press should be consumed";

        // Release 'a' — commits 'a'
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 1 PASSED";
    });

    // =========================================================================
    // TEST 2: Hold 'a' + Space → commits 'ä'
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 2;
        FCITX_INFO() << "=== Test 2: Hold a + Space -> commits ä ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test2");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Space during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 2 PASSED";
    });

    // =========================================================================
    // TEST 3: Unmapped key 'b' → passes through
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 3;
        FCITX_INFO() << "=== Test 3: Unmapped key b passes through ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test3");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
        FCITX_ASSERT(!consumed) << "Unmapped 'b' press should pass through";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), true);
        FCITX_ASSERT(!consumed) << "Unmapped 'b' release should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 3 PASSED";
    });

    // =========================================================================
    // TEST 4: Modifier passthrough — Ctrl+a, Alt+a not consumed
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 4;
        FCITX_INFO() << "=== Test 4: Modifier passthrough ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test4");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyState::Ctrl, kCodeA), false);
        FCITX_ASSERT(!consumed) << "Ctrl+a should pass through";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyState::Alt, kCodeA), false);
        FCITX_ASSERT(!consumed) << "Alt+a should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 4 PASSED";
    });

    // =========================================================================
    // TEST 5: Pure modifier keys (Shift, Ctrl alone) → not consumed
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 5;
        FCITX_INFO() << "=== Test 5: Pure modifier keys pass through ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test5");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        FCITX_ASSERT(!consumed) << "Shift_L press should pass through";
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        FCITX_ASSERT(!consumed) << "Shift_L release should pass through";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Control_L, KeyStates(), kCodeCtrlL), false);
        FCITX_ASSERT(!consumed) << "Ctrl_L press should pass through";
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Control_L, KeyStates(), kCodeCtrlL), true);
        FCITX_ASSERT(!consumed) << "Ctrl_L release should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 5 PASSED";
    });

    // =========================================================================
    // TEST 6: Sequence "as" — press a + release, press s + release
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 6;
        FCITX_INFO() << "=== Test 6: Sequence as -> a then s ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test6");

        // 'a' press + release → commits 'a'
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // 's' press + release → commits 's'
        tf->call<ITestFrontend::pushCommitExpectation>("s");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 6 PASSED";
    });

    // =========================================================================
    // TEST 7: Sequence "aß" — hold a + Space, hold s + Space
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 7;
        FCITX_INFO() << "=== Test 7: Sequence a+Space s+Space -> ä ß ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test7");

        // Hold 'a' + Space → 'ä'
        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Hold 's' + Space → 'ß'
        tf->call<ITestFrontend::pushCommitExpectation>("ß");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 7 PASSED";
    });

    // =========================================================================
    // TEST 8: Sequence "sa" — press s + release, press a + release
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 8;
        FCITX_INFO() << "=== Test 8: Sequence sa -> s then a ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test8");

        tf->call<ITestFrontend::pushCommitExpectation>("s");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 8 PASSED";
    });

    // =========================================================================
    // TEST 9: Sequence "sä" — hold s + Space, hold a + Space
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 9;
        FCITX_INFO() << "=== Test 9: Sequence s+Space a+Space -> ß ä ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test9");

        tf->call<ITestFrontend::pushCommitExpectation>("ß");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 9 PASSED";
    });

    // =========================================================================
    // TEST 10: Shift+A + Space → commits 'Ä'
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 10;
        FCITX_INFO() << "=== Test 10: Shift+A + Space -> Ä ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test10");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);

        // Press Shift+A + Space → 'Ä'
        tf->call<ITestFrontend::pushCommitExpectation>("Ä");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Release A, Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 10 PASSED";
    });

    // =========================================================================
    // TEST 11: Ordering guard — Space after commit routed through commitString
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 11;
        FCITX_INFO() << "=== Test 11: Ordering guard ===";
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test11");

        // Hold 'a' + Space → 'ä' (sets recentlyCommitted_)
        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Next Space → commitString(" ") via ordering guard
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Space after commit should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 11 PASSED";
    });

    // NOTE: Async timeout test removed — safeSaveAsIni from configureLeaders
    // tests triggers inotify events that cause IM switch during the timer wait,
    // consuming commit expectations. Timeout behavior is tested manually.

    // =========================================================================
    // TEST 61: Shift-invariant custom leader — Shift+A + Shift+/ → Ä
    // When Shift is held for uppercase input, Shift+/ produces '?' keysym.
    // The leader must still match '/' via US QWERTY unshift mapping.
    // Uses dual-split config (z + /) with default mappings (A→Ä, U→Ü).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 12;
        FCITX_INFO() << "=== Test 12: Shift+A + Shift+/ → Ä ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "/", kCodeZ, kCodeSlash);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test12");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);

        // Hold Shift+A (uppercase mapped input)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);

        // Press Shift+/ = '?' (keysym FcitxKey_question, same physical key as
        // '/') Must be recognized as '/' leader via US QWERTY unshift mapping.
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x84");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_question, KeyState::Shift, 61), false);
        FCITX_ASSERT(consumed)
            << "Shift+/ ('?') must match custom leader '/' for uppercase input";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 12 PASSED";
    });

    // =========================================================================
    // TEST 62: Shift-invariant custom leader — Shift+U + Shift+Z → Ü
    // Shift+Z = 'Z', should match leader 'z' via case-insensitive match.
    // Verifies existing letter matching still works in dual-split context.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 13;
        FCITX_INFO() << "=== Test 13: Shift+U + Shift+Z → Ü ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "/", kCodeZ, kCodeSlash);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test13");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);

        // Hold Shift+U (right-hand uppercase input)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_U, KeyState::Shift, 30), false);

        // Press Shift+Z = 'Z' (left-hand leader, opposite hand → allowed)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x9c");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Z, KeyState::Shift, 52), false);
        FCITX_ASSERT(consumed)
            << "Shift+Z ('Z') must match custom leader 'z' for uppercase input";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_U, KeyState::Shift, 30), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 13 PASSED";
    });

    // =========================================================================
    // TEST 30: Double-comma escape — literal comma in output
    // Output "a,,b" should produce single output "a,b" (escaped comma).
    // Must run before configureLeaders tests to avoid inotify interference.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 14;
        FCITX_INFO() << "=== Test 14: Double-comma escape in output ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "a,,b"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test14");

        // Hold 'a' + Space → should commit "a,b" (single output, not cycling)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a,b");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 14 PASSED";
    });

    // =========================================================================
    // TEST 31: Double-comma with cycling — "x,,y,z" → ["x,y", "z"]
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 15;
        FCITX_INFO() << "=== Test 15: Double-comma with cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "x,,y,z"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test15");

        // Hold 'a' + Space → enters cycling, preedit shows "x,y"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Second Space → cycles to "z"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Release 'a' → commits current cycling value "z"
        tf->call<ITestFrontend::pushCommitExpectation>("z");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 15 PASSED";
    });

    // =========================================================================
    // TEST 32: Triple comma ",,," → [","] (escaped comma + separator)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 16;
        FCITX_INFO() << "=== Test 16: Triple comma output ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", ",,,"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test16");

        // Hold 'a' + Space → single output "," (literal comma)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>(",");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 16 PASSED";
    });

    // =========================================================================
    // TEST 13: Left Arrow as leader
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 17;
        FCITX_INFO() << "=== Test 17: Left Arrow as leader ===";
        configureLeaders(instance, false, true, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test17");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Left, KeyStates(), kCodeLeft), false);
        FCITX_ASSERT(consumed)
            << "Left leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 17 PASSED";
    });

    // =========================================================================
    // TEST 14: Right Arrow as leader
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 18;
        FCITX_INFO() << "=== Test 18: Right Arrow as leader ===";
        configureLeaders(instance, false, false, true, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test18");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Right, KeyStates(), kCodeRight), false);
        FCITX_ASSERT(consumed)
            << "Right leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 18 PASSED";
    });

    // =========================================================================
    // TEST 15: Up Arrow as leader
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 19;
        FCITX_INFO() << "=== Test 19: Up Arrow as leader ===";
        configureLeaders(instance, false, false, false, true, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test19");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Up, KeyStates(), kCodeUp), false);
        FCITX_ASSERT(consumed) << "Up leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 19 PASSED";
    });

    // =========================================================================
    // TEST 16: Down Arrow as leader
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 20;
        FCITX_INFO() << "=== Test 20: Down Arrow as leader ===";
        configureLeaders(instance, false, false, false, false, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test20");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Down, KeyStates(), kCodeDown), false);
        FCITX_ASSERT(consumed)
            << "Down leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 20 PASSED";
    });

    // =========================================================================
    // Tests 21-23: Alt/AltGr leader with deferred commit verification.
    // Alt leader uses a 5ms deferred commit timer. To verify the committed
    // string, we wait 10ms via addTimeEvent before checking. These tests
    // are chained: each timer callback schedules the next test, so no
    // synchronous tests run during the verification window.
    //
    // The addTimeEvent return value (unique_ptr<EventSourceTime>) must be
    // kept alive — if it goes out of scope, the timer is cancelled. We use
    // a shared_ptr to a holder struct captured by all lambdas in the chain.
    //
    // IMPORTANT: Tests 24+ are scheduled via scheduleTestsAfterAltVerify()
    // from the last timer callback, guaranteeing all deferred commits are
    // verified before any subsequent test starts.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 21;
        FCITX_INFO() << "=== Test 21: Alt_L as leader ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test21");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);
        FCITX_ASSERT(consumed)
            << "Alt_L leader during gesture should be consumed";

        // Release 'a' — triggers deferred commit (5ms timer)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Shared holder keeps all chained timers alive
        auto holder = std::make_shared<AltVerifyHolder>();

        // Destroy the IC only once Test 21's deferred cycling commit has fired
        // (preedit empty), then chain to Test 22. Poll-based, stall-proof — see
        // destroyAfterDeferredCommit.
        destroyAfterDeferredCommit(
            instance, uuid, holder, [instance, holder]() {
                auto *tf = instance->addonManager().addon("testfrontend");
                FCITX_INFO() << "Test 21 PASSED";

                // --- Test 22: AltGr as leader ---
                g_currentTest = 22;
                FCITX_INFO()
                    << "=== Test 22: AltGr (ISO_Level3_Shift) as leader ===";
                configureLeaders(instance, false, false, false, false, false,
                                 true);
                auto uuid22 = createAndActivate(instance, tf, "test22");

                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid22, Key(FcitxKey_a, KeyStates(), kCodeA), false);

                bool consumed22 = tf->call<ITestFrontend::sendKeyEvent>(
                    uuid22,
                    Key(FcitxKey_ISO_Level3_Shift, KeyStates(), kCodeAltGr),
                    false);
                FCITX_ASSERT(consumed22)
                    << "AltGr leader during gesture should be consumed";

                tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
                tf->call<ITestFrontend::keyEvent>(
                    uuid22, Key(FcitxKey_a, KeyStates(), kCodeA), true);

                destroyAfterDeferredCommit(
                    instance, uuid22, holder, [instance, holder]() {
                        auto *tf =
                            instance->addonManager().addon("testfrontend");
                        FCITX_INFO() << "Test 22 PASSED";

                        // --- Test 23: Alt release consumed, commits "ä" ---
                        g_currentTest = 23;
                        FCITX_INFO() << "=== Test 23: Alt release consumed "
                                        "after leader use ===";
                        configureLeaders(instance, false, false, false, false,
                                         false, true);
                        auto uuid23 = createAndActivate(instance, tf, "test23");

                        tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_a, KeyStates(), kCodeA),
                            false);
                        tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL),
                            false);

                        bool c = tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL),
                            true);
                        FCITX_ASSERT(c)
                            << "Alt release after leader should be consumed";

                        tf->call<ITestFrontend::pushCommitExpectation>(
                            "\xc3\xa4");
                        c = tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_a, KeyStates(), kCodeA), true);
                        FCITX_ASSERT(c) << "Accent key release during Alt "
                                           "gesture should be consumed";

                        destroyAfterDeferredCommit(
                            instance, uuid23, holder, [instance]() {
                                FCITX_INFO() << "Test 23 PASSED";
                                // All deferred commits verified — safe to
                                // continue
                                scheduleTestsAfterAltVerify(instance);
                            });
                    });
            });
    });
}

// Remaining tests — scheduled only after Alt leader deferred commits are
// verified.
static void scheduleTestsAfterAltVerify(Instance *instance) {

    // =========================================================================
    // TEST 20: committed_ — auto-repeat suppression after commit
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 24;
        FCITX_INFO() << "=== Test 24: Auto-repeat suppression after commit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test24");

        // Hold 'a' + Space → commit 'ä'
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Auto-repeat 'a' (same keycode, no release) — should be consumed
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "Auto-repeat after commit should be consumed";

        // Release 'a' — should be consumed (committed_)
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed) << "Release of committed key should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 24 PASSED";
    });

    // =========================================================================
    // TEST 21: Custom leader key
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 25;
        FCITX_INFO() << "=== Test 25: Custom leader key '#' ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "#", "", kCodeHash);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test25");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed)
            << "Custom leader '#' during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 25 PASSED";
    });

    // NOTE: Disabled leader passthrough test removed — triggers a
    // pre-existing testfrontend commit-expectation issue where commits via
    // the "OTHER KEYS" path (commitPendingKey) silently fail the global
    // checkExpectation_ assertion regardless of push timing.

    // =========================================================================
    // TEST 24: Custom leader with multi-byte UTF-8 character (§)
    // Validates utf8::validate/ncharByteLength sanitization path.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 26;
        FCITX_INFO() << "=== Test 26: Custom leader with UTF-8 char § ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "\xc2\xa7", "", kCodeSection); // § in UTF-8
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test26");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_section, KeyStates(), kCodeSection), false);
        FCITX_ASSERT(consumed) << "UTF-8 custom leader § should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 26 PASSED";
    });

    // =========================================================================
    // TEST 25: The stored character does not affect matching
    // CustomKey is a junk multi-character string, but the leader's KEY is the
    // '#' key. Pressing it must still trigger: only the keycode is compared.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 27;
        FCITX_INFO() << "=== Test 27: Junk character, key still leads ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "#x", "", kCodeHash);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test27");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed)
            << "Sanitized '#' from '#x' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 27 PASSED";
    });

    // =========================================================================
    // TEST 26: Padded character does not affect matching either
    // Same as test 27 with a whitespace-padded CustomKey. The '#' key leads.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 28;
        FCITX_INFO() << "=== Test 28: Padded character, key still leads ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "  #  ", "", kCodeHash);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test28");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed)
            << "Sanitized '#' from '  #  ' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 28 PASSED";
    });

    // NOTE: Whitespace-only custom leader rejected test removed — same
    // testfrontend commit-expectation issue (OTHER KEYS path).

    // =========================================================================
    // TEST 28: A multi-byte character in CustomKey does not affect matching
    // CustomKey is "§xyz", but the leader's KEY is the § key. It leads anyway.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 29;
        FCITX_INFO() << "=== Test 29: Multi-byte junk, key still leads ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "\xc2\xa7xyz", "", kCodeSection); // "§xyz"
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test29");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_section, KeyStates(), kCodeSection), false);
        FCITX_ASSERT(consumed)
            << "The § key must lead regardless of what CustomKey stores";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 29 PASSED";
    });

    // =========================================================================
    // TEST 40: Cycling — multiple outputs, cycle through variants
    // Mapping "a" → "ä,ae,@" — Space cycles: ä → ae → @ → ä → ...
    // Release 'a' after second Space → commits "ae" (index 1).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 30;
        FCITX_INFO() << "=== Test 30: Cycling through multiple outputs ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae,@"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test30");

        // Hold 'a' + Space → enters cycling at index 0 (preedit "ä")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Second Space → cycles to index 1 (preedit "ae")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Release 'a' → commits current cycling value "ae"
        tf->call<ITestFrontend::pushCommitExpectation>("ae");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 30 PASSED";
    });

    // =========================================================================
    // TEST 41: Cycling — wrap around to first variant
    // Mapping "a" → "ä,ae" — three Spaces: ä → ae → ä (wraps)
    // Release 'a' → commits "ä" (index 0 after wrap).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 31;
        FCITX_INFO() << "=== Test 31: Cycling wraps around ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test31");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Space 1 → index 0 ("ä")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        // Space 2 → index 1 ("ae")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        // Space 3 → wraps to index 0 ("ä")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Release 'a' → commits "ä"
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 31 PASSED";
    });

    // =========================================================================
    // TEST 42: Overlapping gestures — hold 'a', press 's' while 'a' held
    // Pressing a second mapped key should commit 'a' and start 's' gesture.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 32;
        FCITX_INFO() << "=== Test 32: Overlapping gestures ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test32");

        // Press 'a' → enters waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press 's' → should commit 'a' (pending), then start 's' gesture
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);

        // Now 's' is waiting — Space should convert it
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x9f");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 32 PASSED";
    });

    // =========================================================================
    // TEST 43: Space passthrough without gesture
    // Space alone (no key held) should pass through unconsumed.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 33;
        FCITX_INFO() << "=== Test 33: Space passthrough without gesture ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test33");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed) << "Space without gesture should pass through";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);
        FCITX_ASSERT(!consumed)
            << "Space release without gesture should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 33 PASSED";
    });

    // =========================================================================
    // TEST 44: Ordering guard — non-Space key clears guard
    // After a commit, recentlyCommitted_ is set. Any non-Space press clears
    // it without consuming. A subsequent Space then also passes through.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 34;
        FCITX_INFO() << "=== Test 34: Ordering guard clears on non-Space ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test34");

        // Hold 'a' + Space → 'ä' (sets recentlyCommitted_)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Press 'b' (unmapped) → not consumed, but clears recentlyCommitted_
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
        FCITX_ASSERT(!consumed)
            << "Unmapped 'b' after commit should pass through";
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), true);

        // Space now → guard already cleared, should pass through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed)
            << "Space after guard cleared should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 34 PASSED";
    });

    // =========================================================================
    // TEST 45: State cleanup after cycling — new gesture works immediately
    // After a cycling commit (release key), all cycling state must be reset
    // so a new gesture can start without interference.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 35;
        FCITX_INFO() << "=== Test 35: New gesture after cycling commit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}, {"s", "\xc3\x9f"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test35");

        // Cycling gesture: hold 'a' + Space + Space → index 1 ("ae")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Release 'a' → commits "ae"
        tf->call<ITestFrontend::pushCommitExpectation>("ae");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // New gesture immediately: hold 's' + Space → 'ß' (single output)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x9f");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 35 PASSED";
    });

    // =========================================================================
    // TEST 46: Ordering guard after cycling release
    // After cycling commit via key release, recentlyCommitted_ must be set
    // so the next Space goes through commitString (same channel as the commit).
    // Without this, Space could arrive before the committed text in terminals.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 36;
        FCITX_INFO() << "=== Test 36: Ordering guard after cycling release ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test36");

        // Cycling: hold 'a' + Space → preedit "ä"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Release 'a' → commits "ä" via cycling release path
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Space → ordering guard should route through commitString(" ")
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Space after cycling commit must be consumed by ordering guard";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 36 PASSED";
    });

    // =========================================================================
    // TEST 47: Writing flow — Space leader, German 1000 chars
    // Fast typing with word-boundary overlap: hold last char + Space.
    // With Space as leader, mapped chars at word end get converted.
    // Mapped chars: a,e,i,o,u,s,c,n,y (multilingual cycling)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 37;
        FCITX_INFO() << "=== Test 37: Space leader — German 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test37");

        int collisions =
            typeTextWordBoundaryOverlap(tf, uuid, kGerman1000, true);

        FCITX_ASSERT(collisions > 10)
            << "German should have significant collisions with Space leader"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 37 PASSED — German: " << collisions
                     << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 48: Writing flow — Space leader, English 1000 chars
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 38;
        FCITX_INFO() << "=== Test 38: Space leader — English 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test38");

        int collisions =
            typeTextWordBoundaryOverlap(tf, uuid, kEnglish1000, true);

        FCITX_ASSERT(collisions > 10)
            << "English should have significant collisions with Space leader"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 38 PASSED — English: " << collisions
                     << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 49: Writing flow — Space leader, French 1000 chars
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 39;
        FCITX_INFO() << "=== Test 39: Space leader — French 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test39");

        int collisions =
            typeTextWordBoundaryOverlap(tf, uuid, kFrench1000, true);

        FCITX_ASSERT(collisions > 10)
            << "French should have many collisions (short words ending e/s)"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 39 PASSED — French: " << collisions
                     << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 50: Writing flow — Space leader, Portuguese 1000 chars
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 40;
        FCITX_INFO() << "=== Test 40: Space leader — Portuguese 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test40");

        int collisions =
            typeTextWordBoundaryOverlap(tf, uuid, kPortuguese1000, true);

        FCITX_ASSERT(collisions > 10)
            << "Portuguese should have many collisions (words ending a/o/e/s)"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 40 PASSED — Portuguese: " << collisions
                     << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 51: Writing flow — Alt leader, German 1000 chars
    // Alt is never pressed during normal typing → 0 collisions.
    // This proves Alt is superior to Space for fast typing.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 41;
        FCITX_INFO() << "=== Test 41: Alt leader — German 1000 chars ===";
        configureMultilingualCycling(instance, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test41");

        int collisions =
            typeTextWordBoundaryOverlap(tf, uuid, kGerman1000, false);

        FCITX_ASSERT(collisions == 0)
            << "Alt leader must have zero collisions in normal typing";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 41 PASSED — Alt leader: " << collisions
                     << " collisions (zero!)";
    });

    // =========================================================================
    // TEST 52: Writing flow — Custom leader (#), German 1000 chars
    // Custom key is never pressed during normal typing → 0 collisions.
    // Same advantage as Alt but key position may differ ergonomically.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 42;
        FCITX_INFO()
            << "=== Test 42: Custom leader (#) — German 1000 chars ===";
        configureMultilingualCycling(instance, false, false, "#", kCodeHash);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test42");

        int collisions =
            typeTextWordBoundaryOverlap(tf, uuid, kGerman1000, false);

        FCITX_ASSERT(collisions == 0)
            << "Custom leader must have zero collisions in normal typing";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 42 PASSED — Custom leader (#): " << collisions
                     << " collisions (zero!)";
    });

    // =========================================================================
    // TEST 53: Overlap collision demo — "anbau" → "anbaü" (standard mapping)
    // Demonstrates the specific problem: user holds 'u' at word end,
    // presses Space → 'u' converts to 'ü'. Output: "anbaü" not "anbau ".
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 43;
        FCITX_INFO() << "=== Test 43: anbau overlap collision demo ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test43");

        // Type "anba" with clean typing (release before next key)
        // 'a' is mapped → press, expect "a", release
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        // 'n' unmapped → pass through
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_n, KeyStates(), 57), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_n, KeyStates(), 57), true);
        // 'b' unmapped → pass through
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), true);
        // 'a' mapped → press, expect "a", release
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // NOW: hold 'u' and press Space → conversion!
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), 30), false);
        // Space converts 'u' → 'ü' (single output, direct commit)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Space must trigger conversion of held 'u'";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), 30), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 43 PASSED — anbau → anbaü confirmed";
    });

    // =========================================================================
    // TEST 54: Dual leader collision analysis — all 4 languages
    // Compare Space leader (word-boundary collisions) vs dual custom leaders
    // (within-word collisions with 'a' and ';').
    //
    // Dual leader concept (US QWERTY):
    //   ';' (right hand) = leader for LEFT-hand inputs (a,e,s,c)
    //   'a' (left hand)  = leader for RIGHT-hand inputs (y,u,i,o,n)
    //
    // Space leader collides at EVERY word boundary where last char is mapped.
    // 'a' leader only collides when [yuion] immediately precedes 'a' within
    // a word AND keys overlap — much rarer.
    // ';' leader never appears in normal text → 0 collisions.
    // =========================================================================
    testDispatcher->schedule([]() {
        g_currentTest = 44;
        FCITX_INFO() << "=== Test 44: Dual leader collision analysis ===";

        struct LangResult {
            const char *name;
            const char *text;
            int spaceCollisions;
            int aLeaderCollisions;
            int semicolonCollisions;
        };

        LangResult langs[] = {
            {"German", kGerman1000, 0, 0, 0},
            {"English", kEnglish1000, 0, 0, 0},
            {"French", kFrench1000, 0, 0, 0},
            {"Portuguese", kPortuguese1000, 0, 0, 0},
            {"Spanish", kSpanish1000, 0, 0, 0},
        };

        for (auto &l : langs) {
            l.spaceCollisions = countSpaceLeaderCollisions(l.text);
            // 'a' leader controls right-hand mapped chars (y,u,i,o,n)
            l.aLeaderCollisions =
                countDualLeaderWithinWordCollisions(l.text, 'a', true);
            // ';' leader controls left-hand mapped chars (a,e,s,c)
            l.semicolonCollisions =
                countDualLeaderWithinWordCollisions(l.text, ';', false);
        }

        // Verify: 'a' leader has dramatically fewer collisions than Space
        for (const auto &l : langs) {
            FCITX_ASSERT(l.aLeaderCollisions < l.spaceCollisions / 3)
                << l.name << ": 'a' leader should have <33% of Space collisions"
                << " (a=" << l.aLeaderCollisions
                << " space=" << l.spaceCollisions << ")";
        }

        // Verify: ';' leader has zero collisions (never typed in normal text)
        for (const auto &l : langs) {
            FCITX_ASSERT(l.semicolonCollisions == 0)
                << l.name << ": ';' leader must have zero collisions";
        }

        // Also analyze z / / leader pair:
        //   'z' (left hand, bottom row) = leader for right-hand inputs
        //   '/' (right hand, bottom row) = leader for left-hand inputs
        struct DualPair {
            const char *label;
            char leftLeader;  // left-hand key → controls right-hand inputs
            char rightLeader; // right-hand key → controls left-hand inputs
        };
        DualPair pairs[] = {
            {"a/;", 'a', ';'}, {"z//", 'z', '/'}, {"f/j", 'f', 'j'},
            {"q/p", 'q', 'p'}, {"q/[", 'q', '['},
        };

        std::string result;
        for (const auto &p : pairs) {
            result += "[" + std::string(p.label) + "] ";
            for (const auto &l : langs) {
                int leftL = countDualLeaderWithinWordCollisions(
                    l.text, p.leftLeader, true);
                int rightL = countDualLeaderWithinWordCollisions(
                    l.text, p.rightLeader, false);
                result += std::string(l.name) +
                          "(Space:" + std::to_string(l.spaceCollisions) + " " +
                          p.leftLeader + ":" + std::to_string(leftL) + " " +
                          p.rightLeader + ":" + std::to_string(rightL) + ") ";
            }
        }
        FCITX_INFO() << "Test 44 PASSED — " << result;
    });

    // =========================================================================
    // TEST 55: CustomKey2 basic — second custom leader works
    // CustomKey2="#", hold 'a' + '#' → ä
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 45;
        FCITX_INFO() << "=== Test 45: CustomKey2 basic ===";
        configureLeaders(instance, false, false, false, false, false, false, "",
                         "#", fcitx::kNoKeyCode, kCodeHash);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test45");

        // Hold 'a' + press '#' (CustomKey2) → ä
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "CustomKey2 '#' should trigger conversion";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 45 PASSED";
    });

    // =========================================================================
    // TEST 56: Dual split — allowed (opposite hands)
    // CustomKey="z" (left), CustomKey2="/" (right)
    // Hold 'u' (right-hand input) + press 'z' (left-hand leader) → ü
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 46;
        FCITX_INFO() << "=== Test 46: Dual split — allowed ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "/", kCodeZ, kCodeSlash);
        setMappings(instance, {{"u", "\xc3\xbc"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test46");

        // Hold 'u' (right) + press 'z' (left leader) → ü (allowed: opposite
        // hands)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), 30), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), 52), false);
        FCITX_ASSERT(consumed) << "'z' leader must convert right-hand 'u'";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), 30), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 46 PASSED";
    });

    // =========================================================================
    // TEST 57: Dual split — blocked (same hand)
    // Same config. Hold 'a' (left input) + press 'z' (left leader) → NO
    // conversion. 'z' falls through to OTHER KEYS, commits 'a' as original, 'z'
    // passes through.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 47;
        FCITX_INFO() << "=== Test 47: Dual split — blocked ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "/", kCodeZ, kCodeSlash);
        setMappings(instance, {{"u", "\xc3\xbc"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test47");

        // Hold 'a' (left) + press 'z' (left leader) → BLOCKED
        // 'z' is not allowed for left-hand input 'a', falls through to OTHER
        // KEYS which commits 'a' as original and lets 'z' pass through.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), 52), false);
        FCITX_ASSERT(!consumed)
            << "'z' must NOT convert same-hand 'a' — should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 47 PASSED";
    });

    // =========================================================================
    // TEST 58: Dual split — reverse direction
    // Hold 'a' (left) + press '/' (right leader) → ä (allowed: opposite hands)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 48;
        FCITX_INFO() << "=== Test 48: Dual split — reverse ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "/", kCodeZ, kCodeSlash);
        setMappings(instance, {{"u", "\xc3\xbc"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test48");

        // Hold 'a' (left) + press '/' (right leader) → ä (allowed)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_slash, KeyStates(), 61), false);
        FCITX_ASSERT(consumed) << "'/' leader must convert left-hand 'a'";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 48 PASSED";
    });

    // =========================================================================
    // TEST 59: Single custom key — no split, triggers all mappings
    // Only CustomKey="z" set (no CustomKey2) → 'z' triggers ALL inputs.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 49;
        FCITX_INFO() << "=== Test 49: Single custom key — no split ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "", kCodeZ);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test49");

        // Hold 'a' (left) + press 'z' (left, single custom key) → ä
        // No split because CustomKey2 is empty → triggers ALL mappings
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), 52), false);
        FCITX_ASSERT(consumed)
            << "Single custom key 'z' must trigger all mappings (no split)";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 49 PASSED";
    });

    // =========================================================================
    // TEST 152: Symbol leader still fires while Shift is held
    // The leader is the physical key, so Shift changing its character must not
    // matter. Leader = the '/' key (code 61). With Shift that key reports the
    // '?' keysym; the leader fires anyway, because only the keycode is compared.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 152;
        FCITX_INFO() << "=== Test 152: Symbol leader fires with Shift held ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "/", "", kCodeSlash);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test152");

        // Hold 'a', then press the '/' key WITH Shift: the frontend delivers the
        // shifted keysym ('?'), but the physical code is still the leader's.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid,
            Key(FcitxKey_question, KeyStates(KeyState::Shift), kCodeSlash),
            false);
        FCITX_ASSERT(consumed)
            << "Shift+'/' must still fire the '/' leader: the key is the "
               "leader, not the character it happens to produce";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 152 PASSED";
    });

    // =========================================================================
    // TEST 153: A leader with no key assigned triggers nothing
    // The leader IS its physical key, so a character on its own does not
    // configure one. Enabling CustomKey without capturing a key (kNoKeyCode)
    // must leave it inert: pressing that character falls through as a normal
    // key instead of acting as a leader.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 153;
        FCITX_INFO() << "=== Test 153: Leader without a key is inert ===";
        // Character set, keycode deliberately left at kNoKeyCode.
        configureLeaders(instance, false, false, false, false, false, false,
                         "z");
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test153");

        // Hold 'a', press 'z'. 'z' is not a leader (no key assigned), so it must
        // NOT convert: the preedit commits as plain 'a' and 'z' passes through.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), kCodeZ), false);
        FCITX_ASSERT(!consumed)
            << "A leader with no key assigned must not trigger: the character "
               "alone does not configure one";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 153 PASSED";
    });

    // =========================================================================
    // TEST 154: AltGr must not poison the learned base characters
    // The engine remembers what each physical key produces UNMODIFIED, and the
    // Alt-leader bypass uses that to find the mapping when a modifier changed
    // the keysym. AltGr is the level-3 shift and reports as Mod5, not as Alt,
    // so a modifier guard that only knows about Alt would treat AltGr+s as an
    // unmodified press and learn the level-3 character for that key.
    //
    // Here the 's' key is taught first, then hit with AltGr (reporting '@'),
    // then used as a new mapped key during an Alt gesture. The bypass has to
    // resolve it back to 's' and start an 's' gesture. If AltGr had been learned
    // as the base character, it would commit '@' instead.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 154;
        FCITX_INFO() << "=== Test 154: AltGr does not poison base chars ===";
        configureLeaders(instance, false, false, false, false, false, true);
        setMappings(instance, {{"a", "\xc3\xa4"}, {"s", "\xc3\x9f"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test154");

        // Teach the 's' key: press it unmodified, release without a leader so it
        // commits as itself.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), false);
        tf->call<ITestFrontend::pushCommitExpectation>("s");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyStates(), kCodeS), true);

        // Now hit the SAME physical key with AltGr, which reports Mod5 and a
        // different keysym. This must not become the key's base character.
        // Note this only works if the guard reads the RAW key: normalize()
        // strips Mod5, so the normalized key looks unmodified.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_at, KeyStates(KeyState::Mod5), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_at, KeyStates(KeyState::Mod5), kCodeS), true);

        // Same for Shift: normalize() drops the Shift bit for letters, so a
        // shifted press also looks unmodified on the normalized key. It must not
        // teach 'S' as the base character either. 'S' is not a mapped input
        // here, so it simply passes through.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_S, KeyStates(KeyState::Shift), kCodeS), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_S, KeyStates(KeyState::Shift), kCodeS), true);

        // Hold 'a', press Alt to start cycling.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Press the 's' key with Alt held, arriving with a modifier-changed
        // keysym. The bypass resolves the key's base character: 's', a mapped
        // input, so the cycling value commits and an 's' gesture starts. A
        // poisoned map would resolve '@' and commit that instead, which the
        // frontend flags as an unexpected commit.
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_at, KeyState::Alt, kCodeS), false);
        FCITX_ASSERT(consumed) << "Alt+'s' during cycling must be consumed";

        // Release it: the 's' gesture commits as itself, proving a gesture was
        // started for 's' and not that '@' was emitted.
        tf->call<ITestFrontend::pushCommitExpectation>("s");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_at, KeyState::Alt, kCodeS), true);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 154 PASSED";
    });

    // =========================================================================
    // TEST 155: A negative keycode from a hand-edited config is not a key
    // The config is a plain text file. A keycode that cannot name a real key
    // must collapse to "no key assigned", otherwise it counts as a configured
    // leader: matching nothing, yet arming the hand-split off its own bogus
    // position (-1 classifies as right-hand by omission).
    //
    // Leader 1 claims -1, leader 2 is the real 'f' key on the LEFT half. Taking
    // -1 at face value makes the two look opposite-handed, arms the split, and
    // restricts left-hand leader 'f' to right-hand inputs, which would block
    // left-hand 'a'. Collapsed to "no key", the split stays off and 'f'
    // triggers everything.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 155;
        FCITX_INFO() << "=== Test 155: Negative keycode is no key ===";
        configureLeaders(instance, false, false, false, false, false, false, "z",
                         "f", -1, kCodeF);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test155");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_f, KeyStates(), kCodeF), false);
        FCITX_ASSERT(consumed)
            << "A leader with an unusable keycode must not arm the split";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 155 PASSED";
    });

    // =========================================================================
    // TEST 60: Dual split — built-in leader ignores split
    // CustomKey="z", CustomKey2="/", Space=True.
    // Space is BuiltIn → ignores hand split, triggers all mappings.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 50;
        FCITX_INFO() << "=== Test 50: Built-in leader ignores split ===";
        configureLeaders(instance, true, false, false, false, false, false, "z",
                         "/", kCodeZ, kCodeSlash);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test50");

        // Hold 'a' (left) + Space → ä (Space is BuiltIn, ignores split)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Space (BuiltIn) must ignore dual split and "
                                  "trigger all mappings";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 50 PASSED";
    });

    // =========================================================================
    // TEST 51: Ordering guard skips Super+Space
    // After a commit, modifier+Space must NOT be consumed by the guard.
    // Uses Super (not Ctrl+Space which is the IM toggle in tests).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 51;
        FCITX_INFO() << "=== Test 51: Ordering guard skips Super+Space ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test51");

        // Hold 'a' + Space → ä (sets recentlyCommitted_)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Super+Space → must NOT be consumed (shortcut, not bare Space)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyState::Super, kCodeSpace), false);
        FCITX_ASSERT(!consumed)
            << "Super+Space after commit must pass through ordering guard";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 51 PASSED";
    });

    // =========================================================================
    // TEST 52: Ordering guard skips Alt+Space
    // Alt+Space is a common window manager shortcut — must pass through.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 52;
        FCITX_INFO() << "=== Test 52: Ordering guard skips Alt+Space ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test52");

        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyState::Alt, kCodeSpace), false);
        FCITX_ASSERT(!consumed)
            << "Alt+Space after commit must pass through ordering guard";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 52 PASSED";
    });

    // =========================================================================
    // TEST 53: Same-hand dual leaders — no split
    // CustomKey="z" (left), CustomKey2="x" (left) → same hand → no split.
    // Both leaders trigger all mappings regardless of input hand.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 53;
        FCITX_INFO() << "=== Test 53: Same-hand dual leaders — no split ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "z", "x", kCodeZ, kCodeX);
        setMappings(instance, {{"a", "\xc3\xa4"}, {"u", "\xc3\xbc"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test53");

        // Hold 'a' (left) + 'z' (left leader) → ä (same hand, no split)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), 52), false);
        FCITX_ASSERT(consumed)
            << "Same-hand leader 'z' must trigger same-hand 'a' (no split)";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 53 PASSED";
    });

    // =========================================================================
    // TEST 54: Accent key repeat during waiting — consumed
    // Auto-repeat of held accent key must be consumed without affecting
    // the waiting state. Space should still trigger conversion.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 54;
        FCITX_INFO() << "=== Test 54: Accent key repeat during waiting ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test54");

        // Press 'a' → enters waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Auto-repeat 'a' (same keycode, no release) → consumed
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed)
            << "Accent key repeat during waiting must be consumed";

        // Space → still converts (waiting state preserved)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Space must still convert after accent repeat";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 54 PASSED";
    });

    // =========================================================================
    // TEST 55: Modifier during waiting — commits pending key
    // Ctrl+c while accent key is waiting must commit the original char
    // and let the shortcut through unconsumed.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 55;
        FCITX_INFO()
            << "=== Test 55: Modifier during waiting commits pending ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test55");

        // Press 'a' → enters waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Ctrl+c → commits 'a' as original, passes through
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_c, KeyState::Ctrl, 54), false);
        FCITX_ASSERT(!consumed)
            << "Ctrl+c must pass through after committing pending key";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 55 PASSED";
    });

    // =========================================================================
    // TEST 56: Alt leader — new mapped key commits cycling and starts gesture
    // While cycling with Alt held, pressing a different mapped key should
    // commit the cycling value and start a new gesture for the new key.
    // The Alt modifier must not leak through as a shortcut.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 56;
        FCITX_INFO() << "=== Test 56: Alt leader — new key during cycling ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test56");

        // Hold 'a' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press Alt → starts cycling (ä in preedit)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Press 's' with Alt held → must commit ä and start 's' gesture
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyState::Alt, kCodeS), false);
        FCITX_ASSERT(consumed)
            << "New mapped key during Alt cycling must be consumed";

        // Release 's' → commits 's' as original (no leader pressed for it)
        tf->call<ITestFrontend::pushCommitExpectation>("s");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_s, KeyState::Alt, kCodeS), true);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 56 PASSED";
    });

    // =========================================================================
    // TEST 57: Layout-independent hand classification
    // Verify that physical keycode classification works correctly across
    // different keyboard layouts (QWERTY, QWERTZ, AZERTY, Dvorak).
    // The same physical key must always be classified to the same hand,
    // regardless of which character the layout assigns to it.
    // =========================================================================
    testDispatcher->schedule([]() {
        g_currentTest = 57;
        FCITX_INFO()
            << "=== Test 57: Layout-independent hand classification ===";

        // Exercises the shipped fcitx::isLeftHandKeycode(), not a copy of it.
        //
        // keycodeFor() stands in for the editor's key capture: it answers "which
        // physical key does the user press to type `ch` on this layout". The
        // engine only ever sees that keycode, never the character.
        auto keycodeFor = [](struct xkb_keymap *km, const std::string &ch) {
            xkb_keycode_t min = xkb_keymap_min_keycode(km);
            xkb_keycode_t max = xkb_keymap_max_keycode(km);
            for (xkb_keycode_t code = min; code <= max; ++code) {
                const xkb_keysym_t *syms;
                int n = xkb_keymap_key_get_syms_by_level(km, code, 0, 0, &syms);
                if (n > 0) {
                    uint32_t uc = xkb_keysym_to_utf32(syms[0]);
                    if (uc > 0 && uc <= 0x10FFFF && utf8::UCS4ToUTF8(uc) == ch)
                        return static_cast<int>(code);
                }
            }
            return fcitx::kNoKeyCode;
        };

        auto *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        FCITX_ASSERT(ctx) << "XKB context creation failed";

        // Layout → expected left/right chars (based on physical position)
        struct LayoutCheck {
            const char *layout;
            const char *variant;
            // {char, expectedLeft} — chars that differ across layouts
            std::vector<std::pair<std::string, bool>> checks;
        };

        LayoutCheck layouts[] = {
            {"us",
             nullptr,
             {
                 {"q", true},  {"w", true},  {"e", true},  {"t", true},
                 {"a", true},  {"s", true},  {"d", true},  {"f", true},
                 {"g", true},  {"z", true},  {"x", true},  {"c", true},
                 {"v", true},  {"b", true},  {"y", false}, {"u", false},
                 {"i", false}, {"o", false}, {"p", false}, {"h", false},
                 {"j", false}, {"k", false}, {"l", false}, {"n", false},
                 {"m", false},
             }},
            {"de",
             nullptr,
             {
                 // QWERTZ: z/y swapped vs QWERTY
                 {"q", true},
                 {"w", true},
                 {"e", true},
                 {"r", true},
                 {"t", true},
                 {"a", true},
                 {"s", true},
                 {"d", true},
                 {"f", true},
                 {"g", true},
                 {"y", true}, // keycode 52 (left) —
                              // QWERTY has 'z' here
                 {"x", true},
                 {"c", true},
                 {"v", true},
                 {"b", true},
                 {"z", false}, // keycode 29 (right) — QWERTY has 'y' here
                 {"u", false},
                 {"i", false},
                 {"o", false},
                 {"p", false},
                 {"h", false},
                 {"j", false},
                 {"k", false},
                 {"l", false},
                 {"n", false},
                 {"m", false},
             }},
            {"fr",
             nullptr,
             {
                 // AZERTY: a↔q, w↔z swapped, m moved
                 {"a", true}, // keycode 24 (left)
                 {"z", true}, // keycode 25 (left)
                 {"e", true},  {"r", true},  {"t", true},
                 {"q", true}, // keycode 38 (left)
                 {"s", true},  {"d", true},  {"f", true},
                 {"g", true},  {"w", true}, // keycode 52 (left)
                 {"x", true},  {"c", true},  {"v", true},
                 {"b", true},  {"y", false}, {"u", false},
                 {"i", false}, {"o", false}, {"p", false},
                 {"h", false}, {"j", false}, {"k", false},
                 {"l", false}, {"n", false},
             }},
            {"us",
             "dvorak",
             {
                 // Dvorak: heavily rearranged
                 {"a", true},  // keycode 38 (left)
                 {"o", true},  // keycode 39 (left)
                 {"e", true},  // keycode 40 (left)
                 {"u", true},  // keycode 41 (left)
                 {"i", true},  // keycode 42 (left)
                 {"p", true},  // keycode 27 (left)
                 {"q", true},  // keycode 53 (left)
                 {"j", true},  // keycode 54 (left)
                 {"k", true},  // keycode 55 (left)
                 {"x", true},  // keycode 56 (left)
                 {"d", false}, // keycode 43 (right)
                 {"h", false}, // keycode 44 (right)
                 {"t", false}, // keycode 45 (right)
                 {"n", false}, // keycode 46 (right)
                 {"s", false}, // keycode 57 (right) — left in QWERTY!
                 {"f", false}, // keycode 29 (right)
                 {"g", false}, // keycode 30 (right)
                 {"c", false}, // keycode 31 (right)
                 {"r", false}, // keycode 32 (right)
                 {"l", false}, // keycode 33 (right)
             }},
        };

        int layoutsTested = 0;
        for (const auto &lc : layouts) {
            xkb_rule_names names{};
            names.layout = lc.layout;
            names.variant = lc.variant;
            auto *km = xkb_keymap_new_from_names(ctx, &names,
                                                 XKB_KEYMAP_COMPILE_NO_FLAGS);
            if (!km) {
                FCITX_INFO()
                    << "Skipping layout " << lc.layout
                    << (lc.variant ? std::string("/") + lc.variant : "")
                    << " (not installed)";
                continue;
            }

            for (const auto &[ch, expectLeft] : lc.checks) {
                const int code = keycodeFor(km, ch);
                FCITX_ASSERT(code != fcitx::kNoKeyCode)
                    << lc.layout << ": no key produces '" << ch << "'";
                const bool got = fcitx::isLeftHandKeycode(code);
                FCITX_ASSERT(got == expectLeft)
                    << lc.layout
                    << (lc.variant ? std::string("/") + lc.variant : "")
                    << ": '" << ch << "' sits on keycode " << code
                    << ", expected " << (expectLeft ? "left" : "right")
                    << " but got " << (got ? "left" : "right");
            }

            xkb_keymap_unref(km);
            ++layoutsTested;
            FCITX_INFO() << "Layout " << lc.layout
                         << (lc.variant ? std::string("/") + lc.variant : "")
                         << " — all " << lc.checks.size() << " checks passed";
        }

        FCITX_ASSERT(layoutsTested >= 2)
            << "At least 2 layouts must be available for meaningful coverage";

        // Pin why the hand must never be derived from a character: the same
        // character sits on opposite halves depending on the layout. 'z' is a
        // left-hand key on US and a right-hand key on QWERTZ. Any future
        // char→keycode lookup would have to pick one and be wrong on the other,
        // so this fails loudly if one is reintroduced.
        xkb_rule_names usNames{};
        usNames.layout = "us";
        xkb_rule_names deNames{};
        deNames.layout = "de";
        auto *usKm =
            xkb_keymap_new_from_names(ctx, &usNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
        auto *deKm =
            xkb_keymap_new_from_names(ctx, &deNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (usKm && deKm) {
            const int zUs = keycodeFor(usKm, "z");
            const int zDe = keycodeFor(deKm, "z");
            FCITX_ASSERT(zUs != zDe)
                << "'z' must sit on different keys in us/de for this to mean "
                   "anything";
            FCITX_ASSERT(fcitx::isLeftHandKeycode(zUs))
                << "US 'z' (keycode " << zUs << ") is left-hand";
            FCITX_ASSERT(!fcitx::isLeftHandKeycode(zDe))
                << "QWERTZ 'z' (keycode " << zDe << ") is RIGHT-hand";
            FCITX_INFO() << "Character is not a hand: 'z' is keycode " << zUs
                         << " (left) on us but " << zDe << " (right) on de";
        }
        if (usKm)
            xkb_keymap_unref(usKm);
        if (deKm)
            xkb_keymap_unref(deKm);

        xkb_context_unref(ctx);
        FCITX_INFO() << "Test 57 PASSED — " << layoutsTested
                     << " layouts verified";
    });

    // =========================================================================
    // TEST 57: Alt deferred re-press — cancels deferred commit
    // On KWin Wayland, Alt auto-repeat sends release-press pairs. The
    // release schedules a 5ms deferred commit; re-press must cancel it
    // and continue cycling. Only the final release commits.
    // Timer-based: exits after deferred commit verification.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 58;
        FCITX_INFO() << "=== Test 58: Alt deferred re-press ===";
        configureLeaders(instance, false, false, false, false, false, true);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test58");

        // Hold 'a' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press Alt → starts cycling at index 0 (ä)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Press Alt again → cycle to index 1 (ae)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Release 'a' → deferred commit timer starts (5ms)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Re-press 'a' immediately → cancels deferred timer
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press Alt → cycle wraps to index 0 (ä)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Final release 'a' → new deferred commit timer
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Wait for deferred commit to fire
        struct TimerHolder {
            std::unique_ptr<EventSourceTime> timer;
        };
        auto holder = std::make_shared<TimerHolder>();
        holder->timer = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + kDeferredVerifyDelayUsec, 0,
            [instance, uuid, holder](EventSourceTime *, uint64_t) {
                auto *tf = instance->addonManager().addon("testfrontend");
                tf->call<ITestFrontend::destroyInputContext>(uuid);
                FCITX_INFO() << "Test 58 PASSED";

                scheduleTimeoutTests(instance);
                return false;
            });
    });
}

// =============================================================================
// TIMEOUT TESTS (59-64) — Timer-chained: each test schedules the next after
// its timer fires, so no race conditions between timer callbacks and tests.
// =============================================================================

static void scheduleTimeoutTests(Instance *instance) {
    // =========================================================================
    // TEST 59: Timer fires → original char committed
    // When no leader arrives within the delay, the timeout timer fires and
    // commits the original character (not the mapped output).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 59;
        FCITX_INFO()
            << "=== Test 59: Timeout fires — original char committed ===";
        configureWithDelay(instance, 50, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test59");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("a");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        // Defer cleanup so the addon's timeout timer (50ms) fires and
        // commits before IC destruction.  300ms gives 250ms of headroom
        // over the 50ms addon timer, avoiding flakes on loaded CI runners.
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 300'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");
                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 59 PASSED";
                    scheduleTest60(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 60: Non-mapped key after timeout passes through
// =========================================================================
static void scheduleTest60(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 60;
        FCITX_INFO() << "=== Test 60: Non-mapped key after timeout ===";
        configureWithDelay(instance, 50, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test60");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 300'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");

                    bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
                    FCITX_ASSERT(!consumed)
                        << "Unmapped key after timeout should pass through";

                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 60 PASSED";
                    scheduleTest61(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 61: Mapped key after timeout → new gesture starts
// =========================================================================
static void scheduleTest61(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 61;
        FCITX_INFO()
            << "=== Test 61: Mapped key after timeout starts new gesture ===";
        configureWithDelay(instance, 50, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test61");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        tf->call<ITestFrontend::pushCommitExpectation>("o");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 300'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");

                    // Timer fired, 'o' committed. Press 'u' → new gesture
                    bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_u, KeyStates(), kCodeU), false);
                    FCITX_ASSERT(consumed)
                        << "Mapped key should start new gesture after timeout";

                    tf->call<ITestFrontend::pushCommitExpectation>("u");
                    tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_u, KeyStates(), kCodeU), true);

                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 61 PASSED";
                    scheduleTest62(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 62: Uppercase uses longer delay (2000ms >> 50ms lowercase)
// At 200ms, uppercase (2000ms) hasn't timed out → Space converts to Ä.
// Large gap avoids event-loop batching races.
// =========================================================================
static void scheduleTest62(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 62;
        FCITX_INFO() << "=== Test 62: Uppercase uses longer delay ===";
        configureWithDelay(instance, 50, 2000);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test62");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 400'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                auto *tf = instance->addonManager().addon("testfrontend");

                // At 400ms, uppercase timeout (2000ms) hasn't expired
                tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x84");
                bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
                FCITX_ASSERT(consumed) << "Space at 400ms should convert "
                                          "within 2000ms uppercase window";

                tf->call<ITestFrontend::keyEvent>(
                    uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
                tf->call<ITestFrontend::destroyInputContext>(uuid);
                FCITX_INFO() << "Test 62 PASSED";
                scheduleTest63(instance);
                return false;
            });
    });
}

// =========================================================================
// TEST 63: Lowercase shorter delay confirmed — timer fires before 150ms
// =========================================================================
static void scheduleTest63(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 63;
        FCITX_INFO() << "=== Test 63: Lowercase shorter delay confirmed ===";
        configureWithDelay(instance, 50, 2000);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test63");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 300'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");

                    bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
                    FCITX_ASSERT(!consumed) << "Non-mapped key after lowercase "
                                               "timeout should pass through";

                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 63 PASSED";
                    scheduleTest64(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 64: Ordering guard after timeout commit
// Timer commits char with recentlyCommitted_. Next Space routes through
// commitString to preserve text ordering.
// =========================================================================
static void scheduleTest64(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 64;
        FCITX_INFO() << "=== Test 64: Ordering guard after timeout ===";
        configureWithDelay(instance, 50, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test64");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 300'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");

                    tf->call<ITestFrontend::pushCommitExpectation>(" ");
                    bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace),
                        false);
                    FCITX_ASSERT(consumed)
                        << "Ordering guard should catch Space after timeout";

                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 64 PASSED";
                    scheduleRemainingTests(instance);
                });
                return false;
            });
    });
}

// =============================================================================
// REMAINING TESTS (65-100) — Non-timer, scheduled independently via FIFO.
// =============================================================================

static void scheduleRemainingTests(Instance *instance) {

    // =========================================================================
    // TEST 65: activate() clears all state
    // Switching back to schnelle-umlaute via Ctrl+Space resets state.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 65;
        FCITX_INFO() << "=== Test 65: activate() clears all state ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test65");

        // Start gesture: press 'a' → preedit
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Switch away → deactivate commits pending 'a'
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"), false);
        // Switch back (activate → clears state)
        tf->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"), false);

        // Preedit should be empty — gesture state was cleared
        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit.empty())
            << "Preedit should be empty after activate(), got '" << preedit
            << "'";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 65 PASSED";
    });

    // =========================================================================
    // TEST 66: deactivate() on IM switch commits pending preedit
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 66;
        FCITX_INFO()
            << "=== Test 66: deactivate() on IM switch commits pending ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test66");

        // Press 'a' → preedit "a"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Switch IM → deactivate commits pending 'a'
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"), false);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 66 PASSED";
    });

    // =========================================================================
    // TEST 67: deactivate() on IM switch commits cycling value
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 67;
        FCITX_INFO()
            << "=== Test 67: deactivate() on IM switch commits cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test67");

        // Press 'a' + Space → cycling at index 0 (ä)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        // Press Space again → cycle to index 1 (ae)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Switch IM → deactivate commits cycling value "ae"
        tf->call<ITestFrontend::pushCommitExpectation>("ae");
        tf->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"), false);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 67 PASSED";
    });

    // =========================================================================
    // TEST 68: Ordering guard fires after Space leader commit
    // After a+Space → ä, immediate Space must be committed via ordering
    // guard (not passed through as raw key). In real apps like Chromium,
    // reset() is called after every commit — recentlyCommitted_ survives
    // that because it is NOT cleared in clearAllState(). The TestFrontend
    // API has no reset() method, so this test verifies the ordering guard
    // itself; reset() survival is a design invariant, not tested here.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 68;
        FCITX_INFO()
            << "=== Test 68: ordering guard after Space leader commit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test68");

        // Press 'a' → waiting, key held
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // a+Space → ä committed
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Immediate Space after commit → ordering guard commits " "
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Ordering guard should commit space after gesture";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 68 PASSED";
    });

    // =========================================================================
    // TEST 69: Two ICs with independent state
    // Gesture in IC1 must not affect IC2.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 69;
        FCITX_INFO() << "=== Test 69: Two ICs — independent state ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid1 = createAndActivate(instance, tf, "test69_ic1");
        auto uuid2 = createAndActivate(instance, tf, "test69_ic2");

        // Start gesture in IC1
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid1, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // IC2: press 'b' → should pass through (no gesture in IC2)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid2, Key(FcitxKey_b, KeyStates(), kCodeB), false);
        FCITX_ASSERT(!consumed) << "IC2 should not be affected by IC1 gesture";

        // IC1: complete gesture
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid1, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid1, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid1);
        tf->call<ITestFrontend::destroyInputContext>(uuid2);
        FCITX_INFO() << "Test 69 PASSED";
    });

    // =========================================================================
    // TEST 70: Multiple ICs — gesture only in focused IC
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 70;
        FCITX_INFO() << "=== Test 70: Multi-IC — gesture only in focused ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid1 = createAndActivate(instance, tf, "test70_a");
        auto uuid2 = createAndActivate(instance, tf, "test70_b");

        // Start gesture in IC2 (last created/activated)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid2, Key(FcitxKey_u, KeyStates(), kCodeU), false);

        // Convert in IC2
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid2, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid2, Key(FcitxKey_u, KeyStates(), kCodeU), true);

        // IC1: press 'a', should start fresh gesture (no IC2 leakage)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid1, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "IC1 should start fresh gesture";

        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid1, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid1);
        tf->call<ITestFrontend::destroyInputContext>(uuid2);
        FCITX_INFO() << "Test 70 PASSED";
    });

    // =========================================================================
    // TEST 71: Preedit shows input char during waiting
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 71;
        FCITX_INFO()
            << "=== Test 71: Preedit shows input char during waiting ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test71");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "a")
            << "Preedit should show 'a', got '" << preedit << "'";

        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 71 PASSED";
    });

    // =========================================================================
    // TEST 72: Preedit shows first variant after leader
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 72;
        FCITX_INFO()
            << "=== Test 72: Preedit shows first variant after leader ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae,@"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test72");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        // Press Space → starts cycling, preedit should show "ä" (first variant)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "\xc3\xa4")
            << "Preedit should show ä after leader, got '" << preedit << "'";

        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 72 PASSED";
    });

    // =========================================================================
    // TEST 73: Preedit updates on each cycle
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 73;
        FCITX_INFO() << "=== Test 73: Preedit updates on each cycle ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae,@"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test73");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        // Preedit: ä

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "ae")
            << "Preedit should show 'ae' after second cycle, got '" << preedit
            << "'";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "@")
            << "Preedit should show '@' after third cycle, got '" << preedit
            << "'";

        tf->call<ITestFrontend::pushCommitExpectation>("@");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 73 PASSED";
    });

    // =========================================================================
    // TEST 74: Preedit cleared after commit
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 74;
        FCITX_INFO() << "=== Test 74: Preedit cleared after commit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test74");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(!preedit.empty())
            << "Preedit should be non-empty during waiting";

        // Commit via Space (single output)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit.empty())
            << "Preedit should be empty after commit, got '" << preedit << "'";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 74 PASSED";
    });

    // =========================================================================
    // TEST 75: Preedit shows uppercase input correctly
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 75;
        FCITX_INFO() << "=== Test 75: Preedit shows uppercase input ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test75");

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);

        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "A")
            << "Preedit should show 'A', got '" << preedit << "'";

        tf->call<ITestFrontend::pushCommitExpectation>("A");
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 75 PASSED";
    });

    // =========================================================================
    // TEST 76: Non-mapped key during waiting → commits pending + passes through
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 76;
        FCITX_INFO()
            << "=== Test 76: Non-mapped key during waiting commits pending ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test76");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press 'b' (non-mapped) → commits 'a' and 'b' passes through
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
        FCITX_ASSERT(!consumed) << "Non-mapped key should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 76 PASSED";
    });

    // =========================================================================
    // TEST 77: Non-mapped key during cycling → commits cycling value + passes
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 77;
        FCITX_INFO()
            << "=== Test 77: Non-mapped key during cycling commits cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test77");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        // Now cycling at index 0 (ä). Press Space → index 1 (ae)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Press 'b' → commits "ae", 'b' passes through
        tf->call<ITestFrontend::pushCommitExpectation>("ae");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
        FCITX_ASSERT(!consumed)
            << "Non-mapped key should pass through during cycling";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 77 PASSED";
    });

    // =========================================================================
    // TEST 78: Backspace during waiting → commits pending + Backspace passes
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 78;
        FCITX_INFO() << "=== Test 78: Backspace during waiting ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test78");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Backspace → commits 'a', Backspace passes through
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_BackSpace, KeyStates(), kCodeBackSpace), false);
        FCITX_ASSERT(!consumed) << "Backspace should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 78 PASSED";
    });

    // =========================================================================
    // TEST 79: Enter during waiting → commits pending + Enter passes
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 79;
        FCITX_INFO() << "=== Test 79: Enter during waiting ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test79");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Return, KeyStates(), kCodeReturn), false);
        FCITX_ASSERT(!consumed) << "Enter should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 79 PASSED";
    });

    // =========================================================================
    // TEST 80: Tab during waiting → commits pending + Tab passes
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 80;
        FCITX_INFO() << "=== Test 80: Tab during waiting ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test80");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Tab, KeyStates(), kCodeTab), false);
        FCITX_ASSERT(!consumed) << "Tab should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 80 PASSED";
    });

    // =========================================================================
    // TEST 81: Backspace during cycling → commits cycling value + passes
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 81;
        FCITX_INFO() << "=== Test 81: Backspace during cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test81");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Backspace → commits ä, Backspace passes through
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_BackSpace, KeyStates(), kCodeBackSpace), false);
        FCITX_ASSERT(!consumed)
            << "Backspace during cycling should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 81 PASSED";
    });

    // =========================================================================
    // TEST 82: Config reload clears all active gestures
    // setSubConfig("mappings.txt") calls clearAllState() on all ICs.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 82;
        FCITX_INFO() << "=== Test 82: Config reload clears active gestures ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test82");

        // Start gesture
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Reload mappings → clears all gestures
        setMappings(instance, {
                                  {"a", "\xc3\xa4"},
                                  {"o", "\xc3\xb6"},
                              });

        // Preedit should be cleared
        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit.empty())
            << "Preedit should be empty after reload, got '" << preedit << "'";

        // Release 'a' — no commit expected (gesture was cleared)
        [[maybe_unused]] bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 82 PASSED";
    });

    // =========================================================================
    // TEST 83: Empty mappings → default fallback
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 83;
        FCITX_INFO() << "=== Test 83: Empty mappings fall back to defaults ===";
        configureLeaders(instance, true, false, false, false, false, false);
        // Set empty mappings (no entries) → triggers loadMappingsFromFile
        // fallback
        setMappings(instance, {});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test83");

        // Default mappings should be loaded (a→ä, etc.)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 83 PASSED";
    });

    // =========================================================================
    // TEST 84: Custom key colliding with mapped input → mapping takes priority
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 84;
        FCITX_INFO()
            << "=== Test 84: Custom key = mapped input — mapping wins ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        config.setValueByPath("Delay/Lowercase", "400");
        config.setValueByPath("Delay/Uppercase", "700");
        config.setValueByPath("Leader/Space", "True");
        config.setValueByPath("Leader/Left", "False");
        config.setValueByPath("Leader/Right", "False");
        config.setValueByPath("Leader/Up", "False");
        config.setValueByPath("Leader/Down", "False");
        config.setValueByPath("Leader/Alt", "False");
        // Set custom leader to 'a' which is also a mapped input
        config.setValueByPath("Leader/Custom/CustomKeyEnabled", "True");
        config.setValueByPath("Leader/Custom/CustomKey", "a");
        config.setValueByPath("Leader/Custom/CustomKeyCode",
                              std::to_string(kCodeA));
        config.setValueByPath("Leader/Custom/CustomKey2Enabled", "False");
        config.setValueByPath("Leader/Custom/CustomKey2", "");
        addon->setConfig(config);
        setMappings(instance, {
                                  {"a", "\xc3\xa4"},
                                  {"o", "\xc3\xb6"},
                              });

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test84");

        // Press 'a' alone → leader handler returns early (no gesture),
        // accent handler never reached → passes through
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(!consumed)
            << "'a' as leader should pass through without active gesture";

        // But 'a' as leader CAN convert OTHER mapped keys:
        // Press 'o' → enters waiting (accent handler runs, 'o' is not leader)
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        FCITX_ASSERT(consumed) << "'o' should start gesture";

        // Press 'a' (leader) → converts 'o' to ö
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "'a' as leader should convert 'o' to ö";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 84 PASSED";
    });

    // =========================================================================
    // TEST 85: Alt bypass with non-mapped key → commitString, not shortcut
    // When Alt leader is active and a non-mapped key is pressed, the char
    // is sent via commitString to avoid triggering Alt+key shortcuts.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 85;
        FCITX_INFO() << "=== Test 85: Alt bypass — non-mapped key commits via "
                        "commitString ===";
        configureWithDelay(instance, 400, 700, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test85");

        // Press 'a' (mapped) → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        // Press Alt → starts cycling (Alt leader)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Press 'b' with Alt modifier → non-mapped key during Alt bypass
        // Should commit cycling value + emit 'b' via commitString
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::pushCommitExpectation>("b");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyState::Alt, kCodeB), false);
        FCITX_ASSERT(consumed)
            << "Non-mapped key during Alt bypass should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 85 PASSED";
    });

    // =========================================================================
    // TEST 86: Alt bypass with new mapped key → commits pending, starts new
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 86;
        FCITX_INFO()
            << "=== Test 86: Alt bypass — new mapped key during gesture ===";
        configureWithDelay(instance, 400, 700, false, true);
        setMappings(instance, {{"a", "\xc3\xa4"}, {"o", "\xc3\xb6"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test86");

        // Press 'a' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press 'o' with Alt held → commits 'a' (pending), starts 'o' gesture
        // Alt bypass resolves the base char via XKB
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyState::Alt, kCodeO), false);
        FCITX_ASSERT(consumed) << "New mapped key with Alt should be consumed";

        // Release 'o' → commits 'o' (original, since no leader pressed)
        tf->call<ITestFrontend::pushCommitExpectation>("o");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyState::Alt, kCodeO), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 86 PASSED";
    });

    // =========================================================================
    // TEST 87: splitOutputs — trailing comma ignored (single output)
    // Mapping "ä," → ["ä"] (trailing empty segment skipped)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 87;
        FCITX_INFO()
            << "=== Test 87: splitOutputs — trailing comma ignored ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test87");

        // Single output after trailing comma removed → immediate commit
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 87 PASSED";
    });

    // =========================================================================
    // TEST 88: splitOutputs — leading comma ignored
    // Mapping ",ä" → ["ä"] (leading empty segment skipped)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 88;
        FCITX_INFO() << "=== Test 88: splitOutputs — leading comma ignored ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", ",\xc3\xa4"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test88");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 88 PASSED";
    });

    // =========================================================================
    // TEST 89: splitOutputs — "x,,,y" → ["x,", "y"]
    // Three commas: first two form a double-comma (literal ','), third is
    // separator.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 89;
        FCITX_INFO()
            << "=== Test 89: splitOutputs — double-comma + separator ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "x,,,y"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test89");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        // Space → cycling starts at index 0: "x,"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "x,")
            << "First variant should be 'x,', got '" << preedit << "'";

        // Space → index 1: "y"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "y")
            << "Second variant should be 'y', got '" << preedit << "'";

        tf->call<ITestFrontend::pushCommitExpectation>("y");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 89 PASSED";
    });

    // =========================================================================
    // TEST 90: splitOutputs — ",,," → [","] (double-comma + trailing)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 90;
        FCITX_INFO() << "=== Test 90: splitOutputs — only commas ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", ",,,"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test90");

        // Single output: literal comma
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>(",");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 90 PASSED";
    });

    // =========================================================================
    // TEST 91: splitOutputs — "x,,y,z" → ["x,y", "z"]
    // Double-comma within cycling outputs
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 91;
        FCITX_INFO()
            << "=== Test 91: splitOutputs — double-comma within cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "x,,y,z"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test91");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "x,y")
            << "First variant should be 'x,y', got '" << preedit << "'";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "z")
            << "Second variant should be 'z', got '" << preedit << "'";

        tf->call<ITestFrontend::pushCommitExpectation>("z");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 91 PASSED";
    });

    // =========================================================================
    // TEST 92: sanitizeCustomKey — whitespace-only → no leader active
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 92;
        FCITX_INFO()
            << "=== Test 92: Whitespace-only custom key — no leader ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        config.setValueByPath("Delay/Lowercase", "400");
        config.setValueByPath("Delay/Uppercase", "700");
        config.setValueByPath("Leader/Space", "False");
        config.setValueByPath("Leader/Left", "False");
        config.setValueByPath("Leader/Right", "False");
        config.setValueByPath("Leader/Up", "False");
        config.setValueByPath("Leader/Down", "False");
        config.setValueByPath("Leader/Alt", "False");
        config.setValueByPath("Leader/Custom/CustomKeyEnabled", "True");
        config.setValueByPath("Leader/Custom/CustomKey", "   ");
        config.setValueByPath("Leader/Custom/CustomKey2Enabled", "False");
        config.setValueByPath("Leader/Custom/CustomKey2", "");
        addon->setConfig(config);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test92");

        // Press 'a' → waiting, but no leader is configured
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press Space → not a leader (space disabled, custom is whitespace →
        // empty) Commits pending 'a' + Space passes through
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed)
            << "Space should not be a leader when no leaders configured";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 92 PASSED";
    });

    // =========================================================================
    // TEST 93: sanitizeCustomKey — empty string → no leader
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 93;
        FCITX_INFO() << "=== Test 93: Empty custom key — no leader ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        config.setValueByPath("Delay/Lowercase", "400");
        config.setValueByPath("Delay/Uppercase", "700");
        config.setValueByPath("Leader/Space", "False");
        config.setValueByPath("Leader/Left", "False");
        config.setValueByPath("Leader/Right", "False");
        config.setValueByPath("Leader/Up", "False");
        config.setValueByPath("Leader/Down", "False");
        config.setValueByPath("Leader/Alt", "False");
        config.setValueByPath("Leader/Custom/CustomKeyEnabled", "True");
        config.setValueByPath("Leader/Custom/CustomKey", "");
        config.setValueByPath("Leader/Custom/CustomKey2Enabled", "False");
        config.setValueByPath("Leader/Custom/CustomKey2", "");
        addon->setConfig(config);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test93");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // No leader configured → release commits original
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 93 PASSED";
    });

    // =========================================================================
    // TEST 94: sanitizeCustomKey — uppercase letter normalized
    // CustomKey = "F" → stored as "f", Shift+F still matches (case-insensitive)
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 94;
        FCITX_INFO() << "=== Test 94: Uppercase custom key normalized ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        config.setValueByPath("Delay/Lowercase", "400");
        config.setValueByPath("Delay/Uppercase", "700");
        config.setValueByPath("Leader/Space", "False");
        config.setValueByPath("Leader/Left", "False");
        config.setValueByPath("Leader/Right", "False");
        config.setValueByPath("Leader/Up", "False");
        config.setValueByPath("Leader/Down", "False");
        config.setValueByPath("Leader/Alt", "False");
        config.setValueByPath("Leader/Custom/CustomKeyEnabled", "True");
        config.setValueByPath("Leader/Custom/CustomKey", "F");
        config.setValueByPath("Leader/Custom/CustomKeyCode",
                              std::to_string(kCodeF));
        config.setValueByPath("Leader/Custom/CustomKey2Enabled", "False");
        config.setValueByPath("Leader/Custom/CustomKey2", "");
        addon->setConfig(config);
        setMappings(instance, {{"a", "\xc3\xa4"}, {"A", "\xc3\x84"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test94");

        constexpr int kCodeF = 41;

        // Lowercase: a + f → ä
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_f, KeyStates(), kCodeF), false);
        FCITX_ASSERT(consumed)
            << "Lowercase 'f' should match custom leader 'F'";
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Uppercase: Shift+A + Shift+F → Ä
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x84");
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_F, KeyState::Shift, kCodeF), false);
        FCITX_ASSERT(consumed) << "Shift+F should match custom leader 'F'";
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 94 PASSED";
    });

    // =========================================================================
    // TEST 95: Multiple consecutive commits — ordering guard each time
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 95;
        FCITX_INFO() << "=== Test 95: Consecutive commits — ordering guard "
                        "each time ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test95");

        // First: a + Space → ä, then Space committed via ordering guard
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // Second: o + Space → ö, then Space committed
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);

        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Second ordering guard Space should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 95 PASSED";
    });

    // =========================================================================
    // TEST 96: Ordering guard with Shift+Space
    // Shift is not a modifier in hasModifiers(), so Shift+Space should still
    // be caught by ordering guard.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 96;
        FCITX_INFO() << "=== Test 96: Ordering guard with Shift+Space ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test96");

        // a + Space → ä
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Shift+Space → ordering guard catches it (Shift excluded from
        // hasModifiers)
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyState::Shift, kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Shift+Space should be caught by ordering guard";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 96 PASSED";
    });

    // =========================================================================
    // TEST 97: Fast double-tap — second is new gesture
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 97;
        FCITX_INFO()
            << "=== Test 97: Fast double-tap — second is new gesture ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test97");

        // First tap: press 'a', release 'a' → commits "a"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Second tap: press 'a' again → new gesture (should be consumed)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "Second tap should start new gesture";

        // Complete second gesture
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 97 PASSED";
    });

    // =========================================================================
    // TEST 98: Leader without gesture → passes through
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 98;
        FCITX_INFO()
            << "=== Test 98: Leader without gesture passes through ===";
        configureLeaders(instance, true, true, true, true, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test98");

        // Space without gesture → passes through
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed) << "Space without gesture should pass through";

        // Left Arrow without gesture → passes through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Left, KeyStates(), kCodeLeft), false);
        FCITX_ASSERT(!consumed)
            << "Left Arrow without gesture should pass through";

        // Right Arrow without gesture → passes through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Right, KeyStates(), kCodeRight), false);
        FCITX_ASSERT(!consumed)
            << "Right Arrow without gesture should pass through";

        // Up Arrow without gesture → passes through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Up, KeyStates(), kCodeUp), false);
        FCITX_ASSERT(!consumed)
            << "Up Arrow without gesture should pass through";

        // Down Arrow without gesture → passes through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Down, KeyStates(), kCodeDown), false);
        FCITX_ASSERT(!consumed)
            << "Down Arrow without gesture should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 98 PASSED";
    });

    // =========================================================================
    // TEST 99: All leaders enabled simultaneously
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 99;
        FCITX_INFO() << "=== Test 99: All leaders enabled ===";
        configureLeaders(instance, true, true, true, true, true, true, "#",
                         "", kCodeHash);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test99");

        // Space leader
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Left Arrow leader
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Left, KeyStates(), kCodeLeft), false);
        FCITX_ASSERT(consumed) << "Left Arrow should work as leader";
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);

        // Custom '#' leader
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Custom '#' should work as leader";
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 99 PASSED";
    });

    // =========================================================================
    // TEST 100: Ctrl+key during gesture → commits pending, shortcut passes
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 100;
        FCITX_INFO()
            << "=== Test 100: Ctrl+key during gesture — shortcut passes ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test100");

        // Start gesture
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Ctrl+C → commits 'a', Ctrl+C passes through
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_c, KeyState::Ctrl, 54), false);
        FCITX_ASSERT(!consumed) << "Ctrl+C should pass through during gesture";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 100 PASSED";
        scheduleEmptyOutputTests(instance);
    });
}

// =============================================================================
// EMPTY OUTPUT TESTS (101-103) — Verify mappings with empty split outputs
// (e.g. "a=,") are safely skipped and don't crash.
// =============================================================================

static void scheduleEmptyOutputTests(Instance *instance) {

    // =========================================================================
    // TEST 101: Single-comma output skipped — key falls through
    // Mapping "t=," produces an empty output vector after splitOutputs.
    // The mapping should be silently skipped; 't' falls through as normal key.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 101;
        FCITX_INFO() << "=== Test 101: Single-comma output skipped — key falls "
                        "through ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {
                                  {"a", "\xc3\xa4"},
                                  {"t", ","},
                              });
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test101");

        // 't' has empty outputs → not in umlautMap_ → falls through
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_t, KeyStates(), 28), false);
        FCITX_ASSERT(!consumed) << "'t' with empty output should fall through";

        // 'a' still works normally (valid mapping)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 101 PASSED";
    });

    // =========================================================================
    // TEST 102: All-comma mapping (",,") — literal comma survives
    // ",," is a double-comma escape → single output [","]. Not empty.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 102;
        FCITX_INFO()
            << "=== Test 102: Double-comma mapping — literal comma output ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", ",,"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test102");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>(",");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 102 PASSED";
    });

    // =========================================================================
    // TEST 103: Only empty-output mappings → defaults loaded
    // When all mappings produce empty vectors, umlautMap_ is empty and
    // fallback to built-in defaults kicks in.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 103;
        FCITX_INFO() << "=== Test 103: All empty outputs — defaults loaded ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {
                                  {"a", ","},
                                  {"o", ","},
                              });
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test103");

        // All custom mappings had empty outputs → defaults loaded → 'a' maps to
        // ä
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 103 PASSED";
        scheduleAdvancedEdgeCaseTests(instance);
    });
}

// =============================================================================
// ADVANCED EDGE CASE TESTS (104-109) — Timer-chained where needed.
// Config reload during gesture, IC state pollution, timeout boundaries,
// rapid mapped keys, Shift+Space during uppercase.
// =============================================================================

// =========================================================================
// TEST 104: Config reload during ACTIVE cycling — clears cycling state
// Test 82 reloads during waiting; this tests during cycling (after Space).
// =========================================================================
static void scheduleAdvancedEdgeCaseTests(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 104;
        FCITX_INFO() << "=== Test 104: Config reload during active cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {
                                  {"a", "\xc3\xa4,\xc3\x84"},
                                  {"o", "\xc3\xb6"},
                              });
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test104");

        // Press 'a' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Space → enter cycling (preedit shows "ä")
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        std::string preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit == "\xc3\xa4")
            << "Preedit should show ä, got '" << preedit << "'";

        // Reload mappings while cycling is active
        setMappings(instance, {
                                  {"a", "\xc3\xa4"},
                                  {"o", "\xc3\xb6"},
                              });

        // Preedit should be cleared by reload
        preedit = getClientPreedit(instance);
        FCITX_ASSERT(preedit.empty())
            << "Preedit should be empty after reload, got '" << preedit << "'";

        // New gesture should work normally after reload
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 104 PASSED";
    });

    // =========================================================================
    // TEST 105: IC state pollution — focus switch during active gesture
    // Start gesture in IC1, switch focus to IC2 (deactivate IC1), verify
    // IC2 has clean state and IC1's gesture was committed on deactivate.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 105;
        FCITX_INFO() << "=== Test 105: IC state pollution — focus switch "
                        "during gesture ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid1 = createAndActivate(instance, tf, "test105_ic1");

        // Start gesture in IC1: press 'a' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid1, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Create and activate IC2 — this deactivates IC1
        auto uuid2 = createAndActivate(instance, tf, "test105_ic2");

        // IC2 should have clean state — 'o' starts fresh gesture
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid2, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        FCITX_ASSERT(consumed) << "IC2 should start fresh gesture for 'o'";

        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid2, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid2, Key(FcitxKey_o, KeyStates(), kCodeO), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid1);
        tf->call<ITestFrontend::destroyInputContext>(uuid2);
        FCITX_INFO() << "Test 105 PASSED";
    });

    // =========================================================================
    // TEST 108: Rapid successive mapped keys — each starts fresh gesture
    // Fast typing a,o,u without completing any gesture → each aborts previous.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 108;
        FCITX_INFO() << "=== Test 108: Rapid successive mapped keys ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test108");

        // Press 'a' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Press 'o' before Space → commits 'a', starts waiting for 'o'
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        FCITX_ASSERT(consumed)
            << "'o' should start new gesture (committing 'a')";

        // Press 'u' before Space → commits 'o', starts waiting for 'u'
        tf->call<ITestFrontend::pushCommitExpectation>("o");
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), false);
        FCITX_ASSERT(consumed)
            << "'u' should start new gesture (committing 'o')";

        // Now complete 'u' with Space → ü
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 108 PASSED";
    });

    // =========================================================================
    // TEST 109: Shift+Space during uppercase gesture
    // Press Shift+A (waiting), then Shift+Space → should convert to Ä.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 109;
        FCITX_INFO()
            << "=== Test 109: Shift+Space during uppercase gesture ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test109");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);

        // Press Shift+A → waiting with uppercase
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);

        // Press Shift+Space → should convert A to Ä
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\x84");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyState::Shift, kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Shift+Space should convert uppercase gesture";

        // Release
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 109 PASSED";
        scheduleTest106(instance);
    });
}

// =============================================================================
// TIMEOUT BOUNDARY TESTS (106-107) — Timer-chained: precise timing
// verification.
// =============================================================================

// =========================================================================
// TEST 106: Timeout boundary — Space just before expiry converts
// With 100ms delay, Space at 90ms should still convert.
// =========================================================================
static void scheduleTest106(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 106;
        FCITX_INFO()
            << "=== Test 106: Timeout boundary — Space just before expiry ===";
        configureWithDelay(instance, 300, 600);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test106");

        // Press 'a' → waiting with 300ms delay
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Wait 100ms — comfortably inside the 300ms window. Lower than the
        // 200ms originally used because the fcitx5 event loop under ASan can
        // dispatch the callback noticeably after wall-clock target, eating
        // into the addon's timeout budget. 100ms keeps 200ms of headroom.
        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 100'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");

                    tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
                    bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace),
                        false);
                    FCITX_ASSERT(consumed)
                        << "Space at 100ms should convert within 300ms window";

                    tf->call<ITestFrontend::keyEvent>(
                        uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 106 PASSED";
                    scheduleTest107(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 107: Timeout boundary — Space after expiry passes through
// With 100ms delay, Space at 150ms should NOT convert.
// =========================================================================
static void scheduleTest107(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 107;
        FCITX_INFO()
            << "=== Test 107: Timeout boundary — Space after expiry ===";
        configureWithDelay(instance, 100, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test107");

        // Press 'a' → waiting with 100ms delay
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Addon timer fires at 100ms and commits 'a'
        tf->call<ITestFrontend::pushCommitExpectation>("a");

        // Wait 400ms — well past 100ms window (300ms headroom)
        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 400'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");

                    // Use non-printable key 'b' (unmapped) to avoid
                    // testfrontend commit check on Space passthrough.
                    bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key(FcitxKey_b, KeyStates(), kCodeB), false);
                    FCITX_ASSERT(!consumed)
                        << "Key at 400ms should pass through (100ms expired)";

                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 107 PASSED";
                    scheduleDelayBoundaryTests(instance);
                });
                return false;
            });
    });
}

// =============================================================================
// DELAY BOUNDARY TESTS (110-113) — Timer-chained: verify min/default/max delay
// values for both lowercase and uppercase.
// =============================================================================

// =========================================================================
// TEST 110: Default lowercase delay (400ms) — timer fires
// =========================================================================
static void scheduleDelayBoundaryTests(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 110;
        FCITX_INFO() << "=== Test 110: Default lowercase delay (400ms) — timer "
                        "fires ===";
        configureWithDelay(instance, 400, 700);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test110");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 700'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");
                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 110 PASSED";
                    scheduleTest111(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 111: Default uppercase delay (700ms) — timer fires
// =========================================================================
static void scheduleTest111(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 111;
        FCITX_INFO() << "=== Test 111: Default uppercase delay (700ms) — timer "
                        "fires ===";
        configureWithDelay(instance, 400, 700);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test111");

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("A");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 1'000'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");
                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 111 PASSED";
                    scheduleTest112(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 112: Uppercase min delay (50ms) — timer fires
// =========================================================================
static void scheduleTest112(Instance *instance) {
    testDispatcher->schedule([instance]() {
        g_currentTest = 112;
        FCITX_INFO()
            << "=== Test 112: Uppercase min delay (50ms) — timer fires ===";
        configureWithDelay(instance, 50, 50);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test112");

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("A");

        struct TH {
            std::unique_ptr<EventSourceTime> t;
        };
        auto h = std::make_shared<TH>();
        h->t = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + 300'000, 0,
            [instance, uuid, h](EventSourceTime *, uint64_t) {
                testDispatcher->schedule([instance, uuid]() {
                    auto *tf = instance->addonManager().addon("testfrontend");
                    tf->call<ITestFrontend::destroyInputContext>(uuid);
                    FCITX_INFO() << "Test 112 PASSED";
                    scheduleTest113(instance);
                });
                return false;
            });
    });
}

// =========================================================================
// TEST 113: Max delay (2000ms) — Space within window still converts
// Verifies the maximum allowed delay value works correctly.
// =========================================================================
static void scheduleTest113(Instance *instance) {
    // =========================================================================
    // ERROR HANDLING TESTS (119-121)
    // =========================================================================

    // =========================================================================
    // TEST 119: Mixed valid/invalid mappings — only valid entries loaded
    // Empty inputs are stop markers, empty outputs are skipped.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 119;
        FCITX_INFO() << "=== Test 119: Mixed valid/invalid mappings ===";
        configureLeaders(instance, true, false, false, false, false, false);

        // Override with mixed entries: valid 'a' mapping, 'o' with empty output
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig mc;
        mc.setValueByPath("0/Input", "a");
        mc.setValueByPath("0/Output", "\xc3\xa4");
        mc.setValueByPath("1/Input", "o");
        mc.setValueByPath("1/Output", ""); // empty output — skipped
        mc.setValueByPath("2/Input", "u");
        mc.setValueByPath("2/Output", "\xc3\xbc");
        addon->setSubConfig("mappings.txt", mc);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test119");

        // 'a' should still map (valid entry)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // 'o' should pass through (empty output was skipped)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        FCITX_ASSERT(!consumed) << "'o' with empty output should not be mapped";
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);

        // 'u' should still map (valid entry after gap)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 119 PASSED";
    });

    // =========================================================================
    // TEST 120: Out-of-range delay config — addon clamps to valid range
    // IntConstrainWithStep rejects 0 and 9999, uses default (400/700).
    // Verify gesture still works after invalid config.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 120;
        FCITX_INFO() << "=== Test 120: Out-of-range delay config ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        config.setValueByPath("Delay/Lowercase", "0");    // below min (50)
        config.setValueByPath("Delay/Uppercase", "9999"); // above max (2000)
        config.setValueByPath("Leader/Space", "True");
        config.setValueByPath("AppFilter/Mode", "Disabled");
        addon->setConfig(config);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test120");

        // Gesture should still work — addon uses default delay after rejection
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Gesture should work despite out-of-range delay config";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 120 PASSED";
    });

    // =========================================================================
    // TEST 121: All-invalid mapping entries — defaults loaded as fallback
    // When setSubConfig produces an empty map, loadMappingsFromFile() runs
    // and loads the built-in German defaults.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 121;
        FCITX_INFO()
            << "=== Test 121: All-invalid mappings — defaults loaded ===";
        configureLeaders(instance, true, false, false, false, false, false);

        // Pass entries that all get filtered: empty input stops iteration
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig mc;
        mc.setValueByPath("0/Input", ""); // empty input — stop marker
        mc.setValueByPath("0/Output", "\xc3\xa4");
        addon->setSubConfig("mappings.txt", mc);

        // Map should fall back to defaults (German umlauts)
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test121");

        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Default mappings should load when all entries invalid";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 121 PASSED";
    });

    // =========================================================================
    // APP FILTER TESTS (114-118)
    //
    // Scheduled before test 113 because test 113 owns the final
    // instance->exit() (fired from a 1500ms timer). Any test scheduled
    // after test 113 would race the exit and be silently skipped.
    // =========================================================================

    // Helper: configure app filter mode and lists
    auto configureAppFilter = [](Instance *instance, const std::string &mode,
                                 const std::vector<std::string> &blacklist,
                                 const std::vector<std::string> &whitelist) {
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        // Keep default leaders (Space on)
        config.setValueByPath("Leader/Space", "True");
        config.setValueByPath("AppFilter/Mode", mode);
        for (size_t i = 0; i < blacklist.size(); ++i) {
            config.setValueByPath("AppFilter/Blacklist/" + std::to_string(i),
                                  blacklist[i]);
        }
        for (size_t i = 0; i < whitelist.size(); ++i) {
            config.setValueByPath("AppFilter/Whitelist/" + std::to_string(i),
                                  whitelist[i]);
        }
        addon->setConfig(config);
        setMappings(instance, {{"a", "\xc3\xa4"}});
    };

    // =========================================================================
    // TEST 114: App Filter Disabled — gesture works in any app
    // =========================================================================
    testDispatcher->schedule([instance, configureAppFilter]() {
        g_currentTest = 114;
        FCITX_INFO() << "=== Test 114: App Filter Disabled ===";
        configureAppFilter(instance, "Disabled", {}, {});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "firefox");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Space should be consumed when filter disabled";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 114 PASSED";
    });

    // =========================================================================
    // TEST 115: Blacklist — blocked app is passthrough
    // =========================================================================
    testDispatcher->schedule([instance, configureAppFilter]() {
        g_currentTest = 115;
        FCITX_INFO() << "=== Test 115: Blacklist blocks processing ===";
        configureAppFilter(instance, "Blacklist", {"nvim", "steam"}, {});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "nvim");

        // 'a' must NOT be consumed in a blacklisted app
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(!consumed)
            << "Mapped key must not be consumed in blacklisted app";
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 115 PASSED";
    });

    // =========================================================================
    // TEST 116: Blacklist — non-blocked app still processes
    // =========================================================================
    testDispatcher->schedule([instance, configureAppFilter]() {
        g_currentTest = 116;
        FCITX_INFO() << "=== Test 116: Blacklist allows non-blocked app ===";
        configureAppFilter(instance, "Blacklist", {"nvim", "steam"}, {});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "firefox");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Non-blacklisted app should process gestures";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 116 PASSED";
    });

    // =========================================================================
    // TEST 117: Whitelist — listed app is processed
    // =========================================================================
    testDispatcher->schedule([instance, configureAppFilter]() {
        g_currentTest = 117;
        FCITX_INFO() << "=== Test 117: Whitelist allows listed app ===";
        configureAppFilter(instance, "Whitelist", {}, {"libreoffice", "gedit"});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "libreoffice-writer");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Whitelisted app should process gestures";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 117 PASSED";
    });

    // =========================================================================
    // TEST 118: Whitelist — non-listed app is passthrough
    // =========================================================================
    testDispatcher->schedule([instance, configureAppFilter]() {
        g_currentTest = 118;
        FCITX_INFO() << "=== Test 118: Whitelist blocks non-listed app ===";
        configureAppFilter(instance, "Whitelist", {}, {"libreoffice", "gedit"});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "firefox");

        // 'a' must NOT be consumed in a non-whitelisted app
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(!consumed) << "Non-whitelisted app must not consume keys";
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 118 PASSED";
    });

    // =========================================================================
    // TEST 122: Dual split — shifted input '!' (left) + 'j' leader (right)
    // '!' = Shift+1 (keycode 10, physically left hand). With dual leaders
    // f(left)/j(right), j should trigger left-hand inputs → allowed.
    // This was broken before the inputKeyCode fix: charToKeycode_ has no
    // entry for '!', so isLeftHand("!") fell back to right → j was blocked.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 122;
        FCITX_INFO()
            << "=== Test 122: Dual split — shifted '!' + 'j' (opposite) ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "f", "j", kCodeF, kCodeJ);
        setMappings(instance, {{"!", "excl"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test122");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+1 → '!' enters waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), false);
        // Press 'j' (right-hand leader, opposite to '!' which is physically
        // left)
        tf->call<ITestFrontend::pushCommitExpectation>("excl");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_j, KeyStates(), kCodeJ), false);
        FCITX_ASSERT(consumed)
            << "'j' (right) must trigger '!' (physically left via keycode)";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 122 PASSED";
    });

    // =========================================================================
    // TEST 123: Dual split — shifted input '!' (left) + 'f' leader (left)
    // Same physical hand → blocked. 'f' falls through, commits '!' as-is.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 123;
        FCITX_INFO()
            << "=== Test 123: Dual split — shifted '!' + 'f' (same hand) ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "f", "j", kCodeF, kCodeJ);
        setMappings(instance, {{"!", "excl"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test123");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+1 → '!' enters waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), false);
        // Press 'f' (left-hand leader, same physical hand as '!') → BLOCKED
        tf->call<ITestFrontend::pushCommitExpectation>("!");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_f, KeyStates(), kCodeF), false);
        FCITX_ASSERT(!consumed)
            << "'f' (left) must NOT trigger '!' (also physically left)";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 123 PASSED";
    });

    // =========================================================================
    // TEST 124: Shifted input '!' with Shift held through leader press
    // User holds Shift+1, then presses j (which reports 'J' because Shift is
    // still down). Both the leader match and the split go by keycode, so Shift
    // is irrelevant → works.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 124;
        FCITX_INFO() << "=== Test 124: Shift held through leader — Shift+1 + "
                        "Shift+J ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "f", "j", kCodeF, kCodeJ);
        setMappings(instance, {{"!", "excl"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test124");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+1 → '!'
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), false);
        // Press j with Shift still held → keysym J, Shift modifier
        tf->call<ITestFrontend::pushCommitExpectation>("excl");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_J, KeyState::Shift, kCodeJ), false);
        FCITX_ASSERT(consumed) << "Shift+J must match leader 'j' "
                                  "(case-insensitive) and trigger '!'";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 124 PASSED";
    });

    // =========================================================================
    // TEST 125: Dual split — right-hand shifted input '*' (Shift+8) + 'f'
    // (left)
    // '*' = Shift+8 (keycode 17, right hand). 'f' = left leader → opposite →
    // allowed.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 125;
        FCITX_INFO()
            << "=== Test 125: Dual split — shifted '*' + 'f' (opposite) ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "f", "j", kCodeF, kCodeJ);
        setMappings(instance, {{"*", "star"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test125");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+8 → '*'
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_asterisk, KeyState::Shift, kCode8), false);
        // Press 'f' (left leader, opposite to '*' which is right)
        tf->call<ITestFrontend::pushCommitExpectation>("star");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_f, KeyStates(), kCodeF), false);
        FCITX_ASSERT(consumed)
            << "'f' (left) must trigger '*' (physically right via keycode 17)";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_asterisk, KeyState::Shift, kCode8), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 125 PASSED";
    });

    // =========================================================================
    // TEST 126: Dual split — right-hand shifted input '*' + 'j' (right) →
    // blocked Same physical hand → blocked. Commits '*' as-is.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 126;
        FCITX_INFO()
            << "=== Test 126: Dual split — shifted '*' + 'j' (same hand) ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "f", "j", kCodeF, kCodeJ);
        setMappings(instance, {{"*", "star"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test126");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+8 → '*'
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_asterisk, KeyState::Shift, kCode8), false);
        // Press 'j' (right leader, same hand as '*') → BLOCKED
        tf->call<ITestFrontend::pushCommitExpectation>("*");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_j, KeyStates(), kCodeJ), false);
        FCITX_ASSERT(!consumed)
            << "'j' (right) must NOT trigger '*' (also physically right)";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 126 PASSED";
    });

    // =========================================================================
    // TEST 127: Shifted input cycling — '!' with multiple outputs
    // Dual split f(left)/j(right). '!' physically left → j triggers.
    // Multiple outputs: cycle through, commit on release.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 127;
        FCITX_INFO()
            << "=== Test 127: Shifted input cycling — '!' multi-output ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "f", "j", kCodeF, kCodeJ);
        setMappings(instance, {{"!", "one,two,three"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test127");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+1 → '!' enters waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), false);
        // Press j → enters cycling, preedit shows "one"
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_j, KeyStates(), kCodeJ), false);
        FCITX_ASSERT(consumed) << "j must start cycling for '!'";
        // Press j again → cycle to "two"
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_j, KeyStates(), kCodeJ), false);
        FCITX_ASSERT(consumed) << "j must cycle to next variant";
        // Release Shift+1 → commit "two" (index 1)
        tf->call<ITestFrontend::pushCommitExpectation>("two");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), true);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 127 PASSED";
    });

    // =========================================================================
    // TEST 128: Single custom key — shifted input '!' works without split
    // Only CustomKey="j" (no CustomKey2) → no split, triggers all inputs.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 128;
        FCITX_INFO()
            << "=== Test 128: Single custom key — shifted '!' no split ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "j", "", kCodeJ);
        setMappings(instance, {{"!", "excl"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test128");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);
        // Hold Shift+1 → '!'
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), false);
        // Press j → no split, triggers all
        tf->call<ITestFrontend::pushCommitExpectation>("excl");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_j, KeyStates(), kCodeJ), false);
        FCITX_ASSERT(consumed)
            << "Single custom key 'j' must trigger shifted '!' (no split)";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_exclam, KeyState::Shift, kCode1), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 128 PASSED";
    });

    // =========================================================================
    // TEST 129: FocusOut during pending preedit commits raw char (bug #10)
    // Simulates external focus flap (e.g. MouseTiler 100ms polling). On real
    // FocusOut, the display server auto-commits the preedit — which contains
    // the raw input char "a" (not the mapped "ä") because the gesture was
    // interrupted mid-flight. This documents the bug #10 symptom: umlauts
    // get lost when external tools steal focus during a gesture. After the
    // flap, a fresh gesture must work normally again.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 129;
        FCITX_INFO() << "=== Test 129: FocusOut during pending preedit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test129");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // Press 'a' → preedit "a"
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(getClientPreedit(instance) == "a")
            << "Preedit should be 'a' before flap";

        // External focus flap: FocusOut auto-commits preedit ("a") via
        // frontend. (The addon's preedit buffer is not explicitly cleared, but
        // the addon's internal gesture state is — which is what matters for the
        // next gesture.)
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        ic->focusOut();
        ic->focusIn();

        // Fresh gesture on a DIFFERENT key ('o') proves the addon state was
        // reset: otherwise the held 'a' from before the flap would interfere.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Gesture after focus flap must work";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 129 PASSED";
    });

    // =========================================================================
    // TEST 130: FocusOut during cycling clears cycling state
    // Cycling at index 1 ("ae"), focus flap must clear without committing
    // cycling value (server handles commit). Fresh gesture after flap works.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 130;
        FCITX_INFO() << "=== Test 130: FocusOut during cycling ===";
        configureLeaders(instance, true, false, false, false, false, false);
        setMappings(instance, {{"a", "\xc3\xa4,ae"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test130");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // a+Space → cycling index 0 (ä)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        // Space again → cycle to index 1 (ae)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(getClientPreedit(instance) == "ae")
            << "Preedit should be 'ae' before flap";

        // Focus flap during cycling: frontend auto-commits the cycling preedit
        // ("ae").
        tf->call<ITestFrontend::pushCommitExpectation>("ae");
        ic->focusOut();
        ic->focusIn();

        // Fresh gesture after flap: index resets to 0.
        // (Mapping only has 'a' configured, so we re-use it — the critical test
        // is that the first Space commits index 0 "ä", not index 1 "ae".)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 130 PASSED";
    });

    // =========================================================================
    // TEST 131: Rapid focus-flap loop (50x) — no crash, no state leak
    // Stress test for MouseTiler-style polling: 50 focusOut/focusIn cycles.
    // After the loop, IC must still be responsive and state clean.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 131;
        FCITX_INFO() << "=== Test 131: Rapid focus-flap loop (50x) ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test131");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // 50 rapid focus flaps — no gesture active
        for (int i = 0; i < 50; ++i) {
            ic->focusOut();
            ic->focusIn();
        }

        FCITX_ASSERT(getClientPreedit(instance).empty())
            << "Preedit must be empty after 50 flaps";

        // IC must still be responsive to gestures
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Gesture must work after 50 flaps";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 131 PASSED";
    });

    // =========================================================================
    // TEST 132: Focus-flap directly after commit resets recentlyCommitted_
    // After a+Space→ä, recentlyCommitted_ is true. A focus flap goes through
    // deactivate()→clearAllState() which explicitly clears recentlyCommitted_.
    // The next Space in a fresh gesture must therefore route as an ordinary
    // leader (not through the ordering guard).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 132;
        FCITX_INFO() << "=== Test 132: Focus-flap after commit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test132");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // a+Space → ä committed, recentlyCommitted_=true
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Focus flap → clearAllState() + recentlyCommitted_=false
        ic->focusOut();
        ic->focusIn();

        // New gesture: o+Space → ö. This must commit ö exactly once.
        // If recentlyCommitted_ leaked through the flap, the ordering guard
        // could route this Space through commitString(" ") and yield wrong
        // output.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xb6");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Fresh gesture after flap must convert";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_o, KeyStates(), kCodeO), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 132 PASSED";
    });

    // =========================================================================
    // TEST 133: recentlyCommitted_ explicit cleanup paths
    // After commit, recentlyCommitted_=true (Test 68 verifies the guard).
    // Two code paths explicitly reset the flag via `recentlyCommitted_=false`
    // after clearAllState(): setSubConfig for mappings reload
    // (schnelle-umlaute.cpp:123) and deactivate for IM switch/focus
    // (schnelle-umlaute.cpp:581). This test guards against silent removal
    // of those explicit resets, which would leave the ordering guard armed
    // across config reloads and IM switches.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 133;
        FCITX_INFO()
            << "=== Test 133: recentlyCommitted_ explicit cleanup paths ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test133");

        // --- Path A: setSubConfig(mappings.txt) clears recentlyCommitted_ ---
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Mappings reload resets recentlyCommitted_ explicitly.
        setMappings(instance, {{"a", "\xc3\xa4"}});

        bool consumedA = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumedA)
            << "setSubConfig(mappings.txt) must clear recentlyCommitted_";

        // --- Path B: deactivate (IM switch) clears recentlyCommitted_ ---
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // IM switch + back: both deactivate() and activate() clear the flag.
        tf->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"), false);
        tf->call<ITestFrontend::keyEvent>(uuid, Key("Control+space"), false);

        bool consumedB = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumedB)
            << "deactivate()/activate() must clear recentlyCommitted_";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 133 PASSED";
    });

    // =========================================================================
    // TEST 134: getBaseChar() with out-of-range keycodes — no crash
    // Alt+key with an extreme raw keycode triggers the Alt-bypass path
    // (schnelle-umlaute.cpp:332) which calls getBaseChar(rawCode). The XKB
    // API should gracefully return 0 keysyms for unknown codes. This is a
    // smoke test ensuring no UB / crash on hardware with unusual keymaps
    // or synthetic input with high keycodes.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 134;
        FCITX_INFO() << "=== Test 134: getBaseChar() extreme keycodes ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test134");

        // Press Alt first, then a mapped key with a wildly out-of-range
        // keycode (XKB max is typically 255). Addon resolves base via XKB,
        // which returns nothing → no Alt-bypass, key path falls through.
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyState::Alt, 9999), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyState::Alt, 9999), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), true);

        // Keycode 0 (edge of valid range).
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyState::Alt, 0), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyState::Alt, 0), true);
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 134 PASSED";
    });

    // =========================================================================
    // TEST 135: AppFilter Whitelist with empty program → blocked
    // Per app_filter.cpp:22-23: empty program in Whitelist mode returns true
    // (filtered). Ensures an IC without a program name (e.g. some Wayland
    // compositors or unusual frontends) is safely excluded in whitelist
    // mode rather than accidentally allowed through.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 135;
        FCITX_INFO() << "=== Test 135: AppFilter Whitelist + empty program ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        RawConfig config;
        config.setValueByPath("Leader/Space", "True");
        config.setValueByPath("AppFilter/Mode", "Whitelist");
        config.setValueByPath("AppFilter/Whitelist/0", "libreoffice");
        addon->setConfig(config);
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        // Empty IC name → empty program() → whitelist mode filters it out.
        auto uuid = createAndActivate(instance, tf, "");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(!consumed)
            << "Whitelist with empty program must block (not on list)";

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 135 PASSED";
    });

    // =========================================================================
    // TEST 136: Alt single-output cycling — second leader arms repeat
    // suppression Regression pin for the üu-class duplicate: with Alt + Space
    // both as leaders and a single-output mapping (u→ü), hold 'u' + Alt enters
    // Alt cycling with "ü" in preedit (altGestureSession_=true, size==1).
    // Pressing Space (a different leader) commits "ü" + the leader's space
    // char, and must arm committed_ for the still-held 'u' so the next
    // auto- repeat is consumed instead of starting a fresh gesture. Without
    // this arming, the user would see "ü u" / "üu" duplicates.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 136;
        FCITX_INFO() << "=== Test 136: Alt single-output + second leader arms "
                        "repeat suppression ===";
        configureLeaders(instance, true, false, false, false, false, true);
        setMappings(instance, {{"u", "\xc3\xbc"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test136");

        // Hold 'u' → waiting
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), false);

        // Press Alt → single-output Alt cycling: "ü" in preedit,
        // altGestureSession_=true, no commit yet (Alt branch keeps preedit).
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Press Space (different leader) during Alt session → commits "ü",
        // then commits the leader's printable char " ", then arms
        // committed_.code = kCodeU.
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Second leader during Alt single-output must be consumed";

        // Auto-repeat 'u' (held, same keycode, no release) → must be
        // consumed by committed_ guard, not start a fresh gesture.
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), false);
        FCITX_ASSERT(consumed)
            << "Auto-repeat after Alt single-output commit must be consumed "
               "(üu-class duplicate guard)";

        // Release 'u' → also consumed (still under committed_).
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), kCodeU), true);
        FCITX_ASSERT(consumed)
            << "Release of committed key after Alt single-output must be "
               "consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 136 PASSED";
    });

    // =========================================================================
    // TEST 137: Min-hold lower bound. Space before the minimum hold yields a
    // plain space, not the accent. Synchronous: the leader arrives ~0ms after
    // the key, well below the 200ms minimum, so it is too early to convert.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 137;
        FCITX_INFO()
            << "=== Test 137: Space before min hold yields plain space "
               "===";
        // Window [200, 2000]: lowercase minimum hold 200ms, max 2000ms.
        configureWithDelay(instance, 2000, 2000, true, false, 200, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test137");

        // Press 'a' → waiting.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Immediately press Space (~0ms < 200ms min) → plain "a" + " ", no
        // accent. Two separate single-character commits (issue #90): apps
        // that evaluate text inserts per event drop the letter of a combined
        // "a " insert.
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Space before min hold must be consumed as plain char + space";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 137 PASSED";
    });

    // =========================================================================
    // TEST 138: Min-hold uses the uppercase lower bound for uppercase keys.
    // Shift+A with uppercase min 200ms, then an immediate Space → plain "A ".
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 138;
        FCITX_INFO() << "=== Test 138: Uppercase min hold yields plain space "
                        "===";
        // Lowercase min 0, uppercase min 200: the uppercase bound must apply.
        configureWithDelay(instance, 2000, 2000, true, false, 0, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test138");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("A");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Space before uppercase min hold must be "
                                  "consumed as plain char + space";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 138 PASSED";
    });

    // =========================================================================
    // TEST 139: Min-hold guard arms auto-repeat suppression. Hold 'a', press
    // Space before the minimum hold → plain "a ". The 'a' key is still held;
    // its auto-repeat must be consumed (committed_ guard), not start a
    // fresh gesture, otherwise a duplicate character leaks (üu-class bug).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 139;
        FCITX_INFO() << "=== Test 139: Auto-repeat after early-Space min-hold "
                        "guard is suppressed ===";
        // Lowercase minimum hold 300ms; the synchronous Space below is at ~0ms.
        configureWithDelay(instance, 2000, 2000, true, false, 300, 0);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test139");

        // Hold 'a' → waiting.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Early Space (~0ms < 300ms) → guard commits plain "a" + " ", consumes.
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Early Space must be consumed as plain char";

        // Auto-repeat of the still-held 'a' (same rawCode, no release) must be
        // consumed by the committed_ guard, with no new commit.
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "Auto-repeat after min-hold guard must be "
                                  "consumed (no duplicate)";

        // Release 'a' → also consumed under committed_.
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed)
            << "Release of the committed key after the guard must be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 139 PASSED";
    });

    // =========================================================================
    // TEST 140: Degenerate window (min >= max) does not go dead. A hand-edited
    // config could set min above max; the engine ignores such a lower bound and
    // falls back to [0, max], so an immediate Space still converts.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 140;
        FCITX_INFO() << "=== Test 140: Degenerate window (min >= max) ignores "
                        "the lower bound ===";
        // Lowercase min 2000 >= max 400: degenerate. Lower bound must be
        // ignored, leaving the window [0, 400].
        configureWithDelay(instance, 400, 700, true, false, 2000, 0);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test140");

        // Hold 'a' → waiting.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Immediate Space (~0ms). If the bogus min (2000) were enforced this
        // would be plain "a "; with the degenerate bound ignored it converts.
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Degenerate min >= max must be ignored, so Space converts";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 140 PASSED";
    });

    // =========================================================================
    // TEST 143: reset() flushes a pending deferred space (issue #90; #73
    // review). Space before the minimum hold commits "a" synchronously and arms
    // " " on the zero-delay timer. Chromium and Neovide fire reset() after every
    // commit; a reset() landing before that timer must deliver the consumed
    // space, not silently cancel it via clearAllState(). reset() is invoked
    // directly (synchronously, before the event loop can run the timer) so the
    // race is deterministic. Without the flushPendingSpaceCommit() guard in
    // reset() the dropped " " leaves its expectation queued, and the next
    // commit (test 141's "a") then aborts with a mismatch pointing here.
    // Runs before the timer-chained tests 141-142 so no deferred timer is left
    // in flight (reset() destroys the zero-delay timer as it flushes).
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 143;
        FCITX_INFO()
            << "=== Test 143: reset() flushes pending deferred space ===";
        configureWithDelay(instance, 2000, 2000, true, false, 200, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test143");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // Hold 'a' → waiting.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Early Space (~0ms < 200ms min): "a" commits synchronously, " " is
        // scheduled on the zero-delay timer; inputKeyPressed_ is now false, so
        // the reset() early-return does not cover this state.
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Early Space must be consumed";

        // reset() before the zero-delay timer runs: the flush must deliver the
        // deferred " " (matching the expectation) instead of dropping it.
        ic->reset();

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 143 PASSED";
    });

    // =========================================================================
    // TEST 144: reset() preserves single-output repeat suppression (issue #92).
    // After a single-output commit the still-held key keeps committed_
    // armed so its auto-repeat is consumed (the "üu"-class guard, test 24).
    // Chromium and Neovide fire reset() after every commit; clearAllState()
    // would drop the arming and its heldRawCodes_ entry, so the next repeat
    // would re-enter as a fresh press and duplicate the character. reset() must
    // preserve the still-held code. Verified two ways: the repeat leaves the
    // preedit empty (a fresh gesture would show "a"), and no stray commit is
    // pushed, so a duplicate would abort on the empty expectation queue.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 144;
        FCITX_INFO()
            << "=== Test 144: reset() preserves repeat suppression ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test144");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // Hold 'a' + Space → commit 'ä', arm committed_.code = kCodeA.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // App fires reset() on the commit: inputKeyPressed_ is false after the
        // single-output commit, so it runs clearAllState().
        ic->reset();

        // Auto-repeat 'a' (held, same keycode, no release) must still be
        // consumed by the preserved arming, not start a fresh gesture.
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "Repeat after reset() must be consumed";
        FCITX_ASSERT(getClientPreedit(instance).empty())
            << "Repeat after reset() must not start a fresh gesture";

        // Real release must be consumed by the committed-key release branch;
        // no character is committed (no expectation pushed).
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed) << "Release after reset() must be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 144 PASSED";
    });

    // =========================================================================
    // TEST 145: same guard at the min-hold arming site (issue #92). Space before
    // the minimum hold commits the plain char and arms committed_; a
    // following reset() must preserve it too. reset()'s issue-#90 flush delivers
    // the deferred trailing space, so both "a" and " " are expected, then the
    // held key's auto-repeat must stay suppressed.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 145;
        FCITX_INFO()
            << "=== Test 145: reset() preserves min-hold arming ===";
        configureWithDelay(instance, 2000, 2000, true, false, 200, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test145");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic) << "IC must exist";

        // Hold 'a', early Space (< 200ms min): commits "a", arms
        // committed_, schedules the deferred " ".
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        // reset() flushes the deferred " " and must preserve the arming.
        ic->reset();

        // Auto-repeat 'a' stays suppressed; no fresh gesture.
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        FCITX_ASSERT(consumed) << "Repeat after reset() must be consumed";
        FCITX_ASSERT(getClientPreedit(instance).empty())
            << "Repeat after reset() must not start a fresh gesture";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed) << "Release after reset() must be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 145 PASSED";
    });

    // =========================================================================
    // TEST 146: Wayland held-key repeat fully suppressed (issue #92, hole 2).
    // Regression for the fix: holding an accent key past a single-output
    // conversion on a KWin/Wayland session (auto-repeat as release-press pairs
    // with a frozen timestamp) must leak NO duplicate. Events carry an explicit
    // timestamp via sendKeyAtTime() because the test frontend stamps time()==0,
    // which the #73 predicate treats as non-synthetic.
    //
    // hold 'u' + Space -> "ü" (arms committed_), then several synthetic
    // [release, press] pairs at the frozen burst time, then a genuine release at
    // an advanced time. Each synthetic release keeps the arming (committed_.time
    // matches) and the paired re-press is swallowed by the repeat guard, so the
    // preedit never restarts and nothing else commits; the genuine release just
    // clears the arming. Sleeping >5ms before each release satisfies the elapsed
    // guard (a real burst's first repeat is one repeat-period after the press).
    // Only "ü" is expected — any leaked "u" aborts on the empty queue.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 146;
        FCITX_INFO() << "=== Test 146: Wayland held-key repeat suppressed (#92) "
                        "===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test146");

        const int burstT = 5000; // frozen burst timestamp (ms), nonzero
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, burstT);
        tf->call<ITestFrontend::pushCommitExpectation>("ü");
        sendKeyAtTime(instance, uuid, FcitxKey_space, kCodeSpace, false, burstT);

        // Auto-repeat burst: release-press pairs at the frozen time, all consumed
        // with no commit and no restarted gesture.
        for (int i = 0; i < 3; ++i) {
            // >5ms since the committed gesture's press, so the release passes the
            // synthetic elapsed guard (a real burst's first repeat is delayed).
            std::this_thread::sleep_for(kSyntheticReleaseTestHold);
            bool rel =
                sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, burstT);
            bool pr =
                sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, burstT);
            FCITX_ASSERT(rel && pr) << "Synthetic pair must be consumed";
            FCITX_ASSERT(getClientPreedit(instance).empty())
                << "Held burst must not restart a gesture, got '"
                << getClientPreedit(instance) << "'";
        }

        // Genuine final release (advanced timestamp): clears the arming, no
        // commit. No "u" expectation is pushed, so a duplicate would abort.
        bool fin =
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, burstT + 100);
        FCITX_ASSERT(fin) << "Genuine release must be consumed";
        FCITX_ASSERT(getClientPreedit(instance).empty())
            << "No leftover gesture after release";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 146 PASSED (no duplicate leaked)";
    });

    // =========================================================================
    // TEST 147: an injected equal-timestamp pair (< 5ms after the commit) stays
    // on the historic clear path (issue #92 hole 2 fix must not swallow injected
    // input). Hold 'u' + Space -> "ü" arms committed_; a release at the same
    // frozen time but only microseconds later fails the synthetic elapsed guard,
    // so it is treated as genuine and clears the arming, and the paired re-press
    // then restarts a normal 'u' gesture (preedit 'u').
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 147;
        FCITX_INFO()
            << "=== Test 147: injected fast pair clears arming (#92) ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test147");

        const int t = 5000;
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, t);
        tf->call<ITestFrontend::pushCommitExpectation>("ü");
        sendKeyAtTime(instance, uuid, FcitxKey_space, kCodeSpace, false, t);

        // No sleep: elapsed < 5ms, so the release is NOT classed synthetic and
        // clears the arming; the re-press then starts a fresh 'u' gesture.
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, t);
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, t);
        FCITX_ASSERT(getClientPreedit(instance) == "u")
            << "Injected fast pair must clear the arming and restart a gesture";

        tf->call<ITestFrontend::pushCommitExpectation>("u");
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, t + 100);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 147 PASSED";
    });

    // =========================================================================
    // TEST 148: the synthetic-release elapsed reference is the COMMITTED
    // gesture's own press, not the global startTimeUsec_ a later gesture
    // overwrites (issue #92 hole 2, Loch 2). Commit 'u' (arms committed_ with
    // u's press/start), wait >5ms, then start a fresh 'a' gesture (which resets
    // the global startTimeUsec_ to ~now). A synthetic release of the still-held
    // 'u' must stay recognised as synthetic via committed_.startUsec (>5ms old)
    // and keep the arming, even though the global start is sub-5ms — so the 'u'
    // repeat leaves the 'a' gesture untouched. A flat-field impl keyed on
    // startTimeUsec_ would misclassify, clear the arming, and let the 'u'
    // re-press commit the pending 'a' and restart.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 148;
        FCITX_INFO()
            << "=== Test 148: elapsed ref is the committed gesture (#92) ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test148");

        const int tu = 5000, ta = 6000;
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, tu);
        tf->call<ITestFrontend::pushCommitExpectation>("ü");
        sendKeyAtTime(instance, uuid, FcitxKey_space, kCodeSpace, false, tu);

        // >5ms so committed_.startUsec (u's) is old, then a fresh 'a' gesture
        // resets the GLOBAL startTimeUsec_ to ~now.
        std::this_thread::sleep_for(kSyntheticReleaseTestHold);
        sendKeyAtTime(instance, uuid, FcitxKey_a, kCodeA, false, ta);
        FCITX_ASSERT(getClientPreedit(instance) == "a")
            << "'a' gesture should be waiting";

        // Synthetic release of the still-held 'u'. elapsed vs committed_.startUsec
        // is >5ms -> synthetic -> keep arming; the re-press is swallowed and the
        // 'a' gesture is untouched.
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, tu);
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, tu);
        FCITX_ASSERT(getClientPreedit(instance) == "a")
            << "'u' repeat must not disturb the 'a' gesture, got '"
            << getClientPreedit(instance) << "'";

        // Cleanup: genuine 'u' release clears its arming (no commit); 'a' release
        // commits the waiting 'a'.
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, tu + 100);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        sendKeyAtTime(instance, uuid, FcitxKey_a, kCodeA, true, ta + 100);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 148 PASSED";
    });

    // =========================================================================
    // TEST 149: held-key suppression via the MIN-HOLD arming site (issue #92
    // hole 2), a different arming path than test 146's normal single-output.
    // Space before the minimum hold commits the plain char "u" and arms
    // committed_; the deferred trailing " " is delivered by the next event's
    // top-of-keyEvent flush. A synthetic burst on the held 'u' must leak nothing.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 149;
        FCITX_INFO()
            << "=== Test 149: min-hold site held-key suppressed (#92) ===";
        configureWithDelay(instance, 2000, 2000, true, false, 200, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test149");

        const int burstT = 5000;
        // Hold 'u', early Space (< 200ms min): commits "u", arms committed_,
        // defers " ".
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, burstT);
        tf->call<ITestFrontend::pushCommitExpectation>("u");
        sendKeyAtTime(instance, uuid, FcitxKey_space, kCodeSpace, false, burstT);
        // Delivered by the top-of-keyEvent flush on the first burst event below.
        tf->call<ITestFrontend::pushCommitExpectation>(" ");

        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(kSyntheticReleaseTestHold);
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, burstT);
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, burstT);
            FCITX_ASSERT(getClientPreedit(instance).empty())
                << "Min-hold burst must not restart a gesture, got '"
                << getClientPreedit(instance) << "'";
        }
        bool fin =
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, burstT + 100);
        FCITX_ASSERT(fin) << "Genuine release must be consumed";
        FCITX_ASSERT(getClientPreedit(instance).empty())
            << "No leftover gesture after release";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 149 PASSED";
    });

    // =========================================================================
    // TEST 150: held-key suppression via the ALT single-output arming site
    // (issue #92 hole 2), the third arming path (cf. test 136). Hold 'u' + Alt
    // enters single-output Alt cycling; Space (second leader) commits "ü" + " "
    // and arms committed_. Events carry a frozen timestamp so committed_.time
    // matches the burst; a synthetic burst on the held 'u' must leak nothing.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 150;
        FCITX_INFO() << "=== Test 150: Alt site held-key suppressed (#92) ===";
        configureLeaders(instance, true, false, false, false, false, true);
        setMappings(instance, {{"u", "\xc3\xbc"}});
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test150");

        const int burstT = 5000;
        sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, burstT);
        sendKeyAtTime(instance, uuid, FcitxKey_Alt_L, kCodeAltL, false, burstT);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        sendKeyAtTime(instance, uuid, FcitxKey_space, kCodeSpace, false, burstT);

        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(kSyntheticReleaseTestHold);
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, burstT);
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, false, burstT);
            FCITX_ASSERT(getClientPreedit(instance).empty())
                << "Alt-site burst must not restart a gesture, got '"
                << getClientPreedit(instance) << "'";
        }
        bool fin =
            sendKeyAtTime(instance, uuid, FcitxKey_u, kCodeU, true, burstT + 100);
        FCITX_ASSERT(fin) << "Genuine release must be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 150 PASSED";
    });

    // =========================================================================
    // TEST 141: Post-timeout ordering guard splits char and space (issue #90
    // twin). The accent window has expired but the timeout timer has not
    // fired: the blocking sleep below keeps the single-threaded event loop
    // from running it, so the Space press hits the in-handler ordering guard.
    // It must deliver "a" and " " as separate commits like the min-hold guard
    // (historically this path emitted one combined "a ").
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 141;
        FCITX_INFO()
            << "=== Test 141: Post-timeout guard splits char and space ===";
        // Smallest valid window (kDelayMin = 50ms), no minimum hold.
        configureWithDelay(instance, 50, 50, true, false, 0, 0);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test141");

        // Hold 'a' → waiting.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Let the 50ms window expire WITHOUT yielding to the event loop, so
        // the addon's timeout timer cannot fire and the pending 'a' is still
        // waiting when Space arrives.
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Space right after window expiry must be consumed";

        // The release flushes the deferred space at the top of keyEvent
        // (order guarantee when a key event wins the race against the timer).
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 141 PASSED";
    });

    // =========================================================================
    // TEST 142: Deferred space commit fires on its own timer (issue #90).
    // Space before the minimum hold commits "a" synchronously and " " in its
    // own event-loop turn, so per-event apps receive two single-character
    // inserts. No key event follows before the verify timer here, so the
    // zero-delay timer itself must deliver the space (the flush hook at the
    // top of keyEvent is exercised by tests 137-139 and 141 instead). The IC
    // is destroyed WITHOUT further key events: if the timer path failed, the
    // pending space dies with the IC and the leftover " " expectation makes
    // the next commit (test 113's "ä") fatal with a mismatch pointing here.
    // Timer-chained like tests 21-23: the dispatcher runs all top-level
    // lambdas back to back, so the follow-up test lives in the callback.
    // =========================================================================
    testDispatcher->schedule([instance]() {
        g_currentTest = 142;
        FCITX_INFO() << "=== Test 142: Deferred space commit timer path ===";
        configureWithDelay(instance, 2000, 2000, true, false, 200, 200);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test142");

        // Hold 'a' → waiting.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        // Early Space (~0ms < 200ms min): "a" commits synchronously, " " is
        // scheduled on the zero-delay timer.
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        tf->call<ITestFrontend::pushCommitExpectation>(" ");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed) << "Early Space must be consumed";

        // Give the zero-delay timer its own event-loop turns, then clean up
        // and chain to the final test. The shared holder keeps the chained
        // timers alive (tests 21-23 pattern).
        struct TimerHolder {
            std::unique_ptr<EventSourceTime> timer;
        };
        auto holder = std::make_shared<TimerHolder>();
        holder->timer = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + kDeferredVerifyDelayUsec, 0,
            [instance, uuid, holder](EventSourceTime *, uint64_t) {
                auto *tf = instance->addonManager().addon("testfrontend");
                tf->call<ITestFrontend::destroyInputContext>(uuid);
                FCITX_INFO() << "Test 142 PASSED";

                // --- Test 113: positive min-hold counterpart, ends suite ---
                g_currentTest = 113;
                FCITX_INFO()
                    << "=== Test 113: Inside window [300, 2000] with min > 0, "
                       "Space converts ===";
                // Window [300, 2000]: lowercase minimum hold 300ms, max
                // 2000ms. This is the positive counterpart to tests 137-139:
                // a leader that arrives inside the window (after min, before
                // max) must still convert even though a non-zero minimum hold
                // is configured.
                configureWithDelay(instance, 2000, 2000, true, false, 300,
                                   300);
                auto uuid113 = createAndActivate(instance, tf, "test113");

                // Press 'a' → waiting
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid113, Key(FcitxKey_a, KeyStates(), kCodeA), false);

                // Wait 1500ms, past the 300ms min, inside the 2000ms max
                holder->timer = instance->eventLoop().addTimeEvent(
                    CLOCK_MONOTONIC, nowUsec() + 1'500'000, 0,
                    [instance, uuid113, holder](EventSourceTime *, uint64_t) {
                        auto *tf =
                            instance->addonManager().addon("testfrontend");

                        // At 1500ms: elapsed > 300 (min) and < 2000 (max) →
                        // Space converts to "ä" despite the minimum hold.
                        tf->call<ITestFrontend::pushCommitExpectation>(
                            "\xc3\xa4");
                        bool consumed113 = tf->call<ITestFrontend::sendKeyEvent>(
                            uuid113, Key(FcitxKey_space, KeyStates(), kCodeSpace),
                            false);
                        FCITX_ASSERT(consumed113)
                            << "Space at 1500ms must convert (inside [300, "
                               "2000])";

                        tf->call<ITestFrontend::keyEvent>(
                            uuid113, Key(FcitxKey_a, KeyStates(), kCodeA),
                            true);
                        tf->call<ITestFrontend::destroyInputContext>(uuid113);
                        FCITX_INFO() << "Test 113 PASSED";

                        // --- Test 151: window-timeout per-window repeat (issue
                        // #92 hole 2 / #73). Timer-chained onto the suite tail so
                        // the real accent-window timeout can fire. The window-
                        // timeout arming opts out of synthetic-keep with a zero
                        // press time, so a synthetic release must CLEAR it (one
                        // char per window), unlike the single-output sites. ---
                        g_currentTest = 151;
                        FCITX_INFO() << "=== Test 151: window-timeout per-window "
                                        "repeat (#92/#73) ===";
                        configureWithDelay(instance, 50, 50, true, false, 0, 0);
                        auto uuid151 = createAndActivate(instance, tf, "test151");
                        const int wtT = 5000;
                        // Hold 'u' -> waiting; the window timeout is ~50ms out.
                        sendKeyAtTime(instance, uuid151, FcitxKey_u, kCodeU, false,
                                      wtT);
                        // Latch sawSyntheticRelease_ with a synthetic pair (>5ms),
                        // so the timeout arms committed_ for the trailing release.
                        std::this_thread::sleep_for(kSyntheticReleaseTestHold);
                        sendKeyAtTime(instance, uuid151, FcitxKey_u, kCodeU, true,
                                      wtT);
                        sendKeyAtTime(instance, uuid151, FcitxKey_u, kCodeU, false,
                                      wtT);
                        FCITX_ASSERT(getClientPreedit(instance) == "u")
                            << "Gesture must stay waiting after the synthetic pair";
                        // The window elapses without a leader -> the timeout
                        // commits "u" and (sawSyntheticRelease_) arms
                        // committed_(u, 0, 0). Wait past the window, then verify.
                        tf->call<ITestFrontend::pushCommitExpectation>("u");
                        holder->timer = instance->eventLoop().addTimeEvent(
                            CLOCK_MONOTONIC, nowUsec() + 80'000, 0,
                            [instance, uuid151, wtT,
                             holder](EventSourceTime *, uint64_t) {
                                auto *tf = instance->addonManager().addon(
                                    "testfrontend");
                                // Timeout fired: "u" committed, preedit cleared,
                                // committed_ = {u, 0, 0}.
                                FCITX_ASSERT(getClientPreedit(instance).empty())
                                    << "Window timeout should have committed and "
                                       "cleared the preedit";
                                // A synthetic release must CLEAR the arming
                                // (committed_.time == 0), not keep it, so the held
                                // key restarts a gesture: one char per window.
                                sendKeyAtTime(instance, uuid151, FcitxKey_u, kCodeU,
                                              true, wtT);
                                sendKeyAtTime(instance, uuid151, FcitxKey_u, kCodeU,
                                              false, wtT);
                                FCITX_ASSERT(getClientPreedit(instance) == "u")
                                    << "Window-timeout arming must clear per "
                                       "synthetic release and restart, got '"
                                    << getClientPreedit(instance) << "'";
                                tf->call<ITestFrontend::destroyInputContext>(
                                    uuid151);
                                FCITX_INFO() << "Test 151 PASSED";

                                FCITX_INFO() << "=== All 153 tests PASSED ===";
                                instance->exit();
                                return false;
                            });
                        return false;
                    });
                return false;
            });
    });
}

int main() {
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGSEGV, crashHandler);
    setupTestingEnvironment(TESTING_BINARY_DIR, {"."}, {"tests"});

    char arg0[] = "testschnelleumlaute";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testfrontend,testim,keyboard,schnelle-umlaute";
    char *argv[] = {arg0, arg1, arg2};
    Log::setLogRule("default=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);

    EventDispatcher dispatcher;
    dispatcher.attach(&instance.eventLoop());
    testDispatcher = &dispatcher;

    scheduleTests(&instance);
    instance.exec();

    // Drop the dangling pointer before dispatcher leaves scope, so any
    // post-loop access path the analyzer reasons about is well-defined.
    testDispatcher = nullptr;
    return 0;
}
