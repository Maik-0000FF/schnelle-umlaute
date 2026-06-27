#include "OverlayController.h"
#include "../themes.h"

#include <QCoreApplication>

#include <cstdio>

OverlayController::OverlayController(QObject *parent) : QObject(parent) {}

void OverlayController::show(const QStringList &variants, int currentIndex,
                             const QString &position) {
    variants_ = variants;
    currentIndex_ = currentIndex;
    if (!position.isEmpty())
        position_ = position;
    visible_ = !variants.isEmpty();
    Q_EMIT stateChanged();
}

void OverlayController::hide() {
    visible_ = false;
    Q_EMIT stateChanged();
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

OverlayDBusAdaptor::OverlayDBusAdaptor(OverlayController *ctrl)
    : QDBusAbstractAdaptor(ctrl), ctrl_(ctrl) {}

void OverlayDBusAdaptor::Show(const QStringList &variants, int currentIndex,
                              const QString &position) {
    ctrl_->show(variants, currentIndex, position);
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
