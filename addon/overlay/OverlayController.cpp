#include "OverlayController.h"
#include "../themes.h"
#include "progress_overlay_geometry.h"

#include <QCoreApplication>

#include <cstdio>

OverlayController::OverlayController(QObject *parent) : QObject(parent) {}

void OverlayController::show(const QStringList &variants, int currentIndex,
                             const QString &position, bool label) {
    variants_ = variants;
    currentIndex_ = currentIndex;
    if (!position.isEmpty())
        position_ = position;
    label_ = label;
    visible_ = !variants.isEmpty();
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

void OverlayController::sendCursor(int x, int y) {
    Q_EMIT cursorReported(x, y);
}

void OverlayController::setProgress(int leadMs, int windowMs) {
    progressLeadMs_ = leadMs;
    progressWindowMs_ = windowMs;
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

// Trust note: this method is unauthenticated, so any session process can push
// a cursor pixel. The blast radius is bounded — it can only misplace the
// overlay on this session's screen — and the reply is not request-id matched
// against the pending query, so the "only one overlay open at a time"
// invariant (see OverlayRenderer) is the sole guard against a stale/spoofed
// value landing on the wrong open. Acceptable for a session-local convenience
// surface; revisit if the daemon ever gains a security boundary.
void OverlayDBusAdaptor::SendCursor(int x, int y) { ctrl_->sendCursor(x, y); }

void OverlayDBusAdaptor::SetProgress(int leadMs, int windowMs) {
    ctrl_->setProgress(leadMs, windowMs);
}

void OverlayDBusAdaptor::FreezeProgress() { ctrl_->freezeProgress(); }
