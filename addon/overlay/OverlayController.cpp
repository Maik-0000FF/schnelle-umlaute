#include "OverlayController.h"

#include <QCoreApplication>

OverlayController::OverlayController(QObject *parent) : QObject(parent) {}

void OverlayController::show(const QStringList &variants, int currentIndex,
                             const QString &position) {
    variants_ = variants;
    currentIndex_ = currentIndex;
    if (!position.isEmpty()) position_ = position;
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

OverlayDBusAdaptor::OverlayDBusAdaptor(OverlayController *ctrl)
    : QDBusAbstractAdaptor(ctrl), ctrl_(ctrl) {}

void OverlayDBusAdaptor::Show(const QStringList &variants, int currentIndex,
                              const QString &position) {
    ctrl_->show(variants, currentIndex, position);
}

void OverlayDBusAdaptor::Hide() { ctrl_->hide(); }

void OverlayDBusAdaptor::Quit() { ctrl_->quit(); }
