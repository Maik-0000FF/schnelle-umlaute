#include "OverlayController.h"

OverlayController::OverlayController(QObject *parent) : QObject(parent) {}

void OverlayController::show(const QStringList &variants, int currentIndex,
                             const QString &position, int cursorX,
                             int cursorY) {
    variants_ = variants;
    currentIndex_ = currentIndex;
    if (!position.isEmpty()) position_ = position;
    cursorX_ = cursorX;
    cursorY_ = cursorY;
    visible_ = !variants.isEmpty();
    Q_EMIT stateChanged();
}

void OverlayController::hide() {
    visible_ = false;
    Q_EMIT stateChanged();
}

OverlayDBusAdaptor::OverlayDBusAdaptor(OverlayController *ctrl)
    : QDBusAbstractAdaptor(ctrl), ctrl_(ctrl) {}

void OverlayDBusAdaptor::Show(const QStringList &variants, int currentIndex,
                              const QString &position, int cursorX,
                              int cursorY) {
    ctrl_->show(variants, currentIndex, position, cursorX, cursorY);
}

void OverlayDBusAdaptor::Hide() { ctrl_->hide(); }
