#ifndef SCHNELLE_UMLAUTE_EDITOR_KEY_COMBO_UTIL_H
#define SCHNELLE_UMLAUTE_EDITOR_KEY_COMBO_UTIL_H

#include "keycombo.h"
#include <QObject>
#include <QQmlEngine>
#include <QString>

// QML singleton exposing the Qt-to-fcitx combo conversion to KeyCaptureField.
// The actual mapping lives in qtKeyComboToPortable() (keycombo.h) so it can be
// unit-tested without QML.
class KeyComboUtil : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit KeyComboUtil(QObject *parent = nullptr) : QObject(parent) {}

    // Returns the fcitx portable combo string for a captured Qt key + modifier
    // mask, or "" if the combo is not usable as a shortcut.
    Q_INVOKABLE QString toPortable(int qtKey, int qtModifiers) const {
        return qtKeyComboToPortable(qtKey, qtModifiers);
    }
};

#endif
