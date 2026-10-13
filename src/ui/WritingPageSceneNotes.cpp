#include "ui/WritingPage.h"

#include "core/ArchiveDocument.h"

#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QTreeWidget>

namespace wbw {

void WritingPage::activateSceneNotes() {
    if (property("wbwSceneNotesV1").toBool() || !metadataPanel_) return;
    auto* grid = qobject_cast<QGridLayout*>(metadataPanel_->layout());
    if (!grid) return;
    setProperty("wbwSceneNotesV1", true);

    auto* title = new QLabel(tr("Notas rápidas"), metadataPanel_);
    title->setObjectName(QStringLiteral("sceneInspectorSection"));
    sceneNotes_ = new QTextEdit(metadataPanel_);
    sceneNotes_->setObjectName(QStringLiteral("sceneNotes"));
    sceneNotes_->setPlaceholderText(tr("Apuntes de esta escena…"));
    sceneNotes_->setAcceptRichText(false);
    sceneNotes_->setMinimumHeight(96);
    sceneNotes_->setMaximumHeight(170);
    grid->addWidget(title, 14, 0, 1, 2);
    grid->addWidget(sceneNotes_, 15, 0, 1, 2);
    grid->setRowStretch(16, 1);

    const auto loadCurrent = [this]() {
        if (!sceneNotes_) return;
        QString note;
        if (document_ && tree_) {
            if (QTreeWidgetItem* item = tree_->currentItem()) {
                if (item->data(0, Qt::UserRole).toString() == QStringLiteral("scene")) {
                    const int chapterIndex = item->data(0, Qt::UserRole + 1).toInt();
                    const int sceneIndex = item->data(0, Qt::UserRole + 2).toInt();
                    const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
                    if (chapterIndex >= 0 && chapterIndex < chapters.size()) {
                        const QJsonArray scenes = chapters.at(chapterIndex).toObject().value(QStringLiteral("scenes")).toArray();
                        if (sceneIndex >= 0 && sceneIndex < scenes.size())
                            note = scenes.at(sceneIndex).toObject().value(QStringLiteral("notes")).toString();
                    }
                }
            }
        }
        const QSignalBlocker blocker(sceneNotes_);
        sceneNotes_->setPlainText(note);
    };

    connect(tree_, &QTreeWidget::currentItemChanged, this,
            [loadCurrent](QTreeWidgetItem*, QTreeWidgetItem*) { loadCurrent(); });

    connect(sceneNotes_, &QTextEdit::textChanged, this, [this]() {
        if (!document_ || !tree_ || !sceneNotes_) return;
        QTreeWidgetItem* item = tree_->currentItem();
        if (!item || item->data(0, Qt::UserRole).toString() != QStringLiteral("scene")) return;
        const int chapterIndex = item->data(0, Qt::UserRole + 1).toInt();
        const int sceneIndex = item->data(0, Qt::UserRole + 2).toInt();
        QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
        if (chapterIndex < 0 || chapterIndex >= chapters.size()) return;
        QJsonObject chapter = chapters.at(chapterIndex).toObject();
        QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        if (sceneIndex < 0 || sceneIndex >= scenes.size()) return;
        QJsonObject scene = scenes.at(sceneIndex).toObject();
        const QString note = sceneNotes_->toPlainText();
        if (scene.value(QStringLiteral("notes")).toString() == note) return;
        scene.insert(QStringLiteral("notes"), note);
        scenes.replace(sceneIndex, scene);
        chapter.insert(QStringLiteral("scenes"), scenes);
        chapters.replace(chapterIndex, chapter);
        document_->setArray(QStringLiteral("writingChapters"), chapters);
        emit changed();
    });

    loadCurrent();
}

} // namespace wbw
