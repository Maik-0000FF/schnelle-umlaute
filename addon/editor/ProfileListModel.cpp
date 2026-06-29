#include "ProfileListModel.h"
#include "FcitxReload.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

QString configDir() {
    auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/fcitx5/schnelle-umlaute/");
}

QString profilesConfPath() { return configDir() + QStringLiteral("profiles.conf"); }

} // namespace

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
    QDir dir(configDir() + QStringLiteral("profiles"));
    int n = 1;
    for (;;) {
        QString slug = (n == 1) ? base : base + QStringLiteral("-") +
                                              QString::number(n);
        QString rel = QStringLiteral("profiles/") + slug + QStringLiteral(".txt");
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
    if (entries_[row].file == QString::fromLatin1(kStandardFile))
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
        return QString::fromLatin1(kStandardFile);
    return entries_[row].file;
}

int ProfileListModel::activeRow() const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (entries_[i].name == active_)
            return i;
    return entries_.empty() ? -1 : 0;
}

bool ProfileListModel::createProfile(const QString &name) {
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
    // No mapping file is written here: until the profile is first edited and
    // saved, both editor and engine fall back to the default mappings, so a new
    // profile starts from the defaults.
    save();
    return true;
}

bool ProfileListModel::renameProfile(int row, const QString &name) {
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
    // Delete the profile's mappings file (never mappings.txt — Standard is
    // protected above, so file is always a profiles/*.txt here).
    if (file != QString::fromLatin1(kStandardFile)) {
        QFile::remove(configDir() + file);
    }
    // If the active profile was removed, fall back to the first profile.
    if (wasActive) {
        active_ = entries_.empty() ? QString::fromLatin1(kStandardName)
                                   : entries_.front().name;
        Q_EMIT activeChanged();
    }
    save();
    return true;
}

bool ProfileListModel::setActiveRow(int row) {
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

bool ProfileListModel::setSelectKey(int row, const QString &combo) {
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return false;
    entries_[row].selectKey = combo.trimmed();
    auto idx = index(row);
    Q_EMIT dataChanged(idx, idx, {SelectKeyRole});
    save();
    return true;
}

void ProfileListModel::setCycleNext(const QString &combo) {
    QString c = combo.trimmed();
    if (c == cycleNext_)
        return;
    cycleNext_ = c;
    Q_EMIT cycleNextChanged();
    save();
}

void ProfileListModel::setCyclePrev(const QString &combo) {
    QString c = combo.trimmed();
    if (c == cyclePrev_)
        return;
    cyclePrev_ = c;
    Q_EMIT cyclePrevChanged();
    save();
}

void ProfileListModel::seedStandardIfEmpty(bool persist) {
    if (!entries_.empty())
        return;
    Entry e;
    e.name = QString::fromLatin1(kStandardName);
    e.file = QString::fromLatin1(kStandardFile);
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
                    !nameExists(cur.name, -1) && !fileExists(cur.file, -1))
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
            int eq = t.indexOf(QChar('='));
            if (eq < 0)
                continue;
            QString key = t.left(eq).trimmed();
            QString val = t.mid(eq + 1).trimmed();
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
            }
        }
        flush();
        f.close();
    }

    // Fresh install / pre-profiles upgrade: materialize the Standard profile
    // (pointing at the existing mappings.txt, which is never rewritten here).
    if (entries_.empty()) {
        seedStandardIfEmpty(/*persist=*/true);
    } else if (active_.isEmpty() || activeRow() < 0) {
        // Unknown/absent active → fall back to the first profile.
        active_ = entries_.front().name;
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
    ts << "# Mapping profiles for schnelle-umlaute. Managed by the editor.\n";
    ts << "Active=" << active_ << "\n";
    ts << "CycleNext=" << cycleNext_ << "\n";
    ts << "CyclePrev=" << cyclePrev_ << "\n";
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const auto &e = entries_[i];
        ts << "\n[Profiles/" << i << "]\n";
        ts << "Name=" << e.name << "\n";
        ts << "File=" << e.file << "\n";
        ts << "SelectKey=" << e.selectKey << "\n";
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
