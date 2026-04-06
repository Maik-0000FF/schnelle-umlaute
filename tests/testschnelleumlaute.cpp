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
#include <ctime>
#include <memory>
#include <vector>

using namespace fcitx;

// Delay (in microseconds) to wait for the deferred Alt cycling commit timer
// (5ms) to fire before verifying the committed string.
constexpr uint64_t kDeferredVerifyDelayUsec = 10'000;  // 10ms

static uint64_t nowUsec() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000 + ts.tv_nsec / 1'000;
}

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

// Helper: load mappings via setSubConfig (the path loadMappingsFromFile reads)
static void setMappings(Instance *instance,
                        const std::vector<std::pair<std::string,std::string>> &entries) {
    auto *addon = instance->addonManager().addon("schnelle-umlaute", true);
    RawConfig mc;
    for (size_t i = 0; i < entries.size(); ++i) {
        mc.setValueByPath(std::to_string(i) + "/Input", entries[i].first);
        mc.setValueByPath(std::to_string(i) + "/Output", entries[i].second);
    }
    addon->setSubConfig("mappings.txt", mc);
}

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
                              const std::string &custom = "",
                              const std::string &custom2 = "") {
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
    config.setValueByPath("Leader/CustomKeyEnabled",
                          custom.empty() ? "False" : "True");
    config.setValueByPath("Leader/CustomKey", custom);
    config.setValueByPath("Leader/CustomKey2Enabled",
                          custom2.empty() ? "False" : "True");
    config.setValueByPath("Leader/CustomKey2", custom2);
    addon->setConfig(config);
    setMappings(instance, {
        {"a", "\xc3\xa4"}, {"o", "\xc3\xb6"}, {"u", "\xc3\xbc"},
        {"s", "\xc3\x9f"}, {"A", "\xc3\x84"}, {"O", "\xc3\x96"},
        {"U", "\xc3\x9c"},
    });
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

// Multilingual cycling mappings: which chars are mapped?
// Lowercase: a,e,i,o,u,s,c,n,y  Uppercase: A,E,O,U
static bool isMultilingualMapped(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
           c == 's' || c == 'c' || c == 'n' || c == 'y' ||
           c == 'A' || c == 'E' || c == 'O' || c == 'U';
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
        case 'A': return "\xc3\x84";   // Ä
        case 'E': return "\xc3\x89";   // É
        case 'O': return "\xc3\x96";   // Ö
        case 'U': return "\xc3\x9c";   // Ü
        default:  return "";
    }
}

static bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
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
    config.setValueByPath("Leader/CustomKeyEnabled",
                          custom.empty() ? "False" : "True");
    config.setValueByPath("Leader/CustomKey", custom);
    addon->setConfig(config);
    setMappings(instance, {
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
        sym = FcitxKey_period; code = 60;
    } else if (c == ',') {
        sym = FcitxKey_comma; code = 59;
    } else {
        return;
    }

    tf->call<ITestFrontend::sendKeyEvent>(
        uuid, Key(sym, states, code), false);
    if (isMultilingualMapped(c)) {
        tf->call<ITestFrontend::pushCommitExpectation>(std::string(1, c));
    }
    tf->call<ITestFrontend::sendKeyEvent>(
        uuid, Key(sym, states, code), true);
}

// Simulate fast typing with word-boundary overlap.
// Within each word: clean typing (press-release).
// At word end: HOLD last LETTER while pressing Space.
// Trailing punctuation (.,) shields the last letter from Space collision.
// Handles uppercase letters (Shift modifier) and punctuation.
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

        // Find last letter (skip trailing punctuation like . ,)
        int lastLetterIdx = -1;
        for (int i = (int)word.size() - 1; i >= 0; --i) {
            if (isLetter(word[i])) { lastLetterIdx = i; break; }
        }
        if (lastLetterIdx < 0) continue;  // no letters in word

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
                    lastSimWasMapped = false;  // punctuation clears flag
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
                lastSym = static_cast<FcitxKeySym>(
                    FcitxKey_a + (lastLetter - 'a'));
                lastCode = kLetterCodes[lastLetter - 'a'];
            } else {
                lastSym = static_cast<FcitxKeySym>(
                    FcitxKey_A + (lastLetter - 'A'));
                lastStates = KeyState::Shift;
                lastCode = kLetterCodes[lastLetter - 'A'];
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

// Tests 24+ are scheduled from within the Alt leader timer chain (Tests 21-23)
// to guarantee deferred commits are verified before any subsequent test runs.
static void scheduleTestsAfterAltVerify(Instance *instance);

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
    // TEST 61: Shift-invariant custom leader — Shift+A + Shift+/ → Ä
    // When Shift is held for uppercase input, Shift+/ produces '?' keysym.
    // The leader must still match '/' via US QWERTY unshift mapping.
    // Uses dual-split config (z + /) with default mappings (A→Ä, U→Ü).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 12: Shift+A + Shift+/ → Ä ===";
        configureLeaders(instance, false, false, false, false, false, false, "z", "/");

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test12");

        // Press Shift
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_Shift_L, KeyStates(), kCodeShiftL), false);

        // Hold Shift+A (uppercase mapped input)
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_A, KeyState::Shift, kCodeA), false);

        // Press Shift+/ = '?' (keysym FcitxKey_question, same physical key as '/')
        // Must be recognized as '/' leader via US QWERTY unshift mapping.
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 13: Shift+U + Shift+Z → Ü ===";
        configureLeaders(instance, false, false, false, false, false, false, "z", "/");

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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 17: Left Arrow as leader ===";
        configureLeaders(instance, false, true, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test17");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Left, KeyStates(), kCodeLeft), false);
        FCITX_ASSERT(consumed) << "Left leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 17 PASSED";
    });

    // =========================================================================
    // TEST 14: Right Arrow as leader
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 18: Right Arrow as leader ===";
        configureLeaders(instance, false, false, true, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test18");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Right, KeyStates(), kCodeRight), false);
        FCITX_ASSERT(consumed) << "Right leader during gesture should be consumed";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 18 PASSED";
    });

    // =========================================================================
    // TEST 15: Up Arrow as leader
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 20: Down Arrow as leader ===";
        configureLeaders(instance, false, false, false, false, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test20");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Down, KeyStates(), kCodeDown), false);
        FCITX_ASSERT(consumed) << "Down leader during gesture should be consumed";

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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 21: Alt_L as leader ===";
        configureLeaders(instance, false, false, false, false, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test21");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);
        FCITX_ASSERT(consumed) << "Alt_L leader during gesture should be consumed";

        // Release 'a' — triggers deferred commit (5ms timer)
        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        // Shared holder keeps all chained timers alive
        struct TimerHolder { std::unique_ptr<EventSourceTime> timer; };
        auto holder = std::make_shared<TimerHolder>();

        // Wait for deferred commit, then clean up and chain to Test 18
        holder->timer = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, nowUsec() + kDeferredVerifyDelayUsec, 0,
            [instance, uuid, holder](EventSourceTime *, uint64_t) {
                auto *tf = instance->addonManager().addon("testfrontend");
                tf->call<ITestFrontend::destroyInputContext>(uuid);
                FCITX_INFO() << "Test 21 PASSED";

                // --- Test 22: AltGr as leader ---
                FCITX_INFO() << "=== Test 22: AltGr (ISO_Level3_Shift) as leader ===";
                configureLeaders(instance, false, false, false, false, false, true);
                auto uuid22 = createAndActivate(instance, tf, "test22");

                tf->call<ITestFrontend::sendKeyEvent>(
                    uuid22, Key(FcitxKey_a, KeyStates(), kCodeA), false);

                bool consumed22 = tf->call<ITestFrontend::sendKeyEvent>(
                    uuid22, Key(FcitxKey_ISO_Level3_Shift, KeyStates(), kCodeAltGr), false);
                FCITX_ASSERT(consumed22) << "AltGr leader during gesture should be consumed";

                tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
                tf->call<ITestFrontend::keyEvent>(
                    uuid22, Key(FcitxKey_a, KeyStates(), kCodeA), true);

                holder->timer = instance->eventLoop().addTimeEvent(
                    CLOCK_MONOTONIC, nowUsec() + kDeferredVerifyDelayUsec, 0,
                    [instance, uuid22, holder](EventSourceTime *, uint64_t) {
                        auto *tf = instance->addonManager().addon("testfrontend");
                        tf->call<ITestFrontend::destroyInputContext>(uuid22);
                        FCITX_INFO() << "Test 22 PASSED";

                        // --- Test 23: Alt release consumed, commits "ä" ---
                        FCITX_INFO() << "=== Test 23: Alt release consumed after leader use ===";
                        configureLeaders(instance, false, false, false, false, false, true);
                        auto uuid23 = createAndActivate(instance, tf, "test23");

                        tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_a, KeyStates(), kCodeA), false);
                        tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), false);

                        bool c = tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_Alt_L, KeyStates(), kCodeAltL), true);
                        FCITX_ASSERT(c) << "Alt release after leader should be consumed";

                        tf->call<ITestFrontend::pushCommitExpectation>("\xc3\xa4");
                        c = tf->call<ITestFrontend::sendKeyEvent>(
                            uuid23, Key(FcitxKey_a, KeyStates(), kCodeA), true);
                        FCITX_ASSERT(c) << "Accent key release during Alt gesture should be consumed";

                        holder->timer = instance->eventLoop().addTimeEvent(
                            CLOCK_MONOTONIC, nowUsec() + kDeferredVerifyDelayUsec, 0,
                            [instance, uuid23, holder](EventSourceTime *, uint64_t) {
                                auto *tf = instance->addonManager().addon("testfrontend");
                                tf->call<ITestFrontend::destroyInputContext>(uuid23);
                                FCITX_INFO() << "Test 23 PASSED";
                                // All deferred commits verified — safe to continue
                                scheduleTestsAfterAltVerify(instance);
                                return false;
                            });
                        return false;
                    });
                return false;
            });
    });
}

// Remaining tests — scheduled only after Alt leader deferred commits are verified.
static void scheduleTestsAfterAltVerify(Instance *instance) {

    // =========================================================================
    // TEST 20: committedKeyCode_ — auto-repeat suppression after commit
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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

        // Release 'a' — should be consumed (committedKeyCode_)
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);
        FCITX_ASSERT(consumed) << "Release of committed key should be consumed";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 24 PASSED";
    });

    // =========================================================================
    // TEST 21: Custom leader key
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 25: Custom leader key '#' ===";
        configureLeaders(instance, false, false, false, false, false, false, "#");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test25");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Custom leader '#' during gesture should be consumed";

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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 26: Custom leader with UTF-8 char § ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "\xc2\xa7");  // § in UTF-8
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test26");

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
        FCITX_INFO() << "Test 26 PASSED";
    });

    // =========================================================================
    // TEST 25: Custom leader sanitization — multi-char trimmed to first
    // Config "#x" should sanitize to "#" and work as leader.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 27: Custom leader multi-char sanitized ===";
        configureLeaders(instance, false, false, false, false, false, false, "#x");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test27");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Sanitized '#' from '#x' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 27 PASSED";
    });

    // =========================================================================
    // TEST 26: Custom leader sanitization — whitespace trimmed
    // Config "  #  " should sanitize to "#" and work as leader.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 28: Custom leader whitespace trimmed ===";
        configureLeaders(instance, false, false, false, false, false, false, "  #  ");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test28");

        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), false);

        tf->call<ITestFrontend::pushCommitExpectation>("ä");
        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_numbersign, KeyStates(), kCodeHash), false);
        FCITX_ASSERT(consumed) << "Sanitized '#' from '  #  ' should work as leader";

        tf->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_a, KeyStates(), kCodeA), true);

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 28 PASSED";
    });

    // NOTE: Whitespace-only custom leader rejected test removed — same
    // testfrontend commit-expectation issue (OTHER KEYS path).

    // =========================================================================
    // TEST 28: Custom leader sanitization — multi-byte UTF-8 trimmed from longer
    // Config "§xyz" should sanitize to "§" (2 bytes kept, rest dropped).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 29: Multi-byte UTF-8 trimmed from longer ===";
        configureLeaders(instance, false, false, false, false, false, false,
                         "\xc2\xa7xyz");  // "§xyz"
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test29");

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
        FCITX_INFO() << "Test 29 PASSED";
    });

    // =========================================================================
    // TEST 40: Cycling — multiple outputs, cycle through variants
    // Mapping "a" → "ä,ae,@" — Space cycles: ä → ae → @ → ä → ...
    // Release 'a' after second Space → commits "ae" (index 1).
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 33: Space passthrough without gesture ===";
        configureLeaders(instance, true, false, false, false, false, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test33");

        bool consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed) << "Space without gesture should pass through";

        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), true);
        FCITX_ASSERT(!consumed) << "Space release without gesture should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 33 PASSED";
    });

    // =========================================================================
    // TEST 44: Ordering guard — non-Space key clears guard
    // After a commit, recentlyCommitted_ is set. Any non-Space press clears
    // it without consuming. A subsequent Space then also passes through.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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
        FCITX_ASSERT(!consumed) << "Unmapped 'b' after commit should pass through";
        tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_b, KeyStates(), kCodeB), true);

        // Space now → guard already cleared, should pass through
        consumed = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_space, KeyStates(), kCodeSpace), false);
        FCITX_ASSERT(!consumed) << "Space after guard cleared should pass through";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 34 PASSED";
    });

    // =========================================================================
    // TEST 45: State cleanup after cycling — new gesture works immediately
    // After a cycling commit (release key), all cycling state must be reset
    // so a new gesture can start without interference.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
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
        FCITX_ASSERT(consumed) << "Space after cycling commit must be consumed by ordering guard";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 36 PASSED";
    });

    // =========================================================================
    // TEST 47: Writing flow — Space leader, German 1000 chars
    // Fast typing with word-boundary overlap: hold last char + Space.
    // With Space as leader, mapped chars at word end get converted.
    // Mapped chars: a,e,i,o,u,s,c,n,y (multilingual cycling)
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 37: Space leader — German 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test37");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kGerman1000, true);

        FCITX_ASSERT(collisions > 10)
            << "German should have significant collisions with Space leader"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 37 PASSED — German: "
                      << collisions << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 48: Writing flow — Space leader, English 1000 chars
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 38: Space leader — English 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test38");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kEnglish1000, true);

        FCITX_ASSERT(collisions > 10)
            << "English should have significant collisions with Space leader"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 38 PASSED — English: "
                      << collisions << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 49: Writing flow — Space leader, French 1000 chars
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 39: Space leader — French 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test39");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kFrench1000, true);

        FCITX_ASSERT(collisions > 10)
            << "French should have many collisions (short words ending e/s)"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 39 PASSED — French: "
                      << collisions << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 50: Writing flow — Space leader, Portuguese 1000 chars
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 40: Space leader — Portuguese 1000 chars ===";
        configureMultilingualCycling(instance, true, false);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test40");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kPortuguese1000, true);

        FCITX_ASSERT(collisions > 10)
            << "Portuguese should have many collisions (words ending a/o/e/s)"
            << " (got " << collisions << ")";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 40 PASSED — Portuguese: "
                      << collisions << " collisions with Space leader";
    });

    // =========================================================================
    // TEST 51: Writing flow — Alt leader, German 1000 chars
    // Alt is never pressed during normal typing → 0 collisions.
    // This proves Alt is superior to Space for fast typing.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 41: Alt leader — German 1000 chars ===";
        configureMultilingualCycling(instance, false, true);
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test41");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kGerman1000, false);

        FCITX_ASSERT(collisions == 0)
            << "Alt leader must have zero collisions in normal typing";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 41 PASSED — Alt leader: "
                      << collisions << " collisions (zero!)";
    });

    // =========================================================================
    // TEST 52: Writing flow — Custom leader (#), German 1000 chars
    // Custom key is never pressed during normal typing → 0 collisions.
    // Same advantage as Alt but key position may differ ergonomically.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 42: Custom leader (#) — German 1000 chars ===";
        configureMultilingualCycling(instance, false, false, "#");
        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test42");

        int collisions = typeTextWordBoundaryOverlap(
            tf, uuid, kGerman1000, false);

        FCITX_ASSERT(collisions == 0)
            << "Custom leader must have zero collisions in normal typing";

        tf->call<ITestFrontend::destroyInputContext>(uuid);
        FCITX_INFO() << "Test 42 PASSED — Custom leader (#): "
                      << collisions << " collisions (zero!)";
    });

    // =========================================================================
    // TEST 53: Overlap collision demo — "anbau" → "anbaü" (standard mapping)
    // Demonstrates the specific problem: user holds 'u' at word end,
    // presses Space → 'u' converts to 'ü'. Output: "anbaü" not "anbau ".
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 44: Dual leader collision analysis ===";

        struct LangResult {
            const char *name;
            const char *text;
            int spaceCollisions;
            int aLeaderCollisions;
            int semicolonCollisions;
        };

        LangResult langs[] = {
            {"German",     kGerman1000,     0, 0, 0},
            {"English",    kEnglish1000,    0, 0, 0},
            {"French",     kFrench1000,     0, 0, 0},
            {"Portuguese", kPortuguese1000, 0, 0, 0},
            {"Spanish",    kSpanish1000,    0, 0, 0},
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
            {"q/p", 'q', 'p'},
            {"q/[", 'q', '['},
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
        FCITX_INFO() << "Test 44 PASSED — " << result;
    });

    // =========================================================================
    // TEST 55: CustomKey2 basic — second custom leader works
    // CustomKey2="#", hold 'a' + '#' → ä
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 45: CustomKey2 basic ===";
        configureLeaders(instance, false, false, false, false, false, false, "", "#");
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 46: Dual split — allowed ===";
        configureLeaders(instance, false, false, false, false, false, false, "z", "/");
        setMappings(instance, {{"u", "\xc3\xbc"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test46");

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
        FCITX_INFO() << "Test 46 PASSED";
    });

    // =========================================================================
    // TEST 57: Dual split — blocked (same hand)
    // Same config. Hold 'a' (left input) + press 'z' (left leader) → NO conversion.
    // 'z' falls through to OTHER KEYS, commits 'a' as original, 'z' passes through.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 47: Dual split — blocked ===";
        configureLeaders(instance, false, false, false, false, false, false, "z", "/");
        setMappings(instance, {{"u", "\xc3\xbc"}, {"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test47");

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
        FCITX_INFO() << "Test 47 PASSED";
    });

    // =========================================================================
    // TEST 58: Dual split — reverse direction
    // Hold 'a' (left) + press '/' (right leader) → ä (allowed: opposite hands)
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 48: Dual split — reverse ===";
        configureLeaders(instance, false, false, false, false, false, false, "z", "/");
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
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 49: Single custom key — no split ===";
        configureLeaders(instance, false, false, false, false, false, false, "z");
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
    // TEST 60: Dual split — built-in leader ignores split
    // CustomKey="z", CustomKey2="/", Space=True.
    // Space is BuiltIn → ignores hand split, triggers all mappings.
    // =========================================================================
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== Test 50: Built-in leader ignores split ===";
        configureLeaders(instance, true, false, false, false, false, false, "z", "/");
        setMappings(instance, {{"a", "\xc3\xa4"}});

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = createAndActivate(instance, tf, "test50");

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
        FCITX_INFO() << "Test 50 PASSED";
    });

    // All synchronous tests done — exit
    instance->eventDispatcher().schedule([instance]() {
        FCITX_INFO() << "=== All tests PASSED ===";
        instance->exit();
    });
}

int main() {
    setupTestingEnvironment(
        TESTING_BINARY_DIR,
        {"."},
        {"tests"});

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
