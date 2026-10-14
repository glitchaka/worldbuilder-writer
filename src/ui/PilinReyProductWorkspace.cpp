#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QFrame>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {

void PilinReyEditor::configureProductWorkspace() {
    if (property("wbwProductWorkspaceV2").toBool()) {
        if (layersPopover_) layersPopover_->show();
        if (assetsPopover_) assetsPopover_->show();
        layoutFloatingPanels();
        return;
    }
    setProperty("wbwProductWorkspaceV2", true);

    if (toolRail_) {
        toolRail_->setMinimumWidth(54);
        toolRail_->setMaximumWidth(54);
    }
    if (topCommands_) {
        topCommands_->setMinimumHeight(46);
        topCommands_->setMaximumHeight(52);
    }

    for (QFrame* panel : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_}) {
        if (!panel) continue;
        panel->setMinimumWidth(300);
        panel->setMaximumWidth(340);
    }

    if (layers_) {
        layers_->setMinimumHeight(180);
        layers_->setMaximumHeight(260);
        layers_->setAlternatingRowColors(false);
    }
    if (assets_) {
        assets_->setMinimumHeight(170);
        assets_->setMaximumHeight(250);
    }

    if (assetsPopover_ && assets_) {
        if (auto* layout = qobject_cast<QVBoxLayout*>(assetsPopover_->layout())) {
            auto* search = new QLineEdit(assetsPopover_);
            search->setObjectName(QStringLiteral("pilinAssetSearch"));
            search->setPlaceholderText(tr("Buscar símbolos…"));
            search->setClearButtonEnabled(true);

            auto* categories = new QComboBox(assetsPopover_);
            categories->setObjectName(QStringLiteral("pilinAssetCategory"));

            auto* importFolder = new QPushButton(tr("Importar carpeta de símbolos…"), assetsPopover_);
            importFolder->setObjectName(QStringLiteral("pilinImportAssetFolder"));
            importFolder->setToolTip(tr("Importa PNG, JPG y WebP propios o con licencia compatible. No se generan recursos mediante IA."));

            layout->insertWidget(1, search);
            layout->insertWidget(2, categories);
            layout->addWidget(importFolder);

            const auto refreshCategories = [this, categories]() {
                const QSignalBlocker blocker(categories);
                const QString previous = categories->currentText();
                categories->clear();
                categories->addItem(tr("Todos"));
                QStringList values;
                for (const QJsonValue& value : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray()) {
                    const QString category = value.toObject().value(QStringLiteral("category")).toString().trimmed();
                    if (!category.isEmpty() && !values.contains(category, Qt::CaseInsensitive)) values.append(category);
                }
                values.sort(Qt::CaseInsensitive);
                categories->addItems(values);
                const int index = categories->findText(previous);
                categories->setCurrentIndex(index >= 0 ? index : 0);
            };

            const auto filterAssets = [this, search, categories]() {
                const QString query = search->text().trimmed();
                const QString category = categories->currentIndex() > 0 ? categories->currentText() : QString();
                const QJsonArray values = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray();
                for (int row = 0; row < assets_->count(); ++row) {
                    QListWidgetItem* item = assets_->item(row);
                    const QJsonObject asset = row < values.size() ? values.at(row).toObject() : QJsonObject();
                    const bool textMatches = query.isEmpty()
                        || asset.value(QStringLiteral("name")).toString().contains(query, Qt::CaseInsensitive);
                    const bool categoryMatches = category.isEmpty()
                        || asset.value(QStringLiteral("category")).toString().compare(category, Qt::CaseInsensitive) == 0;
                    item->setHidden(!(textMatches && categoryMatches));
                }
            };

            refreshCategories();
            connect(search, &QLineEdit::textChanged, this, [filterAssets](const QString&) { filterAssets(); });
            connect(categories, &QComboBox::currentTextChanged, this, [filterAssets](const QString&) { filterAssets(); });
            connect(importFolder, &QPushButton::clicked, this, [this, refreshCategories, filterAssets]() {
                importAssetDirectory();
                refreshCategories();
                filterAssets();
            });
        }
    }

    QTimer::singleShot(0, this, [this]() {
        if (layersPopover_) layersPopover_->show();
        if (assetsPopover_) assetsPopover_->show();
        layoutFloatingPanels();
        if (layersPopover_) layersPopover_->raise();
        if (assetsPopover_) assetsPopover_->raise();
        if (toolRail_) toolRail_->raise();
        if (topCommands_) topCommands_->raise();
    });
}

} // namespace wbw
