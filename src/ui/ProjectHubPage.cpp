#include "ui/ProjectHubPage.h"

#include "core/ArchiveDocument.h"
#include "storage/ProjectStore.h"

#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

namespace wbw {
namespace {

QPushButton* button(const QString& text) {
    auto* result = new QPushButton(text);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}

int manuscriptWords(const ArchiveDocument& document) {
    int words = 0;
    for (const QJsonValue chapterValue : document.array(QStringLiteral("writingChapters"))) {
        for (const QJsonValue sceneValue : chapterValue.toObject().value(QStringLiteral("scenes")).toArray()) {
            QString content = sceneValue.toObject().value(QStringLiteral("content")).toString();
            content.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
            words += content.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
        }
    }
    return words;
}

} // namespace

ProjectHubPage::ProjectHubPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("projectHubPage"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(34, 28, 34, 34);
    root->setSpacing(18);

    auto* top = new QHBoxLayout;
    auto* identity = new QVBoxLayout;
    identity->setSpacing(2);
    auto* kicker = new QLabel(tr("ARCHIVO"));
    kicker->setObjectName(QStringLiteral("hubKicker"));
    auto* title = new QLabel(tr("Tus historias"));
    title->setObjectName(QStringLiteral("hubTitle"));
    auto* description = new QLabel(tr("Biblioteca local de proyectos. Abre una obra existente, importa un .wbw o comienza una nueva."));
    description->setObjectName(QStringLiteral("hubDescription"));
    description->setWordWrap(true);
    identity->addWidget(kicker);
    identity->addWidget(title);
    identity->addWidget(description);
    top->addLayout(identity, 1);
    auto* import = button(tr("Abrir proyecto .wbw"));
    auto* create = button(tr("+ Nueva obra"));
    create->setObjectName(QStringLiteral("hubPrimary"));
    top->addWidget(import, 0, Qt::AlignBottom);
    top->addWidget(create, 0, Qt::AlignBottom);
    root->addLayout(top);

    auto* storage = new QWidget;
    storage->setObjectName(QStringLiteral("hubStorage"));
    auto* storageLayout = new QHBoxLayout(storage);
    storageLayout->setContentsMargins(12, 10, 12, 10);
    auto* dot = new QLabel(QStringLiteral("●"));
    dot->setObjectName(QStringLiteral("hubStorageDot"));
    auto* storageCopy = new QVBoxLayout;
    storageCopy->setSpacing(0);
    auto* storageTitle = new QLabel(tr("Guardado local disponible"));
    storageTitle->setObjectName(QStringLiteral("hubStorageTitle"));
    auto* storageDetail = new QLabel(tr("Los proyectos administrados se guardan en Documentos/Worldbuilder Writer/Projects."));
    storageDetail->setObjectName(QStringLiteral("hubStorageDetail"));
    storageCopy->addWidget(storageTitle);
    storageCopy->addWidget(storageDetail);
    storageLayout->addWidget(dot);
    storageLayout->addLayout(storageCopy, 1);
    root->addWidget(storage);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    cardsHost_ = new QWidget;
    cardsHost_->setObjectName(QStringLiteral("hubCardsHost"));
    cards_ = new QGridLayout(cardsHost_);
    cards_->setContentsMargins(0, 0, 0, 0);
    cards_->setHorizontalSpacing(14);
    cards_->setVerticalSpacing(14);
    scroll->setWidget(cardsHost_);
    root->addWidget(scroll, 1);

    emptyState_ = new QLabel(tr("Todavía no hay obras en la biblioteca."));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setObjectName(QStringLiteral("hubEmpty"));

    connect(create, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    connect(import, &QPushButton::clicked, this, &ProjectHubPage::importProjectRequested);
    refresh();
}

void ProjectHubPage::refresh() {
    rebuildCards();
}

void ProjectHubPage::rebuildCards() {
    while (QLayoutItem* item = cards_->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    const QStringList files = ProjectStore::projectFiles();
    if (files.isEmpty()) {
        cards_->addWidget(emptyState_, 0, 0, 1, 3);
        emptyState_->show();
        return;
    }
    emptyState_->hide();

    int row = 0;
    int column = 0;
    for (const QString& path : files) {
        ArchiveDocument document;
        QString error;
        const bool loaded = ProjectStore::loadJsonFile(path, document, &error);
        const QJsonObject profile = loaded ? document.object(QStringLiteral("profile")) : QJsonObject();
        const QString title = loaded
            ? (document.storyTitle().isEmpty() ? document.title() : document.storyTitle())
            : QFileInfo(path).dir().dirName();
        const QString genre = profile.value(QStringLiteral("genre")).toString(tr("Proyecto narrativo"));
        const QString status = profile.value(QStringLiteral("status")).toString(tr("Planificación"));
        const int characters = loaded ? document.array(QStringLiteral("characters")).size() : 0;
        const int relationships = loaded ? document.array(QStringLiteral("relationships")).size() : 0;
        const int chapters = loaded ? document.array(QStringLiteral("writingChapters")).size() : 0;
        const int words = loaded ? manuscriptWords(document) : 0;

        auto* card = new QWidget;
        card->setObjectName(QStringLiteral("projectCard"));
        card->setMinimumWidth(290);
        card->setMaximumWidth(420);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 15, 16, 14);
        cardLayout->setSpacing(10);

        auto* cardTop = new QHBoxLayout;
        auto* mark = new QLabel(QStringLiteral("WW"));
        mark->setObjectName(QStringLiteral("projectCardMark"));
        mark->setAlignment(Qt::AlignCenter);
        mark->setFixedSize(34, 34);
        auto* state = new QLabel(status);
        state->setObjectName(QStringLiteral("projectCardState"));
        cardTop->addWidget(mark);
        cardTop->addStretch();
        cardTop->addWidget(state);
        cardLayout->addLayout(cardTop);

        auto* genreLabel = new QLabel(genre.toUpper());
        genreLabel->setObjectName(QStringLiteral("projectCardGenre"));
        auto* titleLabel = new QLabel(title.isEmpty() ? tr("Historia sin título") : title);
        titleLabel->setObjectName(QStringLiteral("projectCardTitle"));
        titleLabel->setWordWrap(true);
        auto* archiveLabel = new QLabel(loaded ? document.title() : QFileInfo(path).fileName());
        archiveLabel->setObjectName(QStringLiteral("projectCardArchive"));
        cardLayout->addWidget(genreLabel);
        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(archiveLabel);

        auto* stats = new QGridLayout;
        stats->setHorizontalSpacing(14);
        const QStringList labels{tr("Fichas"), tr("Enlaces"), tr("Capítulos"), tr("Palabras")};
        const QList<int> values{characters, relationships, chapters, words};
        for (int i = 0; i < labels.size(); ++i) {
            auto* value = new QLabel(QString::number(values.at(i)));
            value->setObjectName(QStringLiteral("projectStatValue"));
            auto* label = new QLabel(labels.at(i));
            label->setObjectName(QStringLiteral("projectStatLabel"));
            stats->addWidget(value, 0, i);
            stats->addWidget(label, 1, i);
        }
        cardLayout->addLayout(stats);

        auto* actions = new QHBoxLayout;
        auto* remove = button(tr("Eliminar"));
        remove->setObjectName(QStringLiteral("projectDelete"));
        auto* open = button(tr("Abrir →"));
        open->setObjectName(QStringLiteral("hubPrimary"));
        actions->addWidget(remove);
        actions->addStretch();
        actions->addWidget(open);
        cardLayout->addLayout(actions);

        connect(open, &QPushButton::clicked, this, [this, path]() { emit openProjectRequested(path); });
        connect(remove, &QPushButton::clicked, this, [this, path, title]() {
            if (QMessageBox::question(this, tr("Eliminar proyecto"), tr("¿Eliminar definitivamente “%1”? Esta acción no se puede deshacer.").arg(title)) != QMessageBox::Yes) return;
            QString error;
            if (!ProjectStore::removeProject(path, &error)) {
                QMessageBox::critical(this, tr("No se pudo eliminar"), error);
                return;
            }
            refresh();
            emit projectDeleted();
        });

        cards_->addWidget(card, row, column);
        if (++column >= 3) {
            column = 0;
            ++row;
        }
    }
    cards_->setRowStretch(row + 1, 1);
}

} // namespace wbw
