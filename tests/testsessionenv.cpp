// Unit tests for classifySessionEnv().
// Pure header-only logic — verifies the rule table that decides whether
// the editor tells the user to log out (environment.d is imported) or to
// edit the compositor configuration (TTY-launched compositor that never
// reads environment.d).

#include "src/session_env.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using fcitx::classifySessionEnv;
using fcitx::EnvMechanism;
using fcitx::imEnvironmentdPayload;
using fcitx::mergeCompositorEnv;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// Hyprland is started from a TTY by default — environment.d is not read,
// so the variables belong in hyprland.conf as `env = KEY,VALUE` lines.
void testHyprlandIsCompositorConf() {
    auto info = classifySessionEnv("wayland", "Hyprland");
    EXPECT(info.mechanism == EnvMechanism::CompositorConf);
    EXPECT(info.session == "Hyprland (Wayland)");
    EXPECT(info.configPath == "~/.config/hypr/hyprland.conf");
    EXPECT(info.snippet.find("env = GTK_IM_MODULE,fcitx") != std::string::npos);
    EXPECT(info.snippet.find("env = QT_IM_MODULE,fcitx") != std::string::npos);
    EXPECT(info.snippet.find("env = XMODIFIERS,@im=fcitx") !=
           std::string::npos);
}

// Other wlroots compositors share the CompositorConf verdict but have no
// single config syntax, so the neutral KEY=VALUE list is shown and no
// config path is claimed.
void testSwayIsCompositorConfWithGenericSnippet() {
    auto info = classifySessionEnv("wayland", "sway");
    EXPECT(info.mechanism == EnvMechanism::CompositorConf);
    EXPECT(info.configPath.empty());
    EXPECT(info.snippet.find("GTK_IM_MODULE=fcitx") != std::string::npos);
    EXPECT(info.snippet.find("env =") == std::string::npos);
}

void testRiverIsCompositorConf() {
    EXPECT(classifySessionEnv("wayland", "river").mechanism ==
           EnvMechanism::CompositorConf);
}

void testNiriIsCompositorConf() {
    EXPECT(classifySessionEnv("wayland", "niri").mechanism ==
           EnvMechanism::CompositorConf);
}

// KDE Plasma logs in through SDDM, which goes through the systemd
// graphical session and imports environment.d → relogin works.
void testKdeIsEnvironmentD() {
    auto info = classifySessionEnv("wayland", "KDE");
    EXPECT(info.mechanism == EnvMechanism::EnvironmentD);
    EXPECT(info.snippet.empty());
    EXPECT(info.configPath.empty());
    EXPECT(info.session == "KDE (Wayland)");
}

// Ubuntu ships XDG_CURRENT_DESKTOP="ubuntu:GNOME"; substring match must
// not misfire and GNOME stays on the environment.d path.
void testGnomeIsEnvironmentD() {
    EXPECT(classifySessionEnv("x11", "ubuntu:GNOME").mechanism ==
           EnvMechanism::EnvironmentD);
}

// Unset desktop is the conservative default: keep the original relogin
// advice rather than guessing at compositor config.
void testUnsetDesktopDefaultsToEnvironmentD() {
    auto info = classifySessionEnv(nullptr, nullptr);
    EXPECT(info.mechanism == EnvMechanism::EnvironmentD);
    EXPECT(info.session == "unknown");
}

// Session-type only affects the descriptor suffix, never the verdict.
void testSessionTypeOnlyLabels() {
    EXPECT(classifySessionEnv("x11", "KDE").session == "KDE (X11)");
    EXPECT(classifySessionEnv(nullptr, "KDE").session == "KDE");
}

// KDE Plasma Wayland is the one desktop that drops GTK_IM_MODULE /
// QT_IM_MODULE (fcitx5's "Wayland Diagnose" warning). The delivery mechanism
// stays EnvironmentD (testKdeIsEnvironmentD); only the module-need axis changes.
void testKdeWaylandDropsImModules() {
    EXPECT(classifySessionEnv("wayland", "KDE").needsImModules == false);
}

// Every other session keeps the full module set: KDE X11 (no native protocol,
// its X11 apps still need the modules), GNOME, wlroots compositors, and the
// unknown default. Gating on Wayland alone, or desktop alone, would be wrong.
void testImModulesNeededElsewhere() {
    EXPECT(classifySessionEnv("x11", "KDE").needsImModules == true);
    EXPECT(classifySessionEnv("wayland", "ubuntu:GNOME").needsImModules == true);
    EXPECT(classifySessionEnv("x11", "ubuntu:GNOME").needsImModules == true);
    EXPECT(classifySessionEnv("wayland", "Hyprland").needsImModules == true);
    EXPECT(classifySessionEnv("wayland", "sway").needsImModules == true);
    EXPECT(classifySessionEnv(nullptr, nullptr).needsImModules == true);
}

// The environment.d payload: the full set keeps GTK/QT, the reduced set drops
// them; both always carry XMODIFIERS and GLFW_IM_MODULE.
void testImEnvironmentdPayload() {
    const std::string full = imEnvironmentdPayload(true);
    EXPECT(full.find("GTK_IM_MODULE=fcitx") != std::string::npos);
    EXPECT(full.find("QT_IM_MODULE=fcitx") != std::string::npos);
    EXPECT(full.find("XMODIFIERS=@im=fcitx") != std::string::npos);
    EXPECT(full.find("GLFW_IM_MODULE=ibus") != std::string::npos);

    const std::string reduced = imEnvironmentdPayload(false);
    EXPECT(reduced.find("GTK_IM_MODULE") == std::string::npos);
    EXPECT(reduced.find("QT_IM_MODULE") == std::string::npos);
    EXPECT(reduced.find("XMODIFIERS=@im=fcitx") != std::string::npos);
    EXPECT(reduced.find("GLFW_IM_MODULE=ibus") != std::string::npos);
}

// The Hyprland snippet (env = KEY,VALUE lines), reused by the merge tests.
// A function rather than a namespace-scope std::string: its construction can
// throw, which during static initialization could not be caught
// (bugprone-throwing-static-initialization).
static std::string hyprSnippet() {
    return classifySessionEnv("wayland", "Hyprland").snippet;
}

// Empty config → the labelled block becomes the whole file, no leading blank
// line, snippet preserved verbatim.
void testMergeIntoEmptyFile() {
    auto r = mergeCompositorEnv("", hyprSnippet());
    EXPECT(r.changed);
    EXPECT(r.content ==
           "# schnelle-umlaute: fcitx5 input-method environment\n" +
               hyprSnippet() + "\n");
}

// All three lines already present uncommented → no-op, nothing to write.
void testMergeAllPresentIsNoOp() {
    const std::string existing = "monitor=,preferred,auto,1\n" + hyprSnippet() +
                                 "\nexec-once = waybar\n";
    auto r = mergeCompositorEnv(existing, hyprSnippet());
    EXPECT(!r.changed);
    EXPECT(r.content.empty());
}

// Commented-out lines do not count as present — the block is still appended.
void testMergeIgnoresCommentedLines() {
    const std::string existing = "# env = GTK_IM_MODULE,fcitx\n"
                                 "# env = QT_IM_MODULE,fcitx\n"
                                 "# env = XMODIFIERS,@im=fcitx\n";
    auto r = mergeCompositorEnv(existing, hyprSnippet());
    EXPECT(r.changed);
    // Existing (commented) content kept verbatim, our block appended after.
    EXPECT(r.content.find(existing) == 0);
    EXPECT(
        r.content.find("# schnelle-umlaute: fcitx5 input-method environment") !=
        std::string::npos);
}

// Partial presence still appends the full block (idempotency favours not
// surgically editing the user's file; duplicate env lines are harmless).
void testMergePartialAppendsFullBlock() {
    const std::string existing = "env = GTK_IM_MODULE,fcitx\n";
    auto r = mergeCompositorEnv(existing, hyprSnippet());
    EXPECT(r.changed);
    EXPECT(r.content.find("env = QT_IM_MODULE,fcitx") != std::string::npos);
    EXPECT(r.content.find("env = XMODIFIERS,@im=fcitx") != std::string::npos);
}

// User content is preserved exactly and separated from our block by a blank
// line; a file without a trailing newline still gets clean separation.
void testMergePreservesContentAndSeparates() {
    const std::string existing = "exec-once = foo"; // no trailing newline
    auto r = mergeCompositorEnv(existing, hyprSnippet());
    EXPECT(r.changed);
    EXPECT(r.content.find("exec-once = foo\n\n"
                          "# schnelle-umlaute: fcitx5 "
                          "input-method environment\n") == 0);
}

int main() {
    testHyprlandIsCompositorConf();
    testSwayIsCompositorConfWithGenericSnippet();
    testRiverIsCompositorConf();
    testNiriIsCompositorConf();
    testKdeIsEnvironmentD();
    testGnomeIsEnvironmentD();
    testUnsetDesktopDefaultsToEnvironmentD();
    testSessionTypeOnlyLabels();
    testKdeWaylandDropsImModules();
    testImModulesNeededElsewhere();
    testImEnvironmentdPayload();
    testMergeIntoEmptyFile();
    testMergeAllPresentIsNoOp();
    testMergeIgnoresCommentedLines();
    testMergePartialAppendsFullBlock();
    testMergePreservesContentAndSeparates();
    std::puts("testsessionenv: all passed");
    return 0;
}
