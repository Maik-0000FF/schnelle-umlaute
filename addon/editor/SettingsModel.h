#ifndef SCHNELLE_UMLAUTE_EDITOR_SETTINGS_MODEL_H
#define SCHNELLE_UMLAUTE_EDITOR_SETTINGS_MODEL_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class SettingsModel : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int delayLowercase READ delayLowercase WRITE setDelayLowercase
                   NOTIFY delayLowercaseChanged)
    Q_PROPERTY(int delayUppercase READ delayUppercase WRITE setDelayUppercase
                   NOTIFY delayUppercaseChanged)

    Q_PROPERTY(bool leaderSpace READ leaderSpace WRITE setLeaderSpace
                   NOTIFY leaderSpaceChanged)
    Q_PROPERTY(bool leaderLeft READ leaderLeft WRITE setLeaderLeft
                   NOTIFY leaderLeftChanged)
    Q_PROPERTY(bool leaderRight READ leaderRight WRITE setLeaderRight
                   NOTIFY leaderRightChanged)
    Q_PROPERTY(bool leaderUp READ leaderUp WRITE setLeaderUp
                   NOTIFY leaderUpChanged)
    Q_PROPERTY(bool leaderDown READ leaderDown WRITE setLeaderDown
                   NOTIFY leaderDownChanged)
    Q_PROPERTY(bool leaderAlt READ leaderAlt WRITE setLeaderAlt
                   NOTIFY leaderAltChanged)

    Q_PROPERTY(bool customKey1Enabled READ customKey1Enabled
                   WRITE setCustomKey1Enabled NOTIFY customKey1EnabledChanged)
    Q_PROPERTY(QString customKey1 READ customKey1 WRITE setCustomKey1
                   NOTIFY customKey1Changed)
    Q_PROPERTY(bool customKey2Enabled READ customKey2Enabled
                   WRITE setCustomKey2Enabled NOTIFY customKey2EnabledChanged)
    Q_PROPERTY(QString customKey2 READ customKey2 WRITE setCustomKey2
                   NOTIFY customKey2Changed)

    Q_PROPERTY(QString appFilterMode READ appFilterMode WRITE setAppFilterMode
                   NOTIFY appFilterModeChanged)
    Q_PROPERTY(QStringList blacklist READ blacklist NOTIFY blacklistChanged)
    Q_PROPERTY(QStringList whitelist READ whitelist NOTIFY whitelistChanged)

public:
    explicit SettingsModel(QObject *parent = nullptr);

    int delayLowercase() const { return delayLowercase_; }
    int delayUppercase() const { return delayUppercase_; }
    bool leaderSpace() const { return leaderSpace_; }
    bool leaderLeft() const { return leaderLeft_; }
    bool leaderRight() const { return leaderRight_; }
    bool leaderUp() const { return leaderUp_; }
    bool leaderDown() const { return leaderDown_; }
    bool leaderAlt() const { return leaderAlt_; }
    bool customKey1Enabled() const { return customKey1Enabled_; }
    QString customKey1() const { return customKey1_; }
    bool customKey2Enabled() const { return customKey2Enabled_; }
    QString customKey2() const { return customKey2_; }
    QString appFilterMode() const { return appFilterMode_; }
    QStringList blacklist() const { return blacklist_; }
    QStringList whitelist() const { return whitelist_; }

    void setDelayLowercase(int v);
    void setDelayUppercase(int v);
    void setLeaderSpace(bool v);
    void setLeaderLeft(bool v);
    void setLeaderRight(bool v);
    void setLeaderUp(bool v);
    void setLeaderDown(bool v);
    void setLeaderAlt(bool v);
    void setCustomKey1Enabled(bool v);
    void setCustomKey1(const QString &v);
    void setCustomKey2Enabled(bool v);
    void setCustomKey2(const QString &v);
    void setAppFilterMode(const QString &v);

    Q_INVOKABLE void addBlacklistEntry(const QString &entry);
    Q_INVOKABLE void removeBlacklistEntry(int index);
    Q_INVOKABLE void addWhitelistEntry(const QString &entry);
    Q_INVOKABLE void removeWhitelistEntry(int index);
    Q_INVOKABLE bool isActiveLeaderKey(const QString &key) const;

    static bool isValidLeaderKey(const QString &s);

Q_SIGNALS:
    void saveFinished();
    void delayLowercaseChanged();
    void delayUppercaseChanged();
    void leaderSpaceChanged();
    void leaderLeftChanged();
    void leaderRightChanged();
    void leaderUpChanged();
    void leaderDownChanged();
    void leaderAltChanged();
    void customKey1EnabledChanged();
    void customKey1Changed();
    void customKey2EnabledChanged();
    void customKey2Changed();
    void appFilterModeChanged();
    void blacklistChanged();
    void whitelistChanged();

private:
    void load();
    void save();
    void reloadFcitx();

    int delayLowercase_ = 400;
    int delayUppercase_ = 700;
    bool leaderSpace_ = true;
    bool leaderLeft_ = false;
    bool leaderRight_ = false;
    bool leaderUp_ = false;
    bool leaderDown_ = false;
    bool leaderAlt_ = false;
    bool customKey1Enabled_ = false;
    QString customKey1_;
    bool customKey2Enabled_ = false;
    QString customKey2_;
    QString appFilterMode_ = "Disabled";
    QStringList blacklist_;
    QStringList whitelist_;
};

#endif
