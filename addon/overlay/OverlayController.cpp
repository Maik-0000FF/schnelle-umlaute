#include "OverlayController.h"
#include "../caret_theme_io.h"
#include "../overlay_protocol.h"
#include "../themes.h"
#include "overlay_render.h"
#include "progress_overlay_geometry.h"

#include <QCoreApplication>

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace {
// Time-unit conversions, named to mirror the engine (state.h defines the same
// values for its side of the SetProgress D-Bus protocol, which carries
// CLOCK_MONOTONIC microseconds). The overlay is a separate Qt binary and cannot
// include the fcitx-tied engine header, so the units are named here too rather
// than left as bare literals on the daemon side of the boundary.
constexpr qint64 kMicrosecondsPerSecond = 1'000'000;
constexpr qint64 kNanosecondsPerMicrosecond = 1'000;
constexpr qint64 kMicrosecondsPerMillisecond = 1'000;

// Same monotonic clock the engine stamps the gesture start with (state.h's
// nowUsec), so the two processes' timestamps are directly comparable.
qint64 monotonicUsec() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<qint64>(ts.tv_sec) * kMicrosecondsPerSecond +
           ts.tv_nsec / kNanosecondsPerMicrosecond;
}
} // namespace

OverlayController::OverlayController(QObject *parent) : QObject(parent) {}

void OverlayController::show(const QStringList &variants, int currentIndex,
                             const QString &position, bool label) {
    // Decide BEFORE the old values are gone, and stop the transitions BEFORE the
    // new ones are written: an animation is started by the property write, so a
    // gate flipped afterwards is too late. This is the protocol edge, the one
    // place every show passes through, which the renderer is not (a new gesture
    // at the same position with the same variants is a no-op for it, and the
    // engine can leave the previous overlay on screen until its commit flash
    // times out). Cycling keeps its animation: same variants, moved index.
    if (schnelle_umlaute::render::showSnapsTransitions(
            visible_, variants != variants_, currentIndex)) {
        setAnimate(false);
    }
    variants_ = variants;
    currentIndex_ = currentIndex;
    if (!position.isEmpty())
        position_ = position;
    label_ = label;
    visible_ = !variants.isEmpty();
    // A label show is a standalone profile-name pill (a runtime profile switch).
    // If a gesture's progress bar is still running or frozen (hold a mapped key,
    // then trigger a switch before releasing), clear it so the bar can't render
    // above the name. Plain accent overlays (label=false) leave the bar alone,
    // so the frozen cycling bar keeps showing.
    if (label && (progressActive_ || progressFrozen_)) {
        progressActive_ = false;
        progressFrozen_ = false;
        Q_EMIT progressChanged();
    }
    Q_EMIT stateChanged();
}

void OverlayController::hide() {
    visible_ = false;
    Q_EMIT stateChanged();
    // Clear the progress bar so the next gesture starts a fresh timeline and a
    // non-progress overlay never shows a stale bar.
    if (progressActive_ || progressFrozen_) {
        progressActive_ = false;
        progressFrozen_ = false;
        Q_EMIT progressChanged();
    }
}

void OverlayController::quit() {
    // Scheduled via the event loop so the DBus reply can be flushed before
    // the process exits — calling quit() directly in the slot sometimes
    // leaves the caller without an acknowledgement.
    QMetaObject::invokeMethod(QCoreApplication::instance(), "quit",
                              Qt::QueuedConnection);
}

void OverlayController::setTheme(const QString &theme) {
    if (!isValidTheme(theme)) {
        std::fprintf(stderr,
                     "schnelle-umlaute-overlay: ignoring invalid theme '%s'\n",
                     theme.toUtf8().constData());
        return;
    }
    if (theme_ == theme)
        return;
    theme_ = theme;
    Q_EMIT themeChanged();
}

bool OverlayController::isValidTheme(const QString &name) {
    return schnelle_umlaute::isValidTheme(name);
}

void OverlayController::setAnimate(bool on) {
    if (animate_ == on)
        return;
    animate_ = on;
    Q_EMIT animateChanged();
}

void OverlayController::setPlaced(bool on) {
    if (placed_ == on)
        return;
    placed_ = on;
    Q_EMIT placedChanged();
}

void OverlayController::applyCaretTheme(const QString &background,
                                        const QString &text,
                                        const QString &highlight,
                                        const QString &highlightText,
                                        const QString &border) {
    // A daemon has nobody to show an error to, so a failure is logged and
    // dropped: the candidate window then keeps the colours it had, which is the
    // same outcome as before this feature existed.
    if (!schnelle_umlaute::caret::apply(background, text, highlight,
                                        highlightText, border))
        qWarning("schnelle-umlaute-overlay: could not write the candidate "
                 "window theme");
}

void OverlayController::setCentering(bool horizontally, bool vertically) {
    if (horizontallyCentered_ == horizontally &&
        verticallyCentered_ == vertically)
        return;
    horizontallyCentered_ = horizontally;
    verticallyCentered_ = vertically;
    Q_EMIT progressChanged();
}

void OverlayController::sendCursor(int requestId, int x, int y) {
    Q_EMIT cursorReported(requestId, x, y);
}

void OverlayController::setProgress(int leadMs, int windowMs, qint64 startUsec) {
    // A gesture start, always. It arrives BEFORE this gesture's Show, and the
    // previous overlay may still be on screen (the engine's commit flash hides
    // with a delay), so without this the panel would animate down from its
    // fully-revealed state of the last gesture while visible: a flash during the
    // lead-in, before the timing window has even opened.
    setAnimate(false);
    progressLeadMs_ = leadMs;
    progressWindowMs_ = windowMs;
    // Measure how much of the gesture already elapsed by the time this message
    // arrived, against the engine's start on the shared monotonic clock, and
    // clamp into [0, total]. startUsec <= 0 (or a clock that ran backwards)
    // disables the compensation. The QML bar starts pre-advanced by this so it
    // closes in step with the engine's real window instead of latency-late.
    const int total = std::max(0, leadMs) + std::max(0, windowMs);
    progressStartUsec_ = startUsec;
    if (startUsec > 0) {
        const qint64 elapsedMs =
            (monotonicUsec() - startUsec) / kMicrosecondsPerMillisecond;
        progressElapsedMs_ =
            static_cast<int>(std::clamp<qint64>(elapsedMs, 0, total));
    } else {
        progressElapsedMs_ = 0;
    }
    progressActive_ = true;
    progressFrozen_ = false;
    Q_EMIT progressChanged();
}

void OverlayController::freezeProgress() {
    if (!progressActive_ || progressFrozen_)
        return;
    progressFrozen_ = true;
    Q_EMIT progressChanged();
}

int OverlayController::progressBarLength(int totalMs, int screenWidth) const {
    return schnelle_umlaute::progress::barLength(totalMs, screenWidth);
}

int OverlayController::progressLeadLength(int barLen, int leadMs,
                                          int totalMs) const {
    return schnelle_umlaute::progress::leadLength(barLen, leadMs, totalMs);
}

int OverlayController::progressElapsedNowMs() const {
    const int total =
        std::max(0, progressLeadMs_) + std::max(0, progressWindowMs_);
    if (progressStartUsec_ <= 0)
        return std::clamp(progressElapsedMs_, 0, total);
    const qint64 elapsedMs =
        (monotonicUsec() - progressStartUsec_) / kMicrosecondsPerMillisecond;
    return static_cast<int>(std::clamp<qint64>(elapsedMs, 0, total));
}

OverlayDBusAdaptor::OverlayDBusAdaptor(OverlayController *ctrl)
    : QDBusAbstractAdaptor(ctrl), ctrl_(ctrl) {}

void OverlayDBusAdaptor::Show(const QStringList &variants, int currentIndex,
                              const QString &position, bool label) {
    ctrl_->show(variants, currentIndex, position, label);
}

void OverlayDBusAdaptor::Hide() { ctrl_->hide(); }

void OverlayDBusAdaptor::Quit() { ctrl_->quit(); }

void OverlayDBusAdaptor::SetTheme(const QString &theme) {
    ctrl_->setTheme(theme);
}

void OverlayDBusAdaptor::ReloadConfig() { Q_EMIT ctrl_->reloadRequested(); }

// Trust note: this method is unauthenticated, so any session process can push a
// cursor pixel. The blast radius is bounded: it can only misplace the overlay on
// this session's screen. The requestId is matched against the query in flight
// (see KWinCursorSource::reportCursor), which drops a reply that outlived its
// own query, but it is correlation rather than authorisation: the id is written
// in plain text into the script file. Acceptable for a session-local
// convenience surface; revisit if the daemon ever gains a security boundary.
void OverlayDBusAdaptor::SendCursor(int requestId, int x, int y) {
    ctrl_->sendCursor(requestId, x, y);
}

void OverlayDBusAdaptor::SetProgress(int leadMs, int windowMs,
                                    qlonglong startUsec) {
    ctrl_->setProgress(leadMs, windowMs, startUsec);
}

void OverlayDBusAdaptor::FreezeProgress() { ctrl_->freezeProgress(); }

int OverlayDBusAdaptor::GetProtocolVersion() {
    return schnelle_umlaute::kOverlayProtocolVersion;
}
