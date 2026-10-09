#pragma once

#include "ui/SemanticTextEdit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QShowEvent>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {

class ArchiveDocument;

class WritingPage final : public QWidget {
    Q_OBJECT
public:
    explicit WritingPage(QWidget* parent = nullptr);

    void setDocument(ArchiveDocument* document);
    void refresh();
    void openFocusMode();

signals:
    void changed();
    void referenceActivated(const QString& kind, const QString& id);

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        if (inspectorOrganized_ || !editorSplit_ || !metadataPanel_ || !editor_) return;
        inspectorOrganized_ = true;

        QWidget* center = editorSplit_->count() > 1 ? editorSplit_->widget(1) : nullptr;
        if (center && center->layout()) center->layout()->removeWidget(metadataPanel_);

        inspectorPanel_ = new QWidget(editorSplit_);
        inspectorPanel_->setObjectName(QStringLiteral("writingInspector"));
        inspectorPanel_->setMinimumWidth(260);
        inspectorPanel_->setMaximumWidth(340);
        auto* inspectorLayout = new QVBoxLayout(inspectorPanel_);
        inspectorLayout->setContentsMargins(0, 0, 0, 0);
        inspectorLayout->setSpacing(0);

        inspectorTabs_ = new QTabWidget(inspectorPanel_);
        inspectorTabs_->setObjectName(QStringLiteral("writingInspectorTabs"));

        metadataPanel_->setParent(inspectorTabs_);
        metadataPanel_->show();
        inspectorTabs_->addTab(metadataPanel_, tr("Detalles"));

        references_ = new QListWidget(inspectorTabs_);
        references_->setObjectName(QStringLiteral("writingReferences"));
        references_->setAlternatingRowColors(false);
        inspectorTabs_->addTab(references_, tr("Referencias"));
        inspectorLayout->addWidget(inspectorTabs_, 1);
        editorSplit_->addWidget(inspectorPanel_);
        editorSplit_->setStretchFactor(0, 0);
        editorSplit_->setStretchFactor(1, 1);
        editorSplit_->setStretchFactor(2, 0);
        editorSplit_->setSizes({250, 1050, 290});

        connect(editor_, &SemanticTextEdit::referenceIndexChanged, this, &WritingPage::updateReferenceInspector, Qt::UniqueConnection);
        connect(references_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            if (!item) return;
            emit referenceActivated(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toString());
        });

        for (QToolButton* button : findChildren<QToolButton*>()) {
            if (button->text() != tr("Detalles")) continue;
            disconnect(button, nullptr, metadataPanel_, nullptr);
            button->setChecked(true);
            connect(button, &QToolButton::toggled, inspectorPanel_, &QWidget::setVisible, Qt::UniqueConnection);
            break;
        }

        setStyleSheet(styleSheet() + QStringLiteral(
            "#writingInspector{background:#ffffff;border:1px solid #d5dce5;}"
            "#writingInspectorTabs::pane{border:0;background:#ffffff;}"
            "#writingReferences{border:0;background:#ffffff;padding:6px;}"
            "#writingReferences::item{padding:7px 8px;border-bottom:1px solid #eef1f5;}"
            "#writingReferences::item:hover{background:#f5f8fc;}"
        ));

        editor_->refreshSemanticReferences();
    }

private:
    QWidget* buildEditorTab();
    QWidget* buildSceneBoardTab();
    void refreshTree();
    void refreshSceneBoard();
    void selectItem();
    void applyScene();
    void addChapter();
    void addScene();
    void removeItem();
    void moveItem(int delta);
    void renameChapter();
    void importManuscript();
    void updateFormattingState();
    void applyCharacterFormat(int property, bool enabled);
    void updateWordCount();
    void loadSceneContent(const QJsonObject& scene);
    QJsonArray serializeFormatting() const;
    void applyFormatting(const QJsonArray& formatting);

    void updateReferenceInspector(const QJsonArray& references) {
        if (!references_) return;
        references_->clear();
        for (const QJsonValue value : references) {
            const QJsonObject reference = value.toObject();
            const QString label = reference.value(QStringLiteral("label")).toString();
            const QString kind = reference.value(QStringLiteral("kind")).toString();
            const QString id = reference.value(QStringLiteral("id")).toString();
            if (label.isEmpty() || id.isEmpty()) continue;

            QString typeLabel;
            if (kind == QStringLiteral("character")) typeLabel = tr("Personaje");
            else if (kind == QStringLiteral("world")) typeLabel = tr("Mundo");
            else if (kind == QStringLiteral("magic")) typeLabel = tr("Magia");
            else if (kind == QStringLiteral("worldText")) typeLabel = tr("Texto del mundo");
            else if (kind == QStringLiteral("magicText")) typeLabel = tr("Texto de magia");
            else typeLabel = kind;

            auto* item = new QListWidgetItem(typeLabel.isEmpty() ? label : QStringLiteral("%1  ·  %2").arg(label, typeLabel));
            item->setData(Qt::UserRole, kind);
            item->setData(Qt::UserRole + 1, id);
            item->setToolTip(tr("Doble clic para abrir la ficha"));
            references_->addItem(item);
        }
        if (references_->count() == 0) {
            auto* empty = new QListWidgetItem(tr("No hay referencias en esta escena."));
            empty->setFlags(Qt::NoItemFlags);
            references_->addItem(empty);
        }
    }

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    bool inspectorOrganized_ = false;
    QTabWidget* tabs_ = nullptr;
    QSplitter* editorSplit_ = nullptr;
    QWidget* indexPanel_ = nullptr;
    QWidget* metadataPanel_ = nullptr;
    QWidget* inspectorPanel_ = nullptr;
    QTabWidget* inspectorTabs_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLineEdit* sceneTitle_ = nullptr;
    QLineEdit* scenePov_ = nullptr;
    QLineEdit* sceneLocation_ = nullptr;
    QLineEdit* sceneLayer_ = nullptr;
    QComboBox* sceneStatus_ = nullptr;
    SemanticTextEdit* editor_ = nullptr;
    QLabel* wordCount_ = nullptr;
    QLabel* proofState_ = nullptr;
    QListWidget* references_ = nullptr;
    QToolButton* bold_ = nullptr;
    QToolButton* italic_ = nullptr;
    QToolButton* underline_ = nullptr;
    QGraphicsView* sceneBoard_ = nullptr;
    QCheckBox* sceneBoardCompact_ = nullptr;
};

} // namespace wbw
