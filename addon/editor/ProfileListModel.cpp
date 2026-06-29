#include "ProfileListModel.h"
#include "FcitxReload.h"
#include "editor_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace {

using schnelle_umlaute::kMappingsFile;
using schnelle_umlaute::kProfilesConf;
using schnelle_umlaute::kProfilesSubdir;
using schnelle_umlaute::kStandardProfile;

// Longest slug accepted, so a pathological profile name can't produce a
// filename that hits the filesystem's NAME_MAX on save.
constexpr int kMaxSlugLength = 64;

QString configDir() { return schnelle_umlaute::configDirPath(); }

QString profilesConfPath() {
    return configDir() + QLatin1String(kProfilesConf);
}

} // namespace

// Mirror of fcitx stringutils::escapeForValue: quote the value when it
// contains whitespace, a quote or a backslash, and backslash-escape the
// special characters. Keeps the editor-written profiles.conf byte-compatible
// with what the engine's safeSaveAsIni would produce.
QString ProfileListModel::escapeValue(const QString &s) {
    bool needEscape = false;
    for (QChar c : s) {
        char16_t u = c.unicode();
        if (u == '\f' || u == '\r' || u == '\t' || u == '\v' || u == ' ' ||
            u == '"' || u == '\\' || u == '\n') {
            needEscape = true;
            break;
        }
    }
    QString out;
    if (needEscape)
        out += QChar('"');
    for (QChar c : s) {
        char16_t u = c.unicode();
        char esc = 0;
        switch (u) {
        case '\\': esc = '\\'; break;
        case '"': esc = '"'; break;
        case '\n': esc = 'n'; break;
        case '\f': esc = 'f'; break;
        case '\r': esc = 'r'; break;
        case '\t': esc = 't'; break;
        case '\v': esc = 'v'; break;
        default: break;
        }
        if (esc) {
            out += QChar('\\');
            out += QChar(esc);
        } else {
            out += c;
        }
    }
    if (needEscape)
        out += QChar('"');
    return out;
}

// Mirror of fcitx stringutils::unescapeForValue: a value wrapped in quotes is
// unescaped; anything else is returned verbatim. A malformed quoted value
// (not closed exactly at the end) is returned raw, best-effort.
QString ProfileListModel::unescapeValue(const QString &s) {
    if (s.size() < 2 || s.front() != QChar('"') || s.back() != QChar('"'))
        return s;
    QString result;
    bool escape = false;
    bool closed = false;
    qsizetype i = 1;
    for (; i < s.size(); ++i) {
        QChar c = s[i];
        if (!escape) {
            if (c == QChar('\\')) {
                escape = true;
            } else if (c == QChar('"')) {
                ++i;
                closed = true;
                break;
            } else {
                result += c;
            }
        } else {
            char16_t u = c.unicode();
            switch (u) {
            case '\\': result += QChar('\\'); break;
            case '"': result += QChar('"'); break;
            case 'n': result += QChar('\n'); break;
            case 'f': result += QChar('\f'); break;
            case 'r': result += QChar('\r'); break;
            case 't': result += QChar('\t'); break;
            case 'v': result += QChar('\v'); break;
            default: result += c; break; // unknown escape: keep literal
            }
            escape = false;
        }
    }
    if (closed && i == s.size())
        return result;
    return s;
}

// Delegates to the shared predicate (profile_paths.h) so the editor and the
// engine apply the exact same "File must stay under profiles/" rule.
bool ProfileListModel::isSafeProfileFile(const QString &file) {
    return schnelle_umlaute::isSafeProfileFile(file.toStdString());
}

ProfileListModel::ProfileListModel(QObject *parent)
    : QAbstractListModel(parent) {
    load();
}

int ProfileListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return static_cast<int>(entries_.size());
}

QVariant ProfileListModel::data(const QModelIndex &index, int role) const {
    int row = index.row();
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return {};
    const auto &e = entries_[row];
    switch (role) {
    case NameRole:
        return e.name;
    case FileRole:
        return e.file;
    case SelectKeyRole:
        return e.selectKey;
    case IsActiveRole:
        return e.name == active_;
    case IsProtectedRole:
        return isProtected(row);
    case FavoriteRole:
        return e.favorite;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProfileListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {FileRole, "file"},
        {SelectKeyRole, "selectKey"},
        {IsActiveRole, "isActive"},
        {IsProtectedRole, "isProtected"},
        {FavoriteRole, "favorite"},
    };
}

QString ProfileListModel::normalizedName(const QString &name) {
    return name.trimmed().toLower();
}

QString ProfileListModel::slugify(const QString &name) {
    QString slug;
    bool lastDash = false;
    for (QChar c : name.toLower()) {
        if ((c >= QChar('a') && c <= QChar('z')) ||
            (c >= QChar('0') && c <= QChar('9'))) {
            slug += c;
            lastDash = false;
        } else if (!slug.isEmpty() && !lastDash) {
            slug += QChar('-');
            lastDash = true;
        }
    }
    if (slug.size() > kMaxSlugLength)
        slug.truncate(kMaxSlugLength);
    while (slug.endsWith(QChar('-')))
        slug.chop(1);
    if (slug.isEmpty())
        slug = QStringLiteral("profile");
    return slug;
}

bool ProfileListModel::nameExists(const QString &name, int excludeRow) const {
    QString norm = normalizedName(name);
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (i == excludeRow)
            continue;
        if (normalizedName(entries_[i].name) == norm)
            return true;
    }
    return false;
}

bool ProfileListModel::fileExists(const QString &file, int excludeRow) const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (i == excludeRow)
            continue;
        if (entries_[i].file == file)
            return true;
    }
    return false;
}

// Build a unique "profiles/<slug>.txt" for a display name, suffixing -2, -3 …
// if the base slug (or an existing file on disk) would collide.
QString ProfileListModel::uniqueSlugFile(const QString &name) const {
    QString base = slugify(name);
    QDir dir(configDir() + QLatin1String(kProfilesSubdir));
    const QString prefix = QLatin1String(kProfilesSubdir) + QStringLiteral("/");
    int n = 1;
    for (;;) {
        QString slug = (n == 1) ? base : base + QStringLiteral("-") +
                                              QString::number(n);
        QString rel = prefix + slug + QStringLiteral(".txt");
        if (!fileExists(rel, -1) && !dir.exists(slug + QStringLiteral(".txt")))
            return rel;
        ++n;
    }
}

QString ProfileListModel::nameErrorFor(const QString &name,
                                       int excludeRow) const {
    QString t = name.trimmed();
    if (t.isEmpty())
        return tr("Name must not be empty");
    if (t.contains(QChar('/')) || t.contains(QChar('\\')) ||
        t.contains(QChar('\n')) || t.contains(QChar('\r')))
        return tr("Name must not contain slashes or line breaks");
    if (nameExists(t, excludeRow))
        return tr("A profile with this name already exists");
    return {};
}

bool ProfileListModel::isProtected(int row) const {
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return true;
    // The Standard profile (its file holds the user's original mappings.txt)
    // and the last remaining profile cannot be deleted.
    if (entries_[row].file == QLatin1String(kMappingsFile))
        return true;
    return entries_.size() <= 1;
}

QStringList ProfileListModel::profileNames() const {
    QStringList names;
    names.reserve(static_cast<int>(entries_.size()));
    for (const auto &e : entries_)
        names << e.name;
    return names;
}

QString ProfileListModel::fileForRow(int row) const {
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return QLatin1String(kMappingsFile);
    return entries_[row].file;
}

int ProfileListModel::activeRow() const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (entries_[i].name == active_)
            return i;
    return entries_.empty() ? -1 : 0;
}

bool ProfileListModel::createProfile(const QString &name) {
    reloadActiveFromDisk();
    QString err = nameErrorFor(name, -1);
    if (!err.isEmpty()) {
        Q_EMIT errorOccurred(err);
        return false;
    }
    Entry e;
    e.name = name.trimmed();
    e.file = uniqueSlugFile(e.name);
    int row = static_cast<int>(entries_.size());
    beginInsertRows(QModelIndex(), row, row);
    entries_.push_back(e);
    endInsertRows();
    Q_EMIT countChanged();
    // No mapping file is written here: a new profile starts empty (the German
    // defaults are seeded only for the Standard profile, in both the editor
    // model and the engine loader). The file is created on the first save once
    // the user adds mappings.
    save();
    return true;
}

bool ProfileListModel::renameProfile(int row, const QString &name) {
    reloadActiveFromDisk();
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    QString err = nameErrorFor(name, row);
    if (!err.isEmpty()) {
        Q_EMIT errorOccurred(err);
        return false;
    }
    QString old = entries_[row].name;
    entries_[row].name = name.trimmed();
    // The File stays stable (Standard keeps mappings.txt); only the display
    // name changes. Keep the active pointer in sync if we renamed the active.
    if (active_ == old)
        active_ = entries_[row].name;
    auto idx = index(row);
    Q_EMIT dataChanged(idx, idx, {NameRole, IsActiveRole});
    Q_EMIT activeChanged();
    save();
    return true;
}

bool ProfileListModel::removeProfile(int row) {
    reloadActiveFromDisk();
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    if (isProtected(row)) {
        Q_EMIT errorOccurred(tr("This profile cannot be deleted"));
        return false;
    }
    QString file = entries_[row].file;
    bool wasActive = (entries_[row].name == active_);
    beginRemoveRows(QModelIndex(), row, row);
    entries_.erase(entries_.begin() + row);
    endRemoveRows();
    Q_EMIT countChanged();
    // Delete the profile's mappings file (never mappings.txt: Standard is
    // protected above, and isSafeProfileFile keeps file under profiles/, so a
    // malformed entry can't aim QFile::remove outside the config dir).
    if (file != QLatin1String(kMappingsFile) && isSafeProfileFile(file)) {
        QFile::remove(configDir() + file);
    }
    // If the active profile was removed, fall back to the first profile and
    // refresh that row's active marker (the delegate's isActive only updates
    // on dataChanged).
    if (wasActive) {
        active_ = entries_.empty() ? QLatin1String(kStandardProfile)
                                   : entries_.front().name;
        Q_EMIT activeChanged();
        if (!entries_.empty()) {
            auto idx = index(0);
            Q_EMIT dataChanged(idx, idx, {IsActiveRole});
        }
    }
    // Let the editor reset its edit target if it was pointing at this file.
    Q_EMIT profileRemoved(file);
    save();
    return true;
}

bool ProfileListModel::setActiveRow(int row) {
    reloadActiveFromDisk();
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    if (entries_[row].name == active_)
        return true;
    QString old = active_;
    active_ = entries_[row].name;
    Q_EMIT activeChanged();
    // Refresh isActive on the old and new rows.
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].name == active_ || entries_[i].name == old) {
            auto idx = index(i);
            Q_EMIT dataChanged(idx, idx, {IsActiveRole});
        }
    }
    save();
    return true;
}

bool ProfileListModel::isComboFree(const QString &combo, int excludeRow,
                                   int excludeCycle) const {
    if (combo.isEmpty())
        return true;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (i == excludeRow)
            continue;
        if (combo.compare(entries_[i].selectKey, Qt::CaseInsensitive) == 0)
            return false;
    }
    if (excludeCycle != 1 && combo.compare(cycleNext_, Qt::CaseInsensitive) == 0)
        return false;
    if (excludeCycle != 2 && combo.compare(cyclePrev_, Qt::CaseInsensitive) == 0)
        return false;
    return true;
}

bool ProfileListModel::setSelectKey(int row, const QString &combo) {
    reloadActiveFromDisk();
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    QString c = combo.trimmed();
    if (!isComboFree(c, row, 0)) {
        Q_EMIT errorOccurred(tr("Shortcut already in use"));
        return false;
    }
    entries_[row].selectKey = c;
    auto idx = index(row);
    Q_EMIT dataChanged(idx, idx, {SelectKeyRole});
    save();
    return true;
}

bool ProfileListModel::setFavorite(int row, bool favorite) {
    reloadActiveFromDisk();
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    if (entries_[row].favorite == favorite)
        return true;
    entries_[row].favorite = favorite;
    auto idx = index(row);
    Q_EMIT dataChanged(idx, idx, {FavoriteRole});
    save();
    return true;
}

void ProfileListModel::setCycleNext(const QString &combo) {
    reloadActiveFromDisk();
    QString c = combo.trimmed();
    if (c == cycleNext_)
        return;
    if (!isComboFree(c, -1, 1)) {
        Q_EMIT errorOccurred(tr("Shortcut already in use"));
        return;
    }
    cycleNext_ = c;
    Q_EMIT cycleNextChanged();
    save();
}

void ProfileListModel::setCyclePrev(const QString &combo) {
    reloadActiveFromDisk();
    QString c = combo.trimmed();
    if (c == cyclePrev_)
        return;
    if (!isComboFree(c, -1, 2)) {
        Q_EMIT errorOccurred(tr("Shortcut already in use"));
        return;
    }
    cyclePrev_ = c;
    Q_EMIT cyclePrevChanged();
    save();
}

void ProfileListModel::seedStandardIfEmpty(bool persist) {
    if (!entries_.empty())
        return;
    Entry e;
    e.name = QLatin1String(kStandardProfile);
    e.file = QLatin1String(kMappingsFile);
    entries_.push_back(e);
    if (active_.isEmpty())
        active_ = e.name;
    if (persist)
        save();
}

void ProfileListModel::load() {
    entries_.clear();
    active_.clear();
    cycleNext_.clear();
    cyclePrev_.clear();

    QFile f(profilesConfPath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        QString section;
        Entry cur;
        bool inEntry = false;
        auto flush = [&]() {
            if (inEntry) {
                if (!cur.name.isEmpty() && !cur.file.isEmpty() &&
                    isSafeProfileFile(cur.file) && !nameExists(cur.name, -1) &&
                    !fileExists(cur.file, -1))
                    entries_.push_back(cur);
                cur = Entry{};
                inEntry = false;
            }
        };
        while (!in.atEnd()) {
            QString t = in.readLine().trimmed();
            if (t.isEmpty() || t.startsWith(QChar('#')))
                continue;
            if (t.startsWith(QChar('[')) && t.endsWith(QChar(']'))) {
                flush();
                section = t.mid(1, t.size() - 2);
                inEntry = section.startsWith(QStringLiteral("Profiles/"));
                continue;
            }
            qsizetype eq = t.indexOf(QChar('='));
            if (eq < 0)
                continue;
            QString key = t.left(eq).trimmed();
            // Values may be fcitx-quoted (e.g. when the engine wrote the file
            // via safeSaveAsIni); unescape them so they match what the editor
            // stored. See escapeValue/unescapeValue.
            QString val = unescapeValue(t.mid(eq + 1).trimmed());
            if (section.isEmpty()) {
                if (key == QStringLiteral("Active"))
                    active_ = val;
                else if (key == QStringLiteral("CycleNext"))
                    cycleNext_ = val;
                else if (key == QStringLiteral("CyclePrev"))
                    cyclePrev_ = val;
            } else if (inEntry) {
                if (key == QStringLiteral("Name"))
                    cur.name = val;
                else if (key == QStringLiteral("File"))
                    cur.file = val;
                else if (key == QStringLiteral("SelectKey"))
                    cur.selectKey = val;
                else if (key == QStringLiteral("Favorite"))
                    cur.favorite = (val.compare(QStringLiteral("True"),
                                                Qt::CaseInsensitive) == 0 ||
                                    val == QStringLiteral("1"));
            }
        }
        flush();
        f.close();
    }

    // Fresh install / pre-profiles upgrade: materialize the Standard profile
    // (pointing at the existing mappings.txt, which is never rewritten here).
    if (entries_.empty()) {
        // Drop any dangling Active read from an otherwise-empty/corrupt file so
        // the seeded Standard becomes active.
        active_.clear();
        seedStandardIfEmpty(/*persist=*/true);
    } else if (active_.isEmpty() || activeRow() < 0) {
        // Unknown/absent active falls back to the first profile.
        active_ = entries_.front().name;
    }
}

void ProfileListModel::reloadActiveFromDisk() {
    QFile f(profilesConfPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    QString diskActive;
    while (!in.atEnd()) {
        QString t = in.readLine().trimmed();
        if (t.startsWith(QChar('['))) // top-level keys only (before sections)
            break;
        if (t.startsWith(QStringLiteral("Active="))) {
            diskActive = unescapeValue(t.mid(7).trimmed());
            break;
        }
    }
    f.close();

    if (diskActive.isEmpty() || diskActive == active_)
        return;
    // Adopt only if it names a profile we know; otherwise keep the current one.
    bool known = false;
    for (const auto &e : entries_) {
        if (e.name == diskActive) {
            known = true;
            break;
        }
    }
    if (!known)
        return;
    const QString old = active_;
    active_ = diskActive;
    Q_EMIT activeChanged();
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].name == active_ || entries_[i].name == old) {
            auto idx = index(i);
            Q_EMIT dataChanged(idx, idx, {IsActiveRole});
        }
    }
}

bool ProfileListModel::save() {
    QString path = profilesConfPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    QString out;
    QTextStream ts(&out);
    // Values are escaped the way fcitx's safeSaveAsIni would, so the engine's
    // readAsIni round-trips them identically (e.g. a profile name with spaces).
    ts << "# Mapping profiles for schnelle-umlaute. Managed by the editor.\n";
    ts << "Active=" << escapeValue(active_) << "\n";
    ts << "CycleNext=" << escapeValue(cycleNext_) << "\n";
    ts << "CyclePrev=" << escapeValue(cyclePrev_) << "\n";
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const auto &e = entries_[i];
        ts << "\n[Profiles/" << i << "]\n";
        ts << "Name=" << escapeValue(e.name) << "\n";
        ts << "File=" << escapeValue(e.file) << "\n";
        ts << "SelectKey=" << escapeValue(e.selectKey) << "\n";
        ts << "Favorite=" << (e.favorite ? "True" : "False") << "\n";
    }
    ts.flush();
    QByteArray buf = out.toUtf8();
    if (file.write(buf) != buf.size() || !file.commit()) {
        Q_EMIT errorOccurred(file.errorString());
        return false;
    }
    ++revision_;
    Q_EMIT changed();
    reloadSchnelleUmlauteAddon();
    return true;
}
