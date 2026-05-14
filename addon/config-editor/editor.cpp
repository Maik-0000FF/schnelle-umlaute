#include "editor.h"
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <fcitx-utils/i18n.h>
#include "model.h"

namespace fcitx {

MappingEditor::MappingEditor(QWidget *parent) : FcitxQtConfigUIWidget(parent) {
    setupUi(this);

    model_ = new MappingModel(this);
    mappingView->horizontalHeader()->setStretchLastSection(true);
    mappingView->verticalHeader()->setVisible(false);
    mappingView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mappingView->setSelectionMode(QAbstractItemView::SingleSelection);
    mappingView->setModel(model_);

    // Help text below add fields
    auto *helpLabel = new QLabel(
        _("Input: single key. Output: text or comma-separated cycling "
          "variants (e.g. é,è,ê,ë). Use double comma for literal comma "
          "(e.g. Hello,, World)."),
        this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("QLabel { color: gray; font-size: 11px; }");
    mainLayout->addWidget(helpLabel, 2, 0);

    // Validation error label. Always visible (with a placeholder space
    // character when there's no message) so its line height is reserved
    // and the rest of the form doesn't jump up/down as messages appear.
    statusLabel_ = new QLabel(QStringLiteral(" "), this);
    statusLabel_->setStyleSheet("QLabel { color: #cc0000; font-size: 11px; }");
    mainLayout->addWidget(statusLabel_, 3, 0);

    connect(addButton, &QPushButton::clicked, this, &MappingEditor::addMapping);
    connect(deleteButton, &QPushButton::clicked, this,
            &MappingEditor::deleteMapping);
    connect(moveUpButton, &QPushButton::clicked, this, [this]() {
        if (auto idx = mappingView->currentIndex();
            idx.isValid() && idx.row() > 0) {
            model_->moveUp(idx.row());
            mappingView->setCurrentIndex(model_->index(idx.row() - 1, 0));
        }
    });
    connect(moveDownButton, &QPushButton::clicked, this, [this]() {
        if (auto idx = mappingView->currentIndex();
            idx.isValid() && idx.row() + 1 < model_->rowCount()) {
            model_->moveDown(idx.row());
            mappingView->setCurrentIndex(model_->index(idx.row() + 1, 0));
        }
    });

    connect(mappingView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MappingEditor::itemFocusChanged);
    connect(model_, &MappingModel::needSaveChanged, this,
            &MappingEditor::changed);

    connect(inputEdit, &QLineEdit::returnPressed, this,
            &MappingEditor::addMapping);
    connect(outputEdit, &QLineEdit::returnPressed, this,
            &MappingEditor::addMapping);
    connect(inputEdit, &QLineEdit::textChanged, this,
            &MappingEditor::revalidate);
    connect(outputEdit, &QLineEdit::textChanged, this,
            &MappingEditor::revalidate);

    loadLeaderKeys();
    // Qualified call: virtual dispatch is inert in constructors,
    // being explicit silences clang-analyzer-optin.cplusplus.VirtualCall.
    MappingEditor::load();
    itemFocusChanged();
}

QString MappingEditor::icon() { return "input-keyboard"; }
QString MappingEditor::title() { return _("Mapping Editor"); }

QSize MappingEditor::sizeHint() const {
    if (auto *screen = QApplication::primaryScreen()) {
        auto geo = screen->availableGeometry();
        int w = qBound(560, static_cast<int>(geo.width() * 0.4), 1400);
        int h = qBound(480, static_cast<int>(geo.height() * 0.5), 1000);
        return {w, h};
    }
    return {640, 500};
}

void MappingEditor::showEvent(QShowEvent *event) {
    FcitxQtConfigUIWidget::showEvent(event);
    if (auto *win = window(); win && win != this) {
        auto hint = sizeHint();
        if (win->width() < hint.width() || win->height() < hint.height()) {
            win->resize(win->size().expandedTo(hint));
        }
    }
}

void MappingEditor::load() { model_->load(); }
void MappingEditor::save() { model_->save(); }

void MappingEditor::addMapping() {
    auto input = inputEdit->text().trimmed();
    // Output is NOT trimmed — leading/trailing whitespace is intentional
    // (e.g. " ls" to skip terminal history, or tab characters).
    auto output = outputEdit->text();
    if (input.isEmpty() || output.isEmpty()) {
        return;
    }
    if (!MappingModel::isValidInput(input)) {
        showInputError(
            _("Input must be a visible character (no whitespace, no tab)"));
        return;
    }
    if (model_->hasInput(input)) {
        showInputError(_("This input key is already mapped"));
        return;
    }
    if (!MappingModel::isValidOutput(output)) {
        showOutputError(_("Output must not contain line breaks"));
        return;
    }
    clearInputError();
    auto idx = model_->addItem(input, output);
    mappingView->setCurrentIndex(idx);
    inputEdit->clear();
    outputEdit->clear();
    inputEdit->setFocus();
}

// Style snippets for inline validation feedback. Object-name selector
// (QLineEdit#inputEdit) gives higher specificity than QLineEdit alone, so
// KDE/Qt themes that style QLineEdit globally don't suppress these borders.
// background-color: palette(base) is the key trick — without an explicit
// background, Qt keeps using the theme's background draw path (which is
// often a 9-slice image with square corners) and border-radius is rendered
// underneath that, invisibly. Setting the palette-base background forces
// Qt onto its own draw path that honors border-radius.
// 1px border + 3px/5px padding matches the size of the unstyled editfield
// in Breeze, so the validated field doesn't visibly shrink vs its neighbors.
static constexpr auto kInputErrorStyle =
    "QLineEdit#inputEdit { border: 1px solid #cc0000; "
    "border-radius: 4px; padding: 3px 5px; "
    "background-color: palette(base); color: palette(text); }";
static constexpr auto kInputWarnStyle =
    "QLineEdit#inputEdit { border: 1px solid #cc8800; "
    "border-radius: 4px; padding: 3px 5px; "
    "background-color: palette(base); color: palette(text); }";
static constexpr auto kOutputErrorStyle =
    "QLineEdit#outputEdit { border: 1px solid #cc0000; "
    "border-radius: 4px; padding: 3px 5px; "
    "background-color: palette(base); color: palette(text); }";

void MappingEditor::showInputError(const QString &msg) {
    statusLabel_->setText(msg);
    statusLabel_->setStyleSheet("QLabel { color: #cc0000; font-size: 11px; }");
    inputEdit->setStyleSheet(kInputErrorStyle);
}

void MappingEditor::showInputWarning(const QString &msg) {
    statusLabel_->setText(msg);
    statusLabel_->setStyleSheet("QLabel { color: #cc8800; font-size: 11px; }");
    inputEdit->setStyleSheet(kInputWarnStyle);
}

void MappingEditor::showOutputError(const QString &msg) {
    statusLabel_->setText(msg);
    statusLabel_->setStyleSheet("QLabel { color: #cc0000; font-size: 11px; }");
    outputEdit->setStyleSheet(kOutputErrorStyle);
}

void MappingEditor::clearInputError() {
    // Reserve the line with a placeholder space — see constructor for why.
    statusLabel_->setText(QStringLiteral(" "));
    inputEdit->setStyleSheet("");
    outputEdit->setStyleSheet("");
}

void MappingEditor::revalidate() {
    clearInputError();
    auto input = inputEdit->text().trimmed();
    // Output is NOT trimmed — leading/trailing whitespace is intentional
    // (matches addMapping()). Only a structural check is run here.
    auto output = outputEdit->text();

    // Input blockers take precedence (same order as addMapping).
    if (!input.isEmpty()) {
        if (!MappingModel::isValidInput(input)) {
            showInputError(
                _("Input must be a visible character (no whitespace, no tab)"));
            return;
        }
        if (model_->hasInput(input)) {
            showInputError(_("This input key is already mapped"));
            return;
        }
    }

    // Output blocker: reject newlines that would corrupt the file format.
    if (!output.isEmpty() && !MappingModel::isValidOutput(output)) {
        showOutputError(_("Output must not contain line breaks"));
        return;
    }

    // Non-blocking warning: leader-key conflict. Only shown when there is
    // no higher-priority error, so the user's attention stays on blockers.
    if (!input.isEmpty() && isLeaderKeyConflict(input)) {
        showInputWarning(_("This key is configured as a Custom Leader — "
                           "it will not work as a mapped input"));
    }
}

void MappingEditor::deleteMapping() {
    if (auto idx = mappingView->currentIndex(); idx.isValid()) {
        model_->deleteItem(idx.row());
    }
}

void MappingEditor::itemFocusChanged() {
    bool sel = mappingView->currentIndex().isValid();
    deleteButton->setEnabled(sel);
    moveUpButton->setEnabled(sel && mappingView->currentIndex().row() > 0);
    moveDownButton->setEnabled(sel && mappingView->currentIndex().row() + 1 <
                                          model_->rowCount());
}

void MappingEditor::loadLeaderKeys() {
    leaderKeys_.clear();
    QString configPath = QStandardPaths::writableLocation(
                             QStandardPaths::GenericConfigLocation) +
                         "/fcitx5/conf/schnelle-umlaute.conf";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("Leader/Custom");
    if (settings.value("CustomKeyEnabled", false).toBool()) {
        QString key = settings.value("CustomKey").toString().trimmed();
        auto ucs4 = key.toUcs4();
        if (!ucs4.isEmpty()) {
            // Copy to char32_t to avoid reinterpret_cast from uint.
            char32_t cp = static_cast<char32_t>(ucs4[0]);
            leaderKeys_ << QString::fromUcs4(&cp, 1);
        }
    }
    if (settings.value("CustomKey2Enabled", false).toBool()) {
        QString key = settings.value("CustomKey2").toString().trimmed();
        auto ucs4 = key.toUcs4();
        if (!ucs4.isEmpty()) {
            char32_t cp = static_cast<char32_t>(ucs4[0]);
            leaderKeys_ << QString::fromUcs4(&cp, 1);
        }
    }
    settings.endGroup();
}

bool MappingEditor::isLeaderKeyConflict(const QString &input) const {
    for (const auto &leader : leaderKeys_) {
        if (input.compare(leader, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace fcitx
