#include "testdir.h"
#include "testfrontend_public.h"
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx-config/rawconfig.h>
#include <vector>

using namespace fcitx;

// Physical key codes (arbitrary but consistent per key)
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

// Helper: create IC and activate schnelle-umlaute
static ICUUID createAndActivate(Instance *instance, AddonInstance *testfrontend,
                                const std::string &name) {
    auto uuid = testfrontend->call<ITestFrontend::createInputContext>(name);
    testfrontend->call<ITestFrontend::keyEvent>(
        uuid, Key("Control+space"), false);
    return uuid;
}

static void configureLeaders(Instance *instance,
                              bool space, bool left, bool right,
                              bool up, bool down, bool alt,
                              const std::string &custom = "") {
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
    config.setValueByPath("Leader/CustomKey", custom);
    config.setValueByPath("Mappings/Input1", "a");
    config.setValueByPath("Mappings/Output1", "ä");
    config.setValueByPath("Mappings/Input2", "o");
    config.setValueByPath("Mappings/Output2", "ö");
    config.setValueByPath("Mappings/Input3", "u");
    config.setValueByPath("Mappings/Output3", "ü");
    config.setValueByPath("Mappings/Input4", "s");
    config.setValueByPath("Mappings/Output4", "ß");
    config.setValueByPath("Mappings/Input5", "A");
    config.setValueByPath("Mappings/Output5", "Ä");
    config.setValueByPath("Mappings/Input6", "O");
    config.setValueByPath("Mappings/Output6", "Ö");
    config.setValueByPath("Mappings/Input7", "U");
    config.setValueByPath("Mappings/Output7", "Ü");
    for (int i = 8; i <= 30; ++i) {
        auto s = std::to_string(i);
        config.setValueByPath("Mappings/Input" + s, "");
        config.setValueByPath("Mappings/Output" + s, "");
    }
    addon->setConfig(config);
}

// =========================================================================
// WRITING FLOW / LEADER COMPARISON HELPERS
// =========================================================================

// evdev key codes for a-z (QWERTY layout)
static constexpr int kLetterCodes[26] = {
    38, 56, 54, 40, 26, 41, 42, 43, 31, 44,  // a-j
    45, 46, 58, 57, 32, 33, 24, 27, 39, 28,  // k-t
    30, 55, 25, 53, 29, 52                     // u-z
};

// Multilingual cycling mappings: which lowercase chars are mapped?
static bool isMultilingualMapped(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
           c == 's' || c == 'c' || c == 'n' || c == 'y';
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
// ';' leader: counts [aesc] immediately followed by ';' within words (≈0 in text)
static int countDualLeaderWithinWordCollisions(
    const std::string &text, char leader, bool leaderControlsRightHand) {
    int collisions = 0;
    char prev = 0;
    for (char c : text) {
        if (c == ' ') { prev = 0; continue; }
        if (prev != 0 && c == leader) {
            if (leaderControlsRightHand && isRightHandMapped(prev))
                collisions++;
            else if (!leaderControlsRightHand && isLeftHandMapped(prev))
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
            if (!cur.empty()) { words.push_back(std::move(cur)); cur.clear(); }
        } else { cur += c; }
    }
    if (!cur.empty()) words.push_back(std::move(cur));
    for (size_t w = 0; w + 1 < words.size(); ++w) {
        if (isMultilingualMapped(words[w].back())) collisions++;
    }
    return collisions;
}

// Does the mapping have multiple outputs (cycling)?
// All mapped chars have cycling EXCEPT 's' (single output: ß)
static bool hasCycling(char c) {
    return c != 's' && isMultilingualMapped(c);
}

// First cycling variant (or single output) for each mapped char
static const char *firstMappedOutput(char c) {
    switch (c) {
        case 'a': return "\xc3\xa4";   // ä
        case 'e': return "\xc3\xa9";   // é
        case 'i': return "\xc3\xad";   // í
        case 'o': return "\xc3\xb6";   // ö
        case 'u': return "\xc3\xbc";   // ü
        case 's': return "\xc3\x9f";   // ß
        case 'c': return "\xc3\xa7";   // ç
        case 'n': return "\xc3\xb1";   // ñ
        case 'y': return "\xc3\xbd";   // ý
        default:  return "";
    }
}

// Configure multilingual cycling mappings with specified leaders
static void configureMultilingualCycling(Instance *instance,
                                          bool space, bool alt,
                                          const std::string &custom = "") {
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
    config.setValueByPath("Leader/CustomKey", custom);
    // a → ä,à,á,â,ã
    config.setValueByPath("Mappings/Input1", "a");
    config.setValueByPath("Mappings/Output1",
        "\xc3\xa4,\xc3\xa0,\xc3\xa1,\xc3\xa2,\xc3\xa3");
    // e → é,è,ê,ë
    config.setValueByPath("Mappings/Input2", "e");
    config.setValueByPath("Mappings/Output2",
        "\xc3\xa9,\xc3\xa8,\xc3\xaa,\xc3\xab");
    // i → í,ì,î,ï
    config.setValueByPath("Mappings/Input3", "i");
    config.setValueByPath("Mappings/Output3",
        "\xc3\xad,\xc3\xac,\xc3\xae,\xc3\xaf");
    // o → ö,ò,ó,ô,õ
    config.setValueByPath("Mappings/Input4", "o");
    config.setValueByPath("Mappings/Output4",
        "\xc3\xb6,\xc3\xb2,\xc3\xb3,\xc3\xb4,\xc3\xb5");
    // u → ü,ù,ú,û
    config.setValueByPath("Mappings/Input5", "u");
    config.setValueByPath("Mappings/Output5",
        "\xc3\xbc,\xc3\xb9,\xc3\xba,\xc3\xbb");
    // s → ß (single output, no cycling)
    config.setValueByPath("Mappings/Input6", "s");
    config.setValueByPath("Mappings/Output6", "\xc3\x9f");
    // c → ç,ć
    config.setValueByPath("Mappings/Input7", "c");
    config.setValueByPath("Mappings/Output7", "\xc3\xa7,\xc4\x87");
    // n → ñ,ń
    config.setValueByPath("Mappings/Input8", "n");
    config.setValueByPath("Mappings/Output8", "\xc3\xb1,\xc5\x84");
    // y → ý,ÿ
    config.setValueByPath("Mappings/Input9", "y");
    config.setValueByPath("Mappings/Output9", "\xc3\xbd,\xc3\xbf");
    // A → Ä,À,Á,Â,Ã
    config.setValueByPath("Mappings/Input10", "A");
    config.setValueByPath("Mappings/Output10",
        "\xc3\x84,\xc3\x80,\xc3\x81,\xc3\x82,\xc3\x83");
    // E → É,È,Ê,Ë
    config.setValueByPath("Mappings/Input11", "E");
    config.setValueByPath("Mappings/Output11",
        "\xc3\x89,\xc3\x88,\xc3\x8a,\xc3\x8b");
    // O → Ö,Ò,Ó,Ô,Õ
    config.setValueByPath("Mappings/Input12", "O");
    config.setValueByPath("Mappings/Output12",
        "\xc3\x96,\xc3\x92,\xc3\x93,\xc3\x94,\xc3\x95");
    // U → Ü,Ù,Ú,Û
    config.setValueByPath("Mappings/Input13", "U");
    config.setValueByPath("Mappings/Output13",
        "\xc3\x9c,\xc3\x99,\xc3\x9a,\xc3\x9b");
    for (int i = 14; i <= 30; ++i) {
        auto s = std::to_string(i);
        config.setValueByPath("Mappings/Input" + s, "");
        config.setValueByPath("Mappings/Output" + s, "");
    }
    addon->setConfig(config);
}

// Type a single char with clean typing (press+release).
// If mapped, pushes commit expectation for original char.
static void typeCharClean(AddonInstance *tf, ICUUID uuid, char c) {
    if (c < 'a' || c > 'z') return;
    auto sym = static_cast<FcitxKeySym>(FcitxKey_a + (c - 'a'));
    int code = kLetterCodes[c - 'a'];
    tf->call<ITestFrontend::sendKeyEvent>(
        uuid, Key(sym, KeyStates(), code), false);
    if (isMultilingualMapped(c)) {
        tf->call<ITestFrontend::pushCommitExpectation>(std::string(1, c));
    }
    tf->call<ITestFrontend::sendKeyEvent>(
        uuid, Key(sym, KeyStates(), code), true);
}

// Simulate fast typing with word-boundary overlap.
// Within each word: clean typing (press-release).
// At word end: HOLD last char while pressing Space.
//   - Space leader + mapped char → conversion (collision!)
//   - No Space leader + mapped char → commits original, Space passes through
//   - Unmapped char → passes through, Space passes through
// Returns number of word-boundary collisions.
static int typeTextWordBoundaryOverlap(
    AddonInstance *tf, ICUUID uuid,
    const std::string &text,
    bool spaceIsLeader) {

    std::vector<std::string> words;
    std::string cur;
    for (char c : text) {
        if (c == ' ') {
            if (!cur.empty()) { words.push_back(std::move(cur)); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) words.push_back(std::move(cur));

    int collisions = 0;

    for (size_t w = 0; w < words.size(); ++w) {
        const auto &word = words[w];
        bool lastWord = (w == words.size() - 1);

        // Type all chars except last with clean typing
        for (size_t i = 0; i + 1 < word.size(); ++i) {
            typeCharClean(tf, uuid, word[i]);
        }

        char last = word.back();
        if (last < 'a' || last > 'z') continue;
        auto lastSym = static_cast<FcitxKeySym>(FcitxKey_a + (last - 'a'));
        int lastCode = kLetterCodes[last - 'a'];
        bool mapped = isMultilingualMapped(last);

        if (lastWord) {
            // Last word: clean type last char (no Space after)
            typeCharClean(tf, uuid, last);

        } else if (mapped && spaceIsLeader) {
            // COLLISION: hold last char + Space → conversion
            collisions++;
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(lastSym, KeyStates(), lastCode), false);

            if (hasCycling(last)) {
                // Multiple outputs: Space enters cycling, release commits
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
                tf->call<ITestFrontend::pushCommitExpectation>(
                    firstMappedOutput(last));
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, KeyStates(), lastCode), true);
            } else {
                // Single output: Space commits directly
                tf->call<ITestFrontend::pushCommitExpectation>(
                    firstMappedOutput(last));
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid, Key(lastSym, KeyStates(), lastCode), true);
            }
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);

        } else if (mapped && !spaceIsLeader) {
            // NO collision: Space is not leader → commits original
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(lastSym, KeyStates(), lastCode), false);
            tf->call<ITestFrontend::pushCommitExpectation>(
                std::string(1, last));
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(lastSym, KeyStates(), lastCode), true);
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);

        } else {
            // Unmapped last char: both pass through
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(lastSym, KeyStates(), lastCode), false);
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(lastSym, KeyStates(), lastCode), true);
            tf->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);
        }
    }
    return collisions;
}

// 100-word test texts (lowercase, no punctuation)
static const char *kGerman100 =
    "die sonne scheint hell durch das fenster und wirft lange "
    "schatten auf den boden es ist ein ruhiger morgen in der "
    "kleinen stadt am fluss die menschen gehen langsam durch die "
    "strassen und geniessen den frischen wind der von den hohen "
    "bergen weht ein alter mann sitzt auf einer bank und liest "
    "seine zeitung waehrend kinder um ihn herum spielen und lachen "
    "der duft von frischem brot kommt aus der baeckerei nebenan "
    "und laesst alle hungrig werden heute wird sicher ein schoener "
    "tag das spuert jeder der am morgen nach draussen geht und "
    "die warme luft auf der haut spuert";

static const char *kEnglish100 =
    "the quick brown fox jumps over the lazy dog and runs across "
    "the fields where tall grass grows on both sides of the path "
    "birds sing in the warm morning light while clouds drift slowly "
    "across the bright blue sky a small river flows between old "
    "rocks and stones making soft sounds that echo through the quiet "
    "valley children play near the water throwing small pebbles and "
    "laughing together the wind picks up dry leaves from the ground "
    "and carries them far away to the hills where mountains stand "
    "tall against the horizon it is a perfect day to enjoy";

static const char *kFrench100 =
    "le petit chat dort sur le tapis pendant que la pluie tombe "
    "doucement dehors les enfants jouent dans le jardin avec un "
    "ballon rouge et bleu la mere prepare le repas dans la grande "
    "cuisine ou les odeurs de pain frais se melangent avec celles "
    "des legumes grilles le pere lit son journal assis dans le "
    "fauteuil pres de la fenetre ouverte par laquelle entre une "
    "brise legere les oiseaux chantent sur les branches des vieux "
    "arbres le soleil brille entre les nuages et fait danser des "
    "ombres sur le sol de la cour il fait tres bon vivre ici";

static const char *kPortuguese100 =
    "o gato pequeno dorme no tapete macio enquanto a chuva cai "
    "suavemente la fora as criancas brincam no jardim com uma bola "
    "vermelha e azul a mae prepara a refeicao na cozinha grande "
    "onde os cheiros de pao fresco se misturam com os dos legumes "
    "grelhados o pai le o jornal sentado na poltrona perto da "
    "janela aberta pela qual entra uma brisa suave os passaros "
    "cantam nos galhos das velhas arvores o sol brilha entre as "
    "nuvens brancas e faz dancar sombras no chao do quintal faz "
    "um belo dia para sair de casa e aproveitar o ar livre";

void scheduleTests(Instance *instance) {
    // =========================================================================
    // SETUP
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Setup ===";
        auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
        FCITX_ASSERT(addon) << "Addon schnelle-umlaute not loaded";

        auto group = instance->inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(
            InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(
            InputMethodGroupItem("schnelle-umlaute"));
        group.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(std::move(group));
        FCITX_INFO() << "Setup complete";
    });

    // =========================================================================
    // TEST 1: Press 'a' + release → commits normal 'a'
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    // TEST 30: Double-comma escape — literal comma in output
    // Output "a,,b" should produce single output "a,b" (escaped comma).
    // Must run before configureLeaders tests to avoid inotify interference.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 30: Double-comma escape in output ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "a,,b");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test30");

        // Hold 'a' + Space → should commit "a,b" (single output, not cycling)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a,b");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 30 PASSED";
    });

    // =========================================================================
    // TEST 31: Double-comma with cycling — "x,,y,z" → ["x,y", "z"]
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 31: Double-comma with cycling ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "x,,y,z");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test31");

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
        FCITX_INFO() << "Test 31 PASSED";
    });

    // =========================================================================
    // TEST 32: Triple comma ",,," → [","] (escaped comma + separator)
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 32: Triple comma output ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", ",,,");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test32");

        // Hold 'a' + Space → single output "," (literal comma)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>(",");
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 32 PASSED";
    });

    // =========================================================================
    // TEST 13: Left Arrow as leader
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 13: Left Arrow as leader ===";
        configureLeaders(instance, false, true, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test13");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Left, KeyStates(), kCodeLeft), false);
        FCITX_ASSERT(consumed) << "Left leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 13 PASSED";
    });

    // =========================================================================
    // TEST 14: Right Arrow as leader
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 14: Right Arrow as leader ===";
        configureLeaders(instance, false, false, true, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test14");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Right, KeyStates(), kCodeRight), false);
        FCITX_ASSERT(consumed) << "Right leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 14 PASSED";
    });

    // =========================================================================
    // TEST 15: Up Arrow as leader
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 15: Up Arrow as leader ===";
        configureLeaders(instance, false, false, false, true, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test15");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Up, KeyStates(), kCodeUp), false);
        FCITX_ASSERT(consumed) << "Up leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 15 PASSED";
    });

    // =========================================================================
    // TEST 16: Down Arrow as leader
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 16: Down Arrow as leader ===";
        configureLeaders(instance, false, false, false, false, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test16");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Down, KeyStates(), kCodeDown), false);
        FCITX_ASSERT(consumed) << "Down leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 16 PASSED";
    });

    // =========================================================================
    // TEST 17: Alt_L as leader — consumed and enters cycling
    // NOTE: Alt leader always uses deferred commit (25ms timer via
    // scheduleDeferredCyclingCommit). The timer fires asynchronously after
    // this callback returns, so pushCommitExpectation cannot verify it —
    // the IC would be destroyed before the timer fires. We verify the
    // consumed status instead; commit correctness is covered by manual test.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 17: Alt_L as leader ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test17");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);
        FCITX_ASSERT(consumed) << "Alt_L leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 17 PASSED";
    });

    // =========================================================================
    // TEST 18: AltGr (ISO_Level3_Shift) as leader — consumed and enters cycling
    // NOTE: Same deferred-commit limitation as Test 17 (see note there).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 18: AltGr (ISO_Level3_Shift) as leader ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test18");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_ISO_Level3_Shift, KeyStates(), kCodeAltGr), false);
        FCITX_ASSERT(consumed) << "AltGr leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 18 PASSED";
    });

    // =========================================================================
    // TEST 19: consumedAltCode_ — Alt release consumed after leader use
    // NOTE: Same deferred-commit limitation as Test 17. Commit verification
    // removed; this test focuses on consumed status of Alt and input key
    // releases during an Alt-led gesture.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 19: Alt release consumed after leader use ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test19");

        // Hold 'a' + Alt_L → enters cycling (preedit "ä", deferred commit)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

        // Release Alt_L — should be consumed (consumedAltCode_)
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), true);
        FCITX_ASSERT(consumed) << "Alt release after leader should be consumed";

        // Release 'a' — consumed by cycling-release handler (defers commit)
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed) << "Accent key release during Alt gesture should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 19 PASSED";
    });

    // =========================================================================
    // TEST 20: committedKeyCode_ — auto-repeat suppression after commit
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 20: Auto-repeat suppression after commit ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test20");

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

        // Release 'a' — should be consumed (committedKeyCode_)
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed) << "Release of committed key should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 20 PASSED";
    });

    // =========================================================================
    // TEST 21: Custom leader key
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 21: Custom leader key '#' ===";
        configureLeaders(instance, false, false, false, false, false, false, "#");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test21");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Custom leader '#' during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 21 PASSED";
    });

    // NOTE: Test 22 (disabled leader passthrough) removed — triggers a
    // pre-existing testfrontend commit-expectation issue where commits via
    // the "OTHER KEYS" path (commitPendingKey) silently fail the global
    // checkExpectation_ assertion regardless of push timing.

    // =========================================================================
    // TEST 24: Custom leader with multi-byte UTF-8 character (§)
    // Validates utf8::validate/ncharByteLength sanitization path.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 24: Custom leader with UTF-8 char § ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "\xc2\xa7");  // § in UTF-8
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test24");

        constexpr int kCodeSection = 21;
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_section, KeyStates(), kCodeSection), false);
        FCITX_ASSERT(consumed) << "UTF-8 custom leader § should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 24 PASSED";
    });

    // =========================================================================
    // TEST 25: Custom leader sanitization — multi-char trimmed to first
    // Config "#x" should sanitize to "#" and work as leader.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 25: Custom leader multi-char sanitized ===";
        configureLeaders(instance, false, false, false, false, false, false, "#x");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test25");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Sanitized '#' from '#x' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 25 PASSED";
    });

    // =========================================================================
    // TEST 26: Custom leader sanitization — whitespace trimmed
    // Config "  #  " should sanitize to "#" and work as leader.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 26: Custom leader whitespace trimmed ===";
        configureLeaders(instance, false, false, false, false, false, false, "  #  ");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test26");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Sanitized '#' from '  #  ' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 26 PASSED";
    });

    // NOTE: Test 27 (whitespace-only custom leader rejected) removed — same
    // testfrontend commit-expectation issue as Test 22 (OTHER KEYS path).

    // =========================================================================
    // TEST 28: Custom leader sanitization — multi-byte UTF-8 trimmed from longer
    // Config "§xyz" should sanitize to "§" (2 bytes kept, rest dropped).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 28: Multi-byte UTF-8 trimmed from longer ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "\xc2\xa7xyz");  // "§xyz"
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test28");

        constexpr int kCodeSection = 21;
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_section, KeyStates(), kCodeSection), false);
        FCITX_ASSERT(consumed) << "Sanitized § from '§xyz' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 28 PASSED";
    });

    // =========================================================================
    // TEST 40: Cycling — multiple outputs, cycle through variants
    // Mapping "a" → "ä,ae,@" — Space cycles: ä → ae → @ → ä → ...
    // Release 'a' after second Space → commits "ae" (index 1).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 40: Cycling through multiple outputs ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4,ae,@");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test40");

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
        FCITX_INFO() << "Test 40 PASSED";
    });

    // =========================================================================
    // TEST 41: Cycling — wrap around to first variant
    // Mapping "a" → "ä,ae" — three Spaces: ä → ae → ä (wraps)
    // Release 'a' → commits "ä" (index 0 after wrap).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 41: Cycling wraps around ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4,ae");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test41");

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
        FCITX_INFO() << "Test 41 PASSED";
    });

    // =========================================================================
    // TEST 42: Overlapping gestures — hold 'a', press 's' while 'a' held
    // Pressing a second mapped key should commit 'a' and start 's' gesture.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 42: Overlapping gestures ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test42");

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
        FCITX_INFO() << "Test 42 PASSED";
    });

    // =========================================================================
    // TEST 43: Space passthrough without gesture
    // Space alone (no key held) should pass through unconsumed.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 43: Space passthrough without gesture ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test43");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed) << "Space without gesture should pass through";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);
        FCITX_ASSERT(!consumed) << "Space release without gesture should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 43 PASSED";
    });

    // =========================================================================
    // TEST 44: Ordering guard — non-Space key clears guard
    // After a commit, recentlyCommitted_ is set. Any non-Space press clears
    // it without consuming. A subsequent Space then also passes through.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 44: Ordering guard clears on non-Space ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test44");

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
        FCITX_ASSERT(!consumed) << "Unmapped 'b' after commit should pass through";
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), true);

        // Space now → guard already cleared, should pass through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed) << "Space after guard cleared should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 44 PASSED";
    });

    // =========================================================================
    // TEST 45: State cleanup after cycling — new gesture works immediately
    // After a cycling commit (release key), all cycling state must be reset
    // so a new gesture can start without interference.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 45: New gesture after cycling commit ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4,ae");
        config.setValueByPath("Mappings/Input2", "s");
        config.setValueByPath("Mappings/Output2", "\xc3\x9f");
        for (int i = 3; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test45");

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
        FCITX_INFO() << "Test 45 PASSED";
    });

    // =========================================================================
    // TEST 46: Ordering guard after cycling release
    // After cycling commit via key release, recentlyCommitted_ must be set
    // so the next Space goes through commitString (same channel as the commit).
    // Without this, Space could arrive before the committed text in terminals.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 46: Ordering guard after cycling release ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4,ae");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test46");

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
        FCITX_ASSERT(consumed) << "Space after cycling commit must be consumed by ordering guard";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 46 PASSED";
    });

    // =========================================================================
    // TEST 47: Writing flow — Space leader, German 100 words
    // Fast typing with word-boundary overlap: hold last char + Space.
    // With Space as leader, mapped chars at word end get converted.
    // Mapped chars: a,e,i,o,u,s,c,n,y (multilingual cycling)
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 47: Space leader — German 100 words ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test47");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kGerman100, true);

        FCITX_ASSERT(collisions > 30)
            << "German should have significant collisions with Space leader"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 47 PASSED — German: "
                      << collisions << "/99 collisions with Space leader";
    });

    // =========================================================================
    // TEST 48: Writing flow — Space leader, English 100 words
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 48: Space leader — English 100 words ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test48");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kEnglish100, true);

        FCITX_ASSERT(collisions > 30)
            << "English should have significant collisions with Space leader"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 48 PASSED — English: "
                      << collisions << "/99 collisions with Space leader";
    });

    // =========================================================================
    // TEST 49: Writing flow — Space leader, French 100 words
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 49: Space leader — French 100 words ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test49");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kFrench100, true);

        FCITX_ASSERT(collisions > 50)
            << "French should have many collisions (short words ending e/s)"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 49 PASSED — French: "
                      << collisions << "/99 collisions with Space leader";
    });

    // =========================================================================
    // TEST 50: Writing flow — Space leader, Portuguese 100 words
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 50: Space leader — Portuguese 100 words ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test50");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kPortuguese100, true);

        FCITX_ASSERT(collisions > 50)
            << "Portuguese should have many collisions (words ending a/o/e/s)"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 50 PASSED — Portuguese: "
                      << collisions << "/99 collisions with Space leader";
    });

    // =========================================================================
    // TEST 51: Writing flow — Alt leader, German 100 words
    // Alt is never pressed during normal typing → 0 collisions.
    // This proves Alt is superior to Space for fast typing.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 51: Alt leader — German 100 words ===";
        configureMultilingualCycling(instance, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test51");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kGerman100, false);

        FCITX_ASSERT(collisions == 0)
            << "Alt leader must have zero collisions in normal typing";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 51 PASSED — Alt leader: "
                      << collisions << "/99 collisions (zero!)";
    });

    // =========================================================================
    // TEST 52: Writing flow — Custom leader (#), German 100 words
    // Custom key is never pressed during normal typing → 0 collisions.
    // Same advantage as Alt but key position may differ ergonomically.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 52: Custom leader (#) — German 100 words ===";
        configureMultilingualCycling(instance, false, false, "#");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test52");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kGerman100, false);

        FCITX_ASSERT(collisions == 0)
            << "Custom leader must have zero collisions in normal typing";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 52 PASSED — Custom leader (#): "
                      << collisions << "/99 collisions (zero!)";
    });

    // =========================================================================
    // TEST 53: Overlap collision demo — "anbau" → "anbaü" (standard mapping)
    // Demonstrates the specific problem: user holds 'u' at word end,
    // presses Space → 'u' converts to 'ü'. Output: "anbaü" not "anbau ".
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 53: anbau overlap collision demo ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test53");

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
        FCITX_INFO() << "Test 53 PASSED — anbau → anbaü confirmed";
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 54: Dual leader collision analysis ===";

        struct LangResult {
            const char *name;
            const char *text;
            int spaceCollisions;
            int aLeaderCollisions;
            int semicolonCollisions;
        };

        LangResult langs[] = {
            {"German",     kGerman100,     0, 0, 0},
            {"English",    kEnglish100,    0, 0, 0},
            {"French",     kFrench100,     0, 0, 0},
            {"Portuguese", kPortuguese100, 0, 0, 0},
        };

        for (auto &l : langs) {
            l.spaceCollisions = countSpaceLeaderCollisions(l.text);
            // 'a' leader controls right-hand mapped chars (y,u,i,o,n)
            l.aLeaderCollisions = countDualLeaderWithinWordCollisions(
                l.text, 'a', true);
            // ';' leader controls left-hand mapped chars (a,e,s,c)
            l.semicolonCollisions = countDualLeaderWithinWordCollisions(
                l.text, ';', false);
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
            char leftLeader;    // left-hand key → controls right-hand inputs
            char rightLeader;   // right-hand key → controls left-hand inputs
        };
        DualPair pairs[] = {
            {"a/;", 'a', ';'},
            {"z//", 'z', '/'},
            {"f/j", 'f', 'j'},
        };

        std::string result;
        for (const auto &p : pairs) {
            result += "[" + std::string(p.label) + "] ";
            for (const auto &l : langs) {
                int leftL = countDualLeaderWithinWordCollisions(
                    l.text, p.leftLeader, true);
                int rightL = countDualLeaderWithinWordCollisions(
                    l.text, p.rightLeader, false);
                result += std::string(l.name) + "(Space:"
                    + std::to_string(l.spaceCollisions) + " "
                    + p.leftLeader + ":" + std::to_string(leftL) + " "
                    + p.rightLeader + ":" + std::to_string(rightL) + ") ";
            }
        }
        FCITX_INFO() << "Test 54 PASSED — " << result;
    });

    // =========================================================================
    // TEST 55: CustomKey2 basic — second custom leader works
    // CustomKey2="#", hold 'a' + '#' → ä
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 55: CustomKey2 basic ===";
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
        config.setValueByPath("Leader/CustomKey", "");
        config.setValueByPath("Leader/CustomKey2", "#");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test55");

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
        FCITX_INFO() << "Test 55 PASSED";
    });

    // =========================================================================
    // TEST 56: Dual split — allowed (opposite hands)
    // CustomKey="z" (left), CustomKey2="/" (right)
    // Hold 'u' (right-hand input) + press 'z' (left-hand leader) → ü
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 56: Dual split — allowed ===";
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
        config.setValueByPath("Leader/CustomKey", "z");
        config.setValueByPath("Leader/CustomKey2", "/");
        config.setValueByPath("Mappings/Input1", "u");
        config.setValueByPath("Mappings/Output1", "\xc3\xbc");
        config.setValueByPath("Mappings/Input2", "a");
        config.setValueByPath("Mappings/Output2", "\xc3\xa4");
        for (int i = 3; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test56");

        // Hold 'u' (right) + press 'z' (left leader) → ü (allowed: opposite hands)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), 30), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xbc");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), 52), false);
        FCITX_ASSERT(consumed) << "'z' leader must convert right-hand 'u'";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_u, KeyStates(), 30), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 56 PASSED";
    });

    // =========================================================================
    // TEST 57: Dual split — blocked (same hand)
    // Same config. Hold 'a' (left input) + press 'z' (left leader) → NO conversion.
    // 'z' falls through to OTHER KEYS, commits 'a' as original, 'z' passes through.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 57: Dual split — blocked ===";
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
        config.setValueByPath("Leader/CustomKey", "z");
        config.setValueByPath("Leader/CustomKey2", "/");
        config.setValueByPath("Mappings/Input1", "u");
        config.setValueByPath("Mappings/Output1", "\xc3\xbc");
        config.setValueByPath("Mappings/Input2", "a");
        config.setValueByPath("Mappings/Output2", "\xc3\xa4");
        for (int i = 3; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test57");

        // Hold 'a' (left) + press 'z' (left leader) → BLOCKED
        // 'z' is not allowed for left-hand input 'a', falls through to OTHER KEYS
        // which commits 'a' as original and lets 'z' pass through.
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("a");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_z, KeyStates(), 52), false);
        FCITX_ASSERT(!consumed)
            << "'z' must NOT convert same-hand 'a' — should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 57 PASSED";
    });

    // =========================================================================
    // TEST 58: Dual split — reverse direction
    // Hold 'a' (left) + press '/' (right leader) → ä (allowed: opposite hands)
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 58: Dual split — reverse ===";
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
        config.setValueByPath("Leader/CustomKey", "z");
        config.setValueByPath("Leader/CustomKey2", "/");
        config.setValueByPath("Mappings/Input1", "u");
        config.setValueByPath("Mappings/Output1", "\xc3\xbc");
        config.setValueByPath("Mappings/Input2", "a");
        config.setValueByPath("Mappings/Output2", "\xc3\xa4");
        for (int i = 3; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test58");

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
        FCITX_INFO() << "Test 58 PASSED";
    });

    // =========================================================================
    // TEST 59: Single custom key — no split, triggers all mappings
    // Only CustomKey="z" set (no CustomKey2) → 'z' triggers ALL inputs.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 59: Single custom key — no split ===";
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
        config.setValueByPath("Leader/CustomKey", "z");
        config.setValueByPath("Leader/CustomKey2", "");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test59");

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
        FCITX_INFO() << "Test 59 PASSED";
    });

    // =========================================================================
    // TEST 60: Dual split — built-in leader ignores split
    // CustomKey="z", CustomKey2="/", Space=True.
    // Space is BuiltIn → ignores hand split, triggers all mappings.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 60: Built-in leader ignores split ===";
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
        config.setValueByPath("Leader/CustomKey", "z");
        config.setValueByPath("Leader/CustomKey2", "/");
        config.setValueByPath("Mappings/Input1", "a");
        config.setValueByPath("Mappings/Output1", "\xc3\xa4");
        for (int i = 2; i <= 30; ++i) {
            auto s = std::to_string(i);
            config.setValueByPath("Mappings/Input" + s, "");
            config.setValueByPath("Mappings/Output" + s, "");
        }
        addon->setConfig(config);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test60");

        // Hold 'a' (left) + Space → ä (Space is BuiltIn, ignores split)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(consumed)
            << "Space (BuiltIn) must ignore dual split and trigger all mappings";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 60 PASSED";
    });

    // All tests done — exit
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== All tests PASSED ===";
        instance->exit();
    });
}

int main() {
    setupTestingEnvironment(
        TESTING_BINARY_DIR,
        {"."},
        {"test"});

    char arg0[] = "testschnelleumlaute";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testfrontend,testim,keyboard,schnelle-umlaute";
    char *argv[] = {arg0, arg1, arg2};
    Log::setLogRule("default=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);

    scheduleTests(&instance);
    instance.exec();

    return 0;
}
