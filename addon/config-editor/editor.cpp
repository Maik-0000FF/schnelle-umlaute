#include "editor.h"
#include "model.h"
#include <fcitx-utils/i18n.h>
#include <QApplication>
#include <QScreen>
#include <QShowEvent>
#include <QLabel>

namespace fcitx {

MappingEditor::MappingEditor(QWidget *parent)
    : FcitxQtConfigUIWidget(parent) {
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

    // Validation error label
    inputStatus_ = new QLabel(this);
    inputStatus_->setStyleSheet("QLabel { color: #cc0000; font-size: 11px; }");
    inputStatus_->setVisible(false);
    mainLayout->addWidget(inputStatus_, 3, 0);

    connect(addButton, &QPushButton::clicked, this,
            &MappingEditor::addMapping);
    connect(deleteButton, &QPushButton::clicked, this,
            &MappingEditor::deleteMapping);
    connect(moveUpButton, &QPushButton::clicked, this, [this]() {
        if (auto idx = mappingView->currentIndex(); idx.isValid() && idx.row() > 0) {
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

    connect(mappingView->selectionModel(),
            &QItemSelectionModel::currentChanged, this,
            &MappingEditor::itemFocusChanged);
    connect(model_, &MappingModel::needSaveChanged, this,
            &MappingEditor::changed);

    connect(inputEdit, &QLineEdit::returnPressed, this,
            &MappingEditor::addMapping);
    connect(outputEdit, &QLineEdit::returnPressed, this,
            &MappingEditor::addMapping);
    connect(inputEdit, &QLineEdit::textChanged, this, [this]() {
        clearInputError();
        auto text = inputEdit->text().trimmed();
        if (!text.isEmpty()) {
            if (!MappingModel::isValidInput(text)) {
                showInputError(_("Input must be exactly one printable character"));
            } else if (model_->hasInput(text)) {
                showInputError(_("This input key is already mapped"));
            }
        }
    });

    load();
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
    auto output = outputEdit->text().trimmed();
    if (input.isEmpty() || output.isEmpty()) {
        return;
    }
    if (!MappingModel::isValidInput(input)) {
        showInputError(_("Input must be exactly one printable character"));
        return;
    }
    if (model_->hasInput(input)) {
        showInputError(_("This input key is already mapped"));
        return;
    }
    clearInputError();
    auto idx = model_->addItem(input, output);
    mappingView->setCurrentIndex(idx);
    inputEdit->clear();
    outputEdit->clear();
    inputEdit->setFocus();
}

void MappingEditor::showInputError(const QString &msg) {
    inputStatus_->setText(msg);
    inputStatus_->setVisible(true);
    inputEdit->setStyleSheet("QLineEdit { border: 1px solid #cc0000; }");
}

void MappingEditor::clearInputError() {
    inputStatus_->setVisible(false);
    inputEdit->setStyleSheet("");
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
    moveDownButton->setEnabled(
        sel && mappingView->currentIndex().row() + 1 < model_->rowCount());
}

} // namespace fcitx
