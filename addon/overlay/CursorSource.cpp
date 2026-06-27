#include "CursorSource.h"

#include <memory>
#include <utility>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QTimer>
#include <QtGlobal>

namespace schnelle_umlaute {

namespace {
// CLI backends answer instantly; the KWin script round-trips through the
// compositor's scripting engine, so it gets a longer leash. Both bound the
// wait so a hung compositor degrades to the grid fallback instead of stalling
// the overlay.
constexpr int kCliTimeoutMs = 500;
constexpr int kKwinTimeoutMs = 1000;

constexpr const char *kKWinService = "org.kde.KWin";
constexpr const char *kKWinScriptingPath = "/Scripting";
constexpr const char *kKWinScriptingIface = "org.kde.kwin.Scripting";
constexpr const char *kKWinScriptIface = "org.kde.kwin.Script";
} // namespace

std::optional<CursorPos> parseXyJson(const QByteArray &json) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject obj = doc.object();
    const QJsonValue x = obj.value(QStringLiteral("x"));
    const QJsonValue y = obj.value(QStringLiteral("y"));
    if (!x.isDouble() || !y.isDouble())
        return std::nullopt;
    return CursorPos{qRound(x.toDouble()), qRound(y.toDouble())};
}

// ── CLI backend (hyprctl / mmsg) ─────────────────────────────────────────────

CliCursorSource::CliCursorSource(QString program, QStringList args,
                                 QObject *parent)
    : CursorSource(parent), program_(std::move(program)),
      args_(std::move(args)) {}

void CliCursorSource::getCursor(CursorCallback cb) {
    auto *proc = new QProcess(this);
    // Guard so the callback fires exactly once across finished / error /
    // timeout, whichever wins the race.
    auto done = std::make_shared<bool>(false);
    auto finish = [cb = std::move(cb), done](std::optional<CursorPos> pos) {
        if (*done)
            return;
        *done = true;
        cb(pos);
    };

    connect(proc, &QProcess::finished, this,
            [proc, finish](int code, QProcess::ExitStatus status) {
                if (status != QProcess::NormalExit || code != 0)
                    finish(std::nullopt);
                else
                    finish(parseXyJson(proc->readAllStandardOutput()));
                proc->deleteLater();
            });
    connect(proc, &QProcess::errorOccurred, this,
            [proc, finish](QProcess::ProcessError) {
                finish(std::nullopt);
                proc->deleteLater();
            });
    QTimer::singleShot(kCliTimeoutMs, proc, [proc, finish]() {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
        finish(std::nullopt);
    });

    proc->start(program_, args_);
}

// ── KWin backend ─────────────────────────────────────────────────────────────

KWinCursorSource::KWinCursorSource(QString scriptDir, QString serviceName,
                                   QString objectPath, QString interfaceName,
                                   QObject *parent)
    : CursorSource(parent), scriptDir_(std::move(scriptDir)),
      serviceName_(std::move(serviceName)), objectPath_(std::move(objectPath)),
      interfaceName_(std::move(interfaceName)) {
    scriptPath_ = scriptDir_ + QStringLiteral("/get-cursor.js");
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this,
            [this]() { resolve(std::nullopt); });
}

bool KWinCursorSource::ensureScriptWritten() {
    if (scriptWritten_)
        return true;
    QDir().mkpath(QFileInfo(scriptPath_).absolutePath());
    QFile f(scriptPath_);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    // The script runs inside KWin's scripting engine, reads
    // workspace.cursorPos, then calls SendCursor back on the daemon's own
    // service so the daemon learns the live global cursor (a value a Wayland
    // client otherwise cannot read).
    const QString body =
        QStringLiteral("callDBus('%1', '%2', '%3', 'SendCursor', "
                       "workspace.cursorPos.x, workspace.cursorPos.y, "
                       "function() {});\n")
            .arg(serviceName_, objectPath_, interfaceName_);
    f.write(body.toUtf8());
    f.close();
    scriptWritten_ = true;
    return true;
}

void KWinCursorSource::getCursor(CursorCallback cb) {
    // One query in flight; a new one supersedes the old (drops it to a grid
    // fallback). Only one overlay opens at a time, so this is a safety net.
    if (pending_)
        resolve(std::nullopt);
    pending_ = std::move(cb);

    if (!ensureScriptWritten()) {
        resolve(std::nullopt);
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService),
        QString::fromLatin1(kKWinScriptingPath),
        QString::fromLatin1(kKWinScriptingIface), QStringLiteral("loadScript"));
    msg << scriptPath_;
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                QDBusPendingReply<int> reply = *w;
                w->deleteLater();
                if (reply.isError()) {
                    resolve(std::nullopt);
                    return;
                }
                runScript(reply.value());
            });
    timer_->start(kKwinTimeoutMs);
}

void KWinCursorSource::runScript(int id) {
    if (!pending_)
        return;
    // Plasma 6 exposes the loaded script at /Scripting/Script<id>. Plasma 5's
    // /<id> path is deliberately not handled: KDE 6 is current, and a Plasma 5
    // host simply never gets the callback and falls back to the grid position.
    currentScriptPath_ =
        QStringLiteral("/Scripting/Script") + QString::number(id);
    QDBusMessage run = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService), currentScriptPath_,
        QString::fromLatin1(kKWinScriptIface), QStringLiteral("run"));
    QDBusConnection::sessionBus().asyncCall(run);
    // The reply arrives via SendCursor → reportCursor(); the timer covers a
    // silent failure.
}

void KWinCursorSource::reportCursor(int x, int y) { resolve(CursorPos{x, y}); }

void KWinCursorSource::resolve(std::optional<CursorPos> pos) {
    timer_->stop();
    if (!currentScriptPath_.isEmpty()) {
        // Unload the one-shot script so repeated opens don't pile up script
        // instances inside KWin.
        QDBusMessage stop = QDBusMessage::createMethodCall(
            QString::fromLatin1(kKWinService), currentScriptPath_,
            QString::fromLatin1(kKWinScriptIface), QStringLiteral("stop"));
        QDBusConnection::sessionBus().asyncCall(stop);
        currentScriptPath_.clear();
    }
    if (pending_) {
        CursorCallback cb = std::move(pending_);
        pending_ = nullptr;
        cb(pos);
    }
}

// ── Null backend ─────────────────────────────────────────────────────────────

void NullCursorSource::getCursor(CursorCallback cb) { cb(std::nullopt); }

// ── Factory ──────────────────────────────────────────────────────────────────

CursorSource *createCursorSource(const QString &xdgCurrentDesktop,
                                 const KWinDeps &kwinDeps, QObject *parent) {
    const QString d = xdgCurrentDesktop.toLower();
    if (d.contains(QLatin1String("kde")) || d.contains(QLatin1String("plasma")))
        return new KWinCursorSource(kwinDeps.scriptDir, kwinDeps.serviceName,
                                    kwinDeps.objectPath, kwinDeps.interfaceName,
                                    parent);
    if (d.contains(QLatin1String("hyprland")))
        return new CliCursorSource(
            QStringLiteral("hyprctl"),
            {QStringLiteral("cursorpos"), QStringLiteral("-j")}, parent);
    if (d.contains(QLatin1String("mango")))
        return new CliCursorSource(
            QStringLiteral("mmsg"),
            {QStringLiteral("get"), QStringLiteral("cursorpos")}, parent);
    return new NullCursorSource(parent);
}

} // namespace schnelle_umlaute
