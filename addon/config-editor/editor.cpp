#include "editor.h"
#include "model.h"
#include <fcitx-utils/i18n.h>

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

    connect(addButton, &QPushButton::clicked, this,
            &MappingEditor::addMapping);
    connect(deleteButton, &QPushButton::clicked, this,
            &MappingEditor::deleteMapping);
    connect(moveUpButton, &QPushButton::clicked, this, [this]() {
        if (auto idx = mappingView->currentIndex(); idx.isValid()) {
            model_->moveUp(idx.row());
            mappingView->setCurrentIndex(model_->index(idx.row() - 1, 0));
        }
    });
    connect(moveDownButton, &QPushButton::clicked, this, [this]() {
        if (auto idx = mappingView->currentIndex(); idx.isValid()) {
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

    load();
    itemFocusChanged();
}

QString MappingEditor::icon() { return "input-keyboard"; }
QString MappingEditor::title() { return _("Mapping Editor"); }

void MappingEditor::load() { model_->load(); }
void MappingEditor::save() { model_->save(); }

void MappingEditor::addMapping() {
    auto input = inputEdit->text().trimmed();
    auto output = outputEdit->text().trimmed();
    if (input.isEmpty() || output.isEmpty()) {
        return;
    }
    auto idx = model_->addItem(input, output);
    mappingView->setCurrentIndex(idx);
    inputEdit->clear();
    outputEdit->clear();
    inputEdit->setFocus();
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
