#include "ui/WorldPage.h"

#include "core/ArchiveDocument.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace wbw {
namespace {

QLabel* editorialLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("knowledgeFieldLabel"));
    return label;
}

QWidget* editorialField(const QString& title, QWidget* field, QWidget* parent) {
    auto* host = new QWidget(parent);
    host->setObjectName(QStringLiteral("knowledgeField"));
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addWidget(editorialLabel(title, host));
    field->setParent(host);
    layout->addWidget(field);
    return host;
}

QFrame* editorialCard(const QString& title, const QString& copy, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("knowledgeEditorialCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(15, 13, 15, 15);
    layout->setSpacing(9);
    auto* heading = new QLabel(title, card);
    heading->setObjectName(QStringLiteral("knowledgeEditorialCardTitle"));
    layout->addWidget(heading);
    if (!copy.isEmpty()) {
        auto* description = new QLabel(copy, card);
        description->setObjectName(QStringLiteral("knowledgeEditorialCardCopy"));
        description->setWordWrap(true);
        layout->addWidget(description);
    }
    return card;
}

QString plainSceneText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument document;
    document.setHtml(scene.value(QStringLiteral("content")).toString());
    return document.toPlainText();
}

QPushButton* buttonContaining(QWidget* root, const QString& needle) {
    if (!root) return nullptr;
    for (QPushButton* button : root->findChildren<QPushButton*>()) {
        if (button && button->text().contains(needle, Qt::CaseInsensitive)) return button;
    }
    return nullptr;
}

} // namespace

void WorldPage::configureEditorialKnowledgeSurface() {
    if (property("wbwEditorialKnowledgeV1").toBool() || !tabs_) return;
    setProperty("wbwEditorialKnowledgeV1", true);

    // ----- Sistemas de magia -------------------------------------------------
    if (auto* magicTab = findChild<QWidget*>(QStringLiteral("magicTab"))) {
        auto* split = magicTab->findChild<QSplitter*>();
        auto* index = findChild<QWidget*>(QStringLiteral("magicIndexPanel"));
        auto* indexLayout = index ? qobject_cast<QVBoxLayout*>(index->layout()) : nullptr;
        auto* centerScroll = split && split->count() > 1 ? qobject_cast<QScrollArea*>(split->widget(1)) : nullptr;
        QWidget* oldEditor = centerScroll ? centerScroll->takeWidget() : nullptr;

        if (split && indexLayout && centerScroll && oldEditor && magicList_) {
            magicTab->setProperty("editorialSurface", true);
            centerScroll->setObjectName(QStringLiteral("magicEditorialScroll"));
            centerScroll->setFrameShape(QFrame::NoFrame);

            auto* search = new QLineEdit(index);
            search->setObjectName(QStringLiteral("magicSearch"));
            search->setPlaceholderText(tr("Buscar sistemas…"));
            search->setClearButtonEnabled(true);
            const int listPosition = qMax(0, indexLayout->indexOf(magicList_));
            indexLayout->insertWidget(listPosition, search);

            connect(search, &QLineEdit::textChanged, this, [this, search](const QString&) {
                if (!magicList_) return;
                const QString query = search->text().trimmed();
                const QJsonArray systems = document_ ? document_->array(QStringLiteral("magicSystems")) : QJsonArray();
                for (int row = 0; row < magicList_->count(); ++row) {
                    const QJsonObject system = row < systems.size() ? systems.at(row).toObject() : QJsonObject();
                    const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
                        .arg(system.value(QStringLiteral("name")).toString(),
                             system.value(QStringLiteral("category")).toString(),
                             system.value(QStringLiteral("status")).toString(),
                             system.value(QStringLiteral("source")).toString(),
                             system.value(QStringLiteral("principle")).toString());
                    magicList_->item(row)->setHidden(!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive));
                }
            });

            QPushButton* attach = buttonContaining(oldEditor, tr("Añadir adjunto"));
            QPushButton* detach = buttonContaining(oldEditor, tr("Quitar"));
            QPushButton* exportAttachment = buttonContaining(oldEditor, tr("Exportar"));

            auto* center = new QWidget;
            center->setObjectName(QStringLiteral("magicEditorialCenter"));
            auto* centerLayout = new QVBoxLayout(center);
            centerLayout->setContentsMargins(22, 20, 22, 26);
            centerLayout->setSpacing(12);

            auto* kicker = new QLabel(tr("SISTEMA DE MAGIA"), center);
            kicker->setObjectName(QStringLiteral("knowledgeEditorialKicker"));
            auto* nameRow = new QHBoxLayout;
            nameRow->setSpacing(9);
            magicName_->setParent(center);
            magicName_->setObjectName(QStringLiteral("magicEditorialName"));
            magicName_->setPlaceholderText(tr("Nombre del sistema"));
            nameRow->addWidget(magicName_, 1);
            magicStatus_->setParent(center);
            magicStatus_->setObjectName(QStringLiteral("magicEditorialStatus"));
            nameRow->addWidget(magicStatus_);
            centerLayout->addWidget(kicker);
            centerLayout->addLayout(nameRow);
            magicCategory_->setParent(center);
            centerLayout->addWidget(editorialField(tr("Categoría"), magicCategory_, center));

            auto* gridHost = new QWidget(center);
            gridHost->setObjectName(QStringLiteral("magicEditorialGrid"));
            auto* grid = new QGridLayout(gridHost);
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(12);
            grid->setVerticalSpacing(12);
            grid->setColumnStretch(0, 1);
            grid->setColumnStretch(1, 1);

            auto addPairCard = [this, gridHost, grid](int row, int column,
                                                     const QString& title, const QString& copy,
                                                     const QList<QPair<QString, QWidget*>>& fields) {
                auto* card = editorialCard(title, copy, gridHost);
                auto* layout = qobject_cast<QVBoxLayout*>(card->layout());
                for (const auto& pair : fields) {
                    if (auto* edit = qobject_cast<QTextEdit*>(pair.second)) {
                        edit->setMinimumHeight(92);
                        edit->setMaximumHeight(160);
                    }
                    layout->addWidget(editorialField(pair.first, pair.second, card));
                }
                grid->addWidget(card, row, column);
            };

            addPairCard(0, 0, tr("Fundamento"), tr("Qué alimenta el sistema y qué principio lo hace coherente."),
                        {{tr("Fuente"), magicSource_}, {tr("Principio"), magicPrinciple_}});
            addPairCard(0, 1, tr("Reglas"), tr("Quién accede, qué paga y dónde deja de funcionar."),
                        {{tr("Acceso"), magicAccess_}, {tr("Coste"), magicCost_}, {tr("Límites"), magicLimits_}});
            addPairCard(1, 0, tr("Manifestación"), tr("Cómo aparece en el mundo material."),
                        {{tr("Manifestaciones"), magicManifestations_}, {tr("Materiales"), magicMaterials_}});
            addPairCard(1, 1, tr("Sociedad"), tr("Quién lo organiza, enseña, persigue o utiliza."),
                        {{tr("Instituciones"), magicInstitutions_}, {tr("Usuarios"), magicUsers_}});
            centerLayout->addWidget(gridHost);
            centerLayout->addStretch(1);
            centerScroll->setWidget(center);

            auto* contextScroll = new QScrollArea(split);
            contextScroll->setObjectName(QStringLiteral("magicContextScroll"));
            contextScroll->setFrameShape(QFrame::NoFrame);
            contextScroll->setWidgetResizable(true);
            contextScroll->setMinimumWidth(290);
            contextScroll->setMaximumWidth(360);
            auto* context = new QWidget;
            context->setObjectName(QStringLiteral("magicContextRail"));
            auto* contextLayout = new QVBoxLayout(context);
            contextLayout->setContentsMargins(13, 16, 13, 18);
            contextLayout->setSpacing(10);
            auto* contextTitle = new QLabel(tr("Contexto"), context);
            contextTitle->setObjectName(QStringLiteral("knowledgeContextTitle"));
            auto* contextCopy = new QLabel(tr("Riesgos, historia, notas y evidencia sin ocupar la superficie principal."), context);
            contextCopy->setObjectName(QStringLiteral("knowledgeContextCopy"));
            contextCopy->setWordWrap(true);
            contextLayout->addWidget(contextTitle);
            contextLayout->addWidget(contextCopy);

            auto addContextCard = [context, contextLayout](const QString& title, QWidget* field, int height) {
                auto* card = editorialCard(title, QString(), context);
                if (auto* edit = qobject_cast<QTextEdit*>(field)) {
                    edit->setMinimumHeight(height);
                    edit->setMaximumHeight(height + 60);
                }
                qobject_cast<QVBoxLayout*>(card->layout())->addWidget(field);
                contextLayout->addWidget(card);
            };
            addContextCard(tr("Riesgos"), magicRisks_, 90);
            addContextCard(tr("Historia"), magicHistory_, 110);
            addContextCard(tr("Notas"), magicNotes_, 100);
            addContextCard(tr("Evidencias"), magicEvidence_, 115);

            auto* meta = editorialCard(tr("Archivo"), tr("Etiquetas y adjuntos asociados al sistema."), context);
            auto* metaLayout = qobject_cast<QVBoxLayout*>(meta->layout());
            metaLayout->addWidget(editorialField(tr("Etiquetas"), magicTags_, meta));
            magicAttachments_->setParent(meta);
            magicAttachments_->setMinimumHeight(90);
            magicAttachments_->setMaximumHeight(160);
            metaLayout->addWidget(editorialLabel(tr("Adjuntos"), meta));
            metaLayout->addWidget(magicAttachments_);
            auto* attachmentActions = new QHBoxLayout;
            attachmentActions->setSpacing(5);
            if (attach) { attach->setParent(meta); attachmentActions->addWidget(attach); }
            if (detach) { detach->setParent(meta); attachmentActions->addWidget(detach); }
            if (exportAttachment) { exportAttachment->setParent(meta); attachmentActions->addWidget(exportAttachment); }
            metaLayout->addLayout(attachmentActions);
            contextLayout->addWidget(meta);
            contextLayout->addStretch(1);
            contextScroll->setWidget(context);
            split->addWidget(contextScroll);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes({235, 850, 320});

            oldEditor->deleteLater();
        }
    }

    // ----- Textos de referencia ---------------------------------------------
    QTabWidget* references = nullptr;
    for (int i = 0; i < tabs_->count(); ++i) {
        QWidget* candidate = tabs_->widget(i);
        if (candidate && worldTextList_ && candidate->isAncestorOf(worldTextList_)) {
            references = qobject_cast<QTabWidget*>(candidate);
            if (references) break;
        }
    }

    if (references) {
        references->setObjectName(QStringLiteral("textsTab"));
        references->setProperty("editorialSurface", true);
        references->setDocumentMode(true);
        if (references->count() > 0) references->setTabText(0, tr("Mundo y documentación"));
        if (references->count() > 1) references->setTabText(1, tr("Magia y fuentes"));

        auto rebuildReferencePage = [this](QListWidget* list,
                                           QLineEdit* title,
                                           QLineEdit* source,
                                           QTextEdit* content,
                                           bool magic) {
            if (!list || !title || !source || !content) return;
            QSplitter* split = qobject_cast<QSplitter*>(list->parentWidget());
            if (!split || split->count() < 2) return;
            QWidget* oldEditor = split->widget(1);
            if (!oldEditor) return;

            QPushButton* import = buttonContaining(oldEditor, tr("Importar"));
            QPushButton* add = buttonContaining(oldEditor, tr("+ Texto"));
            QPushButton* remove = buttonContaining(oldEditor, tr("Eliminar"));

            auto* left = new QFrame(split);
            left->setObjectName(QStringLiteral("referenceIndexPanel"));
            left->setMinimumWidth(235);
            left->setMaximumWidth(310);
            auto* leftLayout = new QVBoxLayout(left);
            leftLayout->setContentsMargins(13, 15, 13, 13);
            leftLayout->setSpacing(8);
            auto* leftTitle = new QLabel(magic ? tr("FUENTES DE MAGIA") : tr("DOCUMENTACIÓN"), left);
            leftTitle->setObjectName(QStringLiteral("knowledgeEditorialKicker"));
            auto* search = new QLineEdit(left);
            search->setObjectName(QStringLiteral("referenceSearch"));
            search->setPlaceholderText(tr("Buscar referencias…"));
            search->setClearButtonEnabled(true);
            list->setParent(left);
            list->setObjectName(QStringLiteral("referenceIndexList"));
            leftLayout->addWidget(leftTitle);
            leftLayout->addWidget(search);
            leftLayout->addWidget(list, 1);
            auto* listActions = new QHBoxLayout;
            listActions->setSpacing(5);
            if (add) { add->setParent(left); listActions->addWidget(add, 1); }
            if (remove) { remove->setParent(left); listActions->addWidget(remove); }
            leftLayout->addLayout(listActions);
            if (import) { import->setParent(left); import->setObjectName(QStringLiteral("referenceImportAction")); leftLayout->addWidget(import); }
            split->insertWidget(0, left);

            auto* center = new QWidget(split);
            center->setObjectName(QStringLiteral("referenceEditorSurface"));
            auto* centerLayout = new QVBoxLayout(center);
            centerLayout->setContentsMargins(24, 20, 24, 24);
            centerLayout->setSpacing(10);
            auto* kicker = new QLabel(magic ? tr("FUENTE / MAGIA") : tr("FUENTE / MUNDO"), center);
            kicker->setObjectName(QStringLiteral("knowledgeEditorialKicker"));
            centerLayout->addWidget(kicker);
            title->setParent(center);
            title->setObjectName(QStringLiteral("referenceEditorialTitle"));
            title->setPlaceholderText(tr("Título de la referencia"));
            centerLayout->addWidget(title);
            source->setParent(center);
            source->setObjectName(QStringLiteral("referenceEditorialSource"));
            source->setPlaceholderText(tr("Origen, autor, archivo o procedencia"));
            centerLayout->addWidget(editorialField(tr("Origen"), source, center));
            auto* bodyLabel = editorialLabel(tr("Contenido"), center);
            centerLayout->addWidget(bodyLabel);
            content->setParent(center);
            content->setObjectName(QStringLiteral("referenceEditorialContent"));
            content->setMinimumHeight(360);
            centerLayout->addWidget(content, 1);

            QWidget* displaced = split->replaceWidget(1, center);
            if (displaced && displaced != center) displaced->deleteLater();

            auto* context = new QFrame(split);
            context->setObjectName(QStringLiteral("referenceContextRail"));
            context->setMinimumWidth(250);
            context->setMaximumWidth(320);
            auto* contextLayout = new QVBoxLayout(context);
            contextLayout->setContentsMargins(13, 16, 13, 16);
            contextLayout->setSpacing(8);
            auto* contextTitle = new QLabel(tr("Uso en el manuscrito"), context);
            contextTitle->setObjectName(QStringLiteral("knowledgeContextTitle"));
            auto* contextCopy = new QLabel(tr("Escenas donde aparece literalmente el título de esta referencia."), context);
            contextCopy->setObjectName(QStringLiteral("knowledgeContextCopy"));
            contextCopy->setWordWrap(true);
            auto* mentions = new QListWidget(context);
            mentions->setObjectName(QStringLiteral("referenceMentions"));
            contextLayout->addWidget(contextTitle);
            contextLayout->addWidget(contextCopy);
            contextLayout->addWidget(mentions, 1);
            split->addWidget(context);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes({260, 850, 280});

            const QString key = magic ? QStringLiteral("magicTexts") : QStringLiteral("worldTexts");
            connect(search, &QLineEdit::textChanged, this, [this, search, list, key](const QString&) {
                const QString query = search->text().trimmed();
                const QJsonArray values = document_ ? document_->array(key) : QJsonArray();
                for (int row = 0; row < list->count(); ++row) {
                    const QJsonObject ref = row < values.size() ? values.at(row).toObject() : QJsonObject();
                    const QString haystack = ref.value(QStringLiteral("title")).toString()
                                            + QLatin1Char(' ') + ref.value(QStringLiteral("sourceName")).toString()
                                            + QLatin1Char(' ') + ref.value(QStringLiteral("content")).toString();
                    list->item(row)->setHidden(!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive));
                }
            });

            const auto refreshMentions = [this, title, mentions]() {
                mentions->clear();
                if (!document_) return;
                const QString needle = title->text().trimmed();
                if (needle.size() < 2) {
                    auto* empty = new QListWidgetItem(tr("Selecciona una referencia para ver sus menciones."), mentions);
                    empty->setFlags(Qt::NoItemFlags);
                    return;
                }
                const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
                for (int c = 0; c < chapters.size(); ++c) {
                    const QJsonObject chapter = chapters.at(c).toObject();
                    const QString chapterName = chapter.value(QStringLiteral("title")).toString(
                        chapter.value(QStringLiteral("label")).toString(tr("Capítulo %1").arg(c + 1)));
                    const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
                    for (int s = 0; s < scenes.size(); ++s) {
                        const QJsonObject scene = scenes.at(s).toObject();
                        if (!plainSceneText(scene).contains(needle, Qt::CaseInsensitive)) continue;
                        const QString sceneName = scene.value(QStringLiteral("title")).toString(tr("Escena %1").arg(s + 1));
                        mentions->addItem(QStringLiteral("%1\n%2").arg(chapterName, sceneName));
                    }
                }
                if (!mentions->count()) {
                    auto* empty = new QListWidgetItem(tr("Sin menciones literales en el manuscrito."), mentions);
                    empty->setFlags(Qt::NoItemFlags);
                }
            };
            connect(list, &QListWidget::currentRowChanged, this, [refreshMentions](int) {
                QTimer::singleShot(0, [refreshMentions]() { refreshMentions(); });
            });
            connect(title, &QLineEdit::textChanged, this, [refreshMentions](const QString&) { refreshMentions(); });
            refreshMentions();
        };

        rebuildReferencePage(worldTextList_, worldTextTitle_, worldTextSource_, worldTextContent_, false);
        rebuildReferencePage(magicTextList_, magicTextTitle_, magicTextSource_, magicTextContent_, true);
    }
}

} // namespace wbw
