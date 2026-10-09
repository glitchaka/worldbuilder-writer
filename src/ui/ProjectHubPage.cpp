#include "ui/ProjectHubPage.h"

#include "core/ArchiveDocument.h"
#include "storage/ProjectStore.h"
#include "ui/SettingsDialog.h"

#include <QDateTime>
#include <QDir>
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

QPushButton* button(const QString& text, const QString& objectName = {}) {
    auto* result = new QPushButton(text);
    result->setCursor(Qt::PointingHandCursor);
    if (!objectName.isEmpty()) result->setObjectName(objectName);
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

QString formatDate(const QString& path) {
    const QDateTime modified = QFileInfo(path).lastModified();
    return modified.isValid() ? modified.date().toString(QStringLiteral("dd MMM yyyy")) : QObject::tr("fecha desconocida");
}

} // namespace

ProjectHubPage::ProjectHubPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("projectHubPage"));
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(0);

    auto* desktopBar = new QWidget;
    desktopBar->setObjectName(QStringLiteral("hubDesktopBar"));
    auto* desktopLayout = new QHBoxLayout(desktopBar);
    desktopLayout->setContentsMargins(22, 12, 22, 12);
    desktopLayout->setSpacing(12);

    auto* mark = new QLabel(QStringLiteral("WW"));
    mark->setObjectName(QStringLiteral("hubAppMark"));
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(38, 38);
    desktopLayout->addWidget(mark);

    auto* identity = new QVBoxLayout;
    identity->setSpacing(0);
    auto* appName = new QLabel(QStringLiteral("Worldbuilder Writer"));
    appName->setObjectName(QStringLiteral("hubAppName"));
    auto* appMode = new QLabel(tr("Biblioteca local de proyectos"));
    appMode->setObjectName(QStringLiteral("hubAppMode"));
    identity->addWidget(appName);
    identity->addWidget(appMode);
    desktopLayout->addLayout(identity);
    desktopLayout->addStretch(1);

    auto* import = button(tr("Abrir proyecto .wbw"), QStringLiteral("hubSecondary"));
    auto* create = button(tr("+ Nueva obra"), QStringLiteral("hubPrimary"));
    auto* settings = button(tr("⚙  Configuración"), QStringLiteral("hubSecondary"));
    desktopLayout->addWidget(import);
    desktopLayout->addWidget(create);
    desktopLayout->addWidget(settings);
    page->addWidget(desktopBar);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* shell = new QWidget;
    shell->setObjectName(QStringLiteral("hubShell"));
    auto* root = new QVBoxLayout(shell);
    root->setContentsMargins(34, 28, 34, 34);
    root->setSpacing(18);

    auto* intro = new QHBoxLayout;
    auto* copy = new QVBoxLayout;
    copy->setSpacing(3);
    auto* kicker = new QLabel(tr("ARCHIVO"));
    kicker->setObjectName(QStringLiteral("hubKicker"));
    auto* title = new QLabel(tr("Tus historias"));
    title->setObjectName(QStringLiteral("hubTitle"));
    auto* description = new QLabel(tr("Los proyectos guardados localmente aparecen como tarjetas independientes."));
    description->setObjectName(QStringLiteral("hubDescription"));
    copy->addWidget(kicker);
    copy->addWidget(title);
    copy->addWidget(description);
    intro->addLayout(copy, 1);

    auto* storage = new QWidget;
    storage->setObjectName(QStringLiteral("hubStorage"));
    storage->setMaximumWidth(360);
    auto* storageLayout = new QHBoxLayout(storage);
    storageLayout->setContentsMargins(12, 10, 12, 10);
    auto* dot = new QLabel(QStringLiteral("●"));
    dot->setObjectName(QStringLiteral("hubStorageDot"));
    auto* storageCopy = new QVBoxLayout;
    storageCopy->setSpacing(0);
    auto* storageTitle = new QLabel(tr("Guardado local disponible"));
    storageTitle->setObjectName(QStringLiteral("hubStorageTitle"));
    auto* storageDetail = new QLabel(tr("No necesitas iniciar sesión para escribir."));
    storageDetail->setObjectName(QStringLiteral("hubStorageDetail"));
    storageCopy->addWidget(storageTitle);
    storageCopy->addWidget(storageDetail);
    storageLayout->addWidget(dot);
    storageLayout->addLayout(storageCopy, 1);
    intro->addWidget(storage, 0, Qt::AlignBottom);
    root->addLayout(intro);

    cardsHost_ = new QWidget;
    cardsHost_->setObjectName(QStringLiteral("hubCardsHost"));
    cards_ = new QGridLayout(cardsHost_);
    cards_->setContentsMargins(0, 0, 0, 0);
    cards_->setHorizontalSpacing(16);
    cards_->setVerticalSpacing(16);
    root->addWidget(cardsHost_, 1);

    emptyState_ = new QLabel;
    emptyState_->hide();

    scroll->setWidget(shell);
    page->addWidget(scroll, 1);

    connect(create, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    connect(import, &QPushButton::clicked, this, &ProjectHubPage::importProjectRequested);
    connect(settings, &QPushButton::clicked, this, [this]() {
        SettingsDialog dialog(this);
        dialog.exec();
        emit settingsRequested();
    });
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

    int row = 0;
    int column = 0;

    auto* newCard = new QPushButton;
    newCard->setObjectName(QStringLiteral("projectCardNew"));
    newCard->setCursor(Qt::PointingHandCursor);
    newCard->setMinimumSize(300, 250);
    newCard->setMaximumWidth(390);
    auto* newLayout = new QVBoxLayout(newCard);
    newLayout->setContentsMargins(22, 22, 22, 22);
    newLayout->setSpacing(10);
    auto* plus = new QLabel(QStringLiteral("+"));
    plus->setObjectName(QStringLiteral("newProjectMark"));
    plus->setAlignment(Qt::AlignCenter);
    plus->setFixedSize(46, 46);
    auto* newTitle = new QLabel(tr("Nueva obra"));
    newTitle->setObjectName(QStringLiteral("newProjectTitle"));
    auto* newCopy = new QLabel(tr("Comienza con un archivo completamente vacío."));
    newCopy->setObjectName(QStringLiteral("newProjectCopy"));
    newCopy->setWordWrap(true);
    auto* newMeta = new QLabel(tr("CREAR DESDE CERO"));
    newMeta->setObjectName(QStringLiteral("newProjectMeta"));
    newLayout->addWidget(plus, 0, Qt::AlignLeft);
    newLayout->addStretch(1);
    newLayout->addWidget(newTitle);
    newLayout->addWidget(newCopy);
    newLayout->addStretch(1);
    newLayout->addWidget(newMeta);
    connect(newCard, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    cards_->addWidget(newCard, row, column++);

    for (const QString& path : ProjectStore::projectFiles()) {
        ArchiveDocument document;
        QString error;
        const bool loaded = ProjectStore::loadJsonFile(path, document, &error);
        const QJsonObject profile = loaded ? document.object(QStringLiteral("profile")) : QJsonObject();
        const QString archiveTitle = loaded ? document.title() : QFileInfo(path).dir().dirName();
        const QString storyTitle = loaded && !document.storyTitle().isEmpty() ? document.storyTitle() : tr("Historia aún sin título");
        const QString genre = profile.value(QStringLiteral("genre")).toString(tr("Proyecto narrativo"));
        const QString status = profile.value(QStringLiteral("status")).toString(tr("Planificación"));
        const int characters = loaded ? document.array(QStringLiteral("characters")).size() : 0;
        const int relationships = loaded ? document.array(QStringLiteral("relationships")).size() : 0;
        const int chapters = loaded ? document.array(QStringLiteral("writingChapters")).size() : 0;
        const int words = loaded ? manuscriptWords(document) : 0;

        if (column >= 3) { column = 0; ++row; }

        auto* card = new QWidget;
        card->setObjectName(QStringLiteral("projectCard"));
        card->setMinimumSize(300, 250);
        card->setMaximumWidth(390);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 14);
        cardLayout->setSpacing(11);

        auto* cardTop = new QHBoxLayout;
        auto* cardMark = new QLabel(QStringLiteral("WW"));
        cardMark->setObjectName(QStringLiteral("projectCardMark"));
        cardMark->setAlignment(Qt::AlignCenter);
        cardMark->setFixedSize(34, 34);
        auto* state = new QLabel(status);
        state->setObjectName(QStringLiteral("projectCardState"));
        cardTop->addWidget(cardMark);
        cardTop->addStretch();
        cardTop->addWidget(state);
        cardLayout->addLayout(cardTop);

        auto* genreLabel = new QLabel(genre.toUpper());
        genreLabel->setObjectName(QStringLiteral("projectCardGenre"));
        auto* titleLabel = new QLabel(archiveTitle.isEmpty() ? tr("Proyecto sin nombre") : archiveTitle);
        titleLabel->setObjectName(QStringLiteral("projectCardTitle"));
        titleLabel->setWordWrap(true);
        auto* storyLabel = new QLabel(storyTitle);
        storyLabel->setObjectName(QStringLiteral("projectCardArchive"));
        storyLabel->setWordWrap(true);
        cardLayout->addWidget(genreLabel);
        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(storyLabel);

        auto* stats = new QGridLayout;
        stats->setHorizontalSpacing(16);
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
        cardLayout->addStretch(1);

        auto* footer = new QHBoxLayout;
        auto* saved = new QLabel(tr("Guardado %1").arg(formatDate(path)));
        saved->setObjectName(QStringLiteral("projectSaved"));
        auto* remove = button(tr("Eliminar"), QStringLiteral("projectDelete"));
        auto* open = button(tr("Abrir  →"), QStringLiteral("hubPrimary"));
        footer->addWidget(saved);
        footer->addStretch();
        footer->addWidget(remove);
        footer->addWidget(open);
        cardLayout->addLayout(footer);

        connect(open, &QPushButton::clicked, this, [this, path]() { emit openProjectRequested(path); });
        connect(remove, &QPushButton::clicked, this, [this, path, archiveTitle]() {
            if (QMessageBox::question(this, tr("Eliminar proyecto"), tr("¿Eliminar definitivamente “%1”? Esta acción no se puede deshacer.").arg(archiveTitle)) != QMessageBox::Yes) return;
            QString error;
            if (!ProjectStore::removeProject(path, &error)) {
                QMessageBox::critical(this, tr("No se pudo eliminar"), error);
                return;
            }
            refresh();
            emit projectDeleted();
        });

        cards_->addWidget(card, row, column++);
    }

    cards_->setColumnStretch(0, 1);
    cards_->setColumnStretch(1, 1);
    cards_->setColumnStretch(2, 1);
    cards_->setRowStretch(row + 1, 1);
}

} // namespace wbw
