#ifndef SCHNELLE_UMLAUTE_EDITOR_PRESET_META_H
#define SCHNELLE_UMLAUTE_EDITOR_PRESET_META_H

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>

// Metadata for a profile mapping file, used both for the bundled-preset picker
// and for auto-registering a loose file a user dropped into profiles/. A preset
// file is a normal mapping file (input=output lines) that may carry optional
// leading "# Name:" / "# Description:" comment headers. The mapping parser skips
// '#' lines, so these headers never reach the engine; only the editor reads them
// here. Keeping name derivation in one place means the picker and the loose-file
// scan always agree on a file's display name.

namespace schnelle_umlaute {

struct PresetMeta {
    QString name;        // display name (header "# Name:" or titleized base)
    QString description; // optional ("# Description:")
    int mappingCount = 0;
};

// "francais" -> "Francais", "math-physik" -> "Math Physik". The fallback when a
// file carries no "# Name:" header; word separators are '-' and '_'.
inline QString titleizeSlug(const QString &slug) {
    const QStringList words =
        slug.split(QRegularExpression(QStringLiteral("[-_]")),
                   Qt::SkipEmptyParts);
    QStringList out;
    out.reserve(words.size());
    for (const QString &w : words)
        out << (w.left(1).toUpper() + w.mid(1));
    return out.isEmpty() ? slug : out.join(QChar(' '));
}

// Read a mapping file: pick up the optional leading "# Name:"/"# Description:"
// header comments (recognized only in the leading comment block, before the
// first mapping) and count the input=output lines. `baseName` is the file's
// slug (its name without ".txt"); it titleizes into the fallback display name
// when there is no "# Name:" header. A missing/unreadable file yields just the
// titleized base name.
inline PresetMeta readPresetMeta(const QString &path, const QString &baseName) {
    PresetMeta meta;
    meta.name = titleizeSlug(baseName);

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return meta;

    bool inHeader = true; // leading comment block only
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString raw = in.readLine();
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QChar('#'))) {
            if (!inHeader)
                continue;
            // Strip the leading '#'(s) and whitespace, then split key: value.
            QString body = line;
            while (body.startsWith(QChar('#')))
                body.remove(0, 1);
            body = body.trimmed();
            const qsizetype colon = body.indexOf(QChar(':'));
            if (colon < 0)
                continue;
            const QString key = body.left(colon).trimmed().toLower();
            const QString val = body.mid(colon + 1).trimmed();
            if (key == QStringLiteral("name") && !val.isEmpty())
                meta.name = val;
            else if (key == QStringLiteral("description"))
                meta.description = val;
            continue;
        }
        // First non-comment line ends the header block; mappings have a '='.
        inHeader = false;
        if (line.contains(QChar('=')))
            ++meta.mappingCount;
    }
    return meta;
}

} // namespace schnelle_umlaute

#endif
