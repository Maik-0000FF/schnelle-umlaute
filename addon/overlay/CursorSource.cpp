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

// Plasma 6 exposes a loaded script at /Scripting/Script<id>. Plasma 5's /<id>
// path is deliberately not handled: KDE 6 is current, and a Plasma 5 host
// simply never gets the callback and falls back to the grid position.
QString kwinScriptPath(int id) {
    return QStringLiteral("/Scripting/Script") + QString::number(id);
}

// The script file is named per query ("get-cursor-<id>.js"). Writer and sweeper
// derive their name and their glob from these two, so the pattern that creates
// the files and the pattern that cleans them up cannot drift apart. The glob
// also still catches the single fixed-name "get-cursor.js" that daemons before
// the per-query naming wrote.
constexpr QLatin1String kScriptStem("get-cursor");
constexpr QLatin1String kScriptSuffix(".js");

QString scriptFileName(int requestId) {
    return kScriptStem + QStringLiteral("-") + QString::number(requestId) +
           kScriptSuffix;
}

QString scriptFileGlob() {
    return kScriptStem + QStringLiteral("*") + kScriptSuffix;
}
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
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this,
            [this]() { resolve(std::nullopt); });
    sweepScriptDir();
}

// A query's script file is deleted when the query resolves, so a daemon that
// dies between writing it and resolving (kill, crash, Quit mid-query) strands
// it. One sweep at construction clears those, plus the single fixed-name
// get-cursor.js that daemons before the per-query naming left behind. The
// source is built lazily on the first cursor-mode open and lives for the
// daemon's lifetime, so this runs once, off the hot path.
void KWinCursorSource::sweepScriptDir() {
    QDir dir(scriptDir_);
    if (!dir.exists())
        return;
    const QStringList stale = dir.entryList({scriptFileGlob()}, QDir::Files);
    for (const QString &name : stale)
        dir.remove(name);
}

int KWinCursorSource::takeRequestId() {
    const int id = requestCounter_;
    requestCounter_ = nextRequestId(requestCounter_);
    return id;
}

QString KWinCursorSource::scriptFilePath(int requestId) const {
    return scriptDir_ + QStringLiteral("/") + scriptFileName(requestId);
}

bool KWinCursorSource::writeScript(int requestId, const QString &filePath) {
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    // The script runs inside KWin's scripting engine, reads
    // workspace.cursorPos, then calls SendCursor back on the daemon's own
    // service so the daemon learns the live global cursor (a value a Wayland
    // client otherwise cannot read). It echoes the query id so a reply that
    // outlives its query can be told apart from the live one.
    const QString body =
        QStringLiteral("callDBus('%1', '%2', '%3', 'SendCursor', "
                       "%4, workspace.cursorPos.x, workspace.cursorPos.y, "
                       "function() {});\n")
            .arg(serviceName_, objectPath_, interfaceName_)
            .arg(requestId);
    // Insist on the WHOLE body reaching the disk before KWin is pointed at the
    // file. A short write or a failed flush would hand it a truncated script
    // that loads happily and never calls SendCursor, and the query would then
    // sit out the full kKwinTimeoutMs instead of failing here and falling back
    // to the grid at once. close() flushes and records any error.
    const QByteArray payload = body.toUtf8();
    if (f.write(payload) != payload.size())
        return false;
    f.close();
    return f.error() == QFileDevice::NoError;
}

void KWinCursorSource::removeScriptFile(const QString &filePath) {
    if (!filePath.isEmpty())
        QFile::remove(filePath);
}

void KWinCursorSource::getCursor(CursorCallback cb) {
    // One query in flight; a new one supersedes the old (drops it to a grid
    // fallback). Only one overlay opens at a time, so this is a safety net.
    if (pending_)
        resolve(std::nullopt);

    const int id = takeRequestId();
    const QString file = scriptFilePath(id);
    if (!writeScript(id, file)) {
        // Nothing started, so there is no query to resolve: answer directly.
        removeScriptFile(file);
        cb(std::nullopt);
        return;
    }
    pending_ = std::move(cb);
    activeRequestId_ = id;
    currentScriptFile_ = file;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService),
        QString::fromLatin1(kKWinScriptingPath),
        QString::fromLatin1(kKWinScriptingIface), QStringLiteral("loadScript"));
    msg << file;
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, id, file](QDBusPendingCallWatcher *w) {
                QDBusPendingReply<int> reply = *w;
                w->deleteLater();
                // KWin declines a load with a NEGATIVE id — most notably when
                // it already has that path loaded (it deduplicates by file
                // name). Per-query file names put that out of reach, but an
                // unchecked negative would address a script object that does
                // not exist, and the query would then sit out the full timeout
                // instead of falling back to the grid right away.
                if (reply.isError() || reply.value() < 0) {
                    if (id == activeRequestId_)
                        resolve(std::nullopt);
                    else
                        removeScriptFile(file);
                    return;
                }
                const QString dbusPath = kwinScriptPath(reply.value());
                // The query was superseded or timed out while the load was in
                // flight. KWin instantiated the script anyway, so unload it and
                // drop its file rather than leaking one of each per abandoned
                // query.
                if (id != activeRequestId_) {
                    stopScript(dbusPath);
                    removeScriptFile(file);
                    return;
                }
                currentScriptPath_ = dbusPath;
                runScript(dbusPath);
            });
    timer_->start(kKwinTimeoutMs);
}

void KWinCursorSource::runScript(const QString &dbusPath) {
    QDBusMessage run = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService), dbusPath,
        QString::fromLatin1(kKWinScriptIface), QStringLiteral("run"));
    QDBusConnection::sessionBus().asyncCall(run);
    // The reply arrives via SendCursor → reportCursor(); the timer covers a
    // silent failure.
}

void KWinCursorSource::stopScript(const QString &dbusPath) {
    if (dbusPath.isEmpty())
        return;
    QDBusMessage stop = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService), dbusPath,
        QString::fromLatin1(kKWinScriptIface), QStringLiteral("stop"));
    QDBusConnection::sessionBus().asyncCall(stop);
}

void KWinCursorSource::reportCursor(int requestId, int x, int y) {
    // Correlation only, see isReplyForActiveQuery(): it tells the live query's
    // reply apart from one that outlived its own query, and it makes a call to
    // an idle source a no-op. It is not an access check, the id sits in plain
    // text in the script file.
    if (!isReplyForActiveQuery(requestId, activeRequestId_))
        return;
    resolve(CursorPos{x, y});
}

void KWinCursorSource::resolve(std::optional<CursorPos> pos) {
    timer_->stop();
    activeRequestId_ = kNoRequest;
    if (!currentScriptPath_.isEmpty()) {
        // Unload the one-shot script so repeated opens don't pile up script
        // instances inside KWin. The script has already run by now, so its file
        // is no longer needed either.
        stopScript(currentScriptPath_);
        currentScriptPath_.clear();
    }
    // On the normal path the script has already run, so its file is spent. When
    // a query is given up on instead (superseded, timed out), its loadScript may
    // still be in flight and KWin may not have read the file yet: deleting it
    // here makes that load fail, which is precisely what a dropped query wants,
    // and the id != activeRequestId_ branch in getCursor() catches the reply
    // either way. So the delete is unconditional on purpose.
    removeScriptFile(currentScriptFile_);
    currentScriptFile_.clear();
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
