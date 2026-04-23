#ifndef SCHNELLE_UMLAUTE_EDITOR_CLICK_OUTSIDE_DEFOCUS_H
#define SCHNELLE_UMLAUTE_EDITOR_CLICK_OUTSIDE_DEFOCUS_H

#include <QByteArray>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>

// Clears focus from text-input items when the user clicks outside of them.
// Flickable exclusively grabs press events inside a ScrollView, which blocks
// QML-level TapHandlers — so we watch presses at the QWindow level instead.
class ClickOutsideDefocus : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::MouseButtonPress) {
            return QObject::eventFilter(watched, event);
        }
        auto *window = qobject_cast<QQuickWindow *>(watched);
        if (!window) {
            return QObject::eventFilter(watched, event);
        }
        QQuickItem *focusItem = window->activeFocusItem();
        if (!focusItem) {
            return QObject::eventFilter(watched, event);
        }
        const QByteArray cls = focusItem->metaObject()->className();
        if (!cls.contains("TextInput") && !cls.contains("TextEdit") &&
            !cls.contains("TextField") && !cls.contains("TextArea")) {
            return QObject::eventFilter(watched, event);
        }
        auto *me = static_cast<QMouseEvent *>(event);
        const QRectF itemRect = focusItem->mapRectToScene(
            QRectF(0, 0, focusItem->width(), focusItem->height()));
        if (!itemRect.contains(me->scenePosition())) {
            focusItem->setFocus(false);
        }
        return QObject::eventFilter(watched, event);
    }
};

#endif
