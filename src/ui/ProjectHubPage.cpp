#include "ui/ProjectHubPage.h"

#include "core/ArchiveDocument.h"
#include "storage/ProjectStore.h"
#include "ui/SettingsDialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

#include <functional>

namespace wbw {
namespace {

class ClickableCard final : public QFrame {
public:
    explicit ClickableCard(QWidget* parent = nullptr) : QFrame(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
    }

    std::function<void()> activated;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            if (activated) activated();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
            if (activated) activated();
            event->accept();
            return;
        }
        QFrame::keyPressEvent(event);
    }
};

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
            const QJsonObject scene = sceneValue.toObject();
            QString content;
            if (scene.contains(QStringLiteral("text"))) content = scene.value(QStringLiteral("text")).toString();
            else {
                content = scene.value(QStringLiteral("content")).toString();
                content.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
            }
            words += content.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
        }
    }
    return words;
}

QString formatDate(const QString& path) {
    const QDateTime modified = QFileInfo(path).lastModified();
    return modified.isValid() ? modified.date().toString(QStringLiteral("dd MMM yyyy")) : QObject::tr("fecha desconocida");
}

QByteArray dataUrlBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive) ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload);
}

void setCover(QLabel* label, const QJsonObject& profile, const QString& fallback) {
    QPixmap pixmap;
    const QByteArray bytes = dataUrlBytes(profile.value(QStringLiteral("coverImageDataUrl")).toString());
    if (!bytes.isEmpty()) pixmap.loadFromData(bytes);
    if (!pixmap.isNull()) {
        label->setText(QString());
        label->setPixmap(pixmap.scaled(label->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        label->setText(fallback.toUpper());
    }
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
    root->setContentsMargins(34, 26, 34, 34);
    root->setSpacing(16);

    auto* intro = new QHBoxLayout;
    auto* copy = new QVBoxLayout;
    copy->setSpacing(3);
    auto* kicker = new QLabel(tr("BIBLIOTECA"));
    kicker->setObjectName(QStringLiteral("hubKicker"));
    auto* title = new QLabel(tr("Tus historias"));
    title->setObjectName(QStringLiteral("hubTitle"));
    auto* description = new QLabel(tr("Crea, organiza y abre todos tus proyectos desde una biblioteca visual."));
    description->setObjectName(QStringLiteral("hubDescription"));
    description->setWordWrap(true);
    copy->addWidget(kicker);
    copy->addWidget(title);
    copy->addWidget(description);
    intro->addLayout(copy, 1);

    auto* storage = new QWidget;
    storage->setObjectName(QStringLiteral("hubStorage"));
    storage->setMaximumWidth(340);
    auto* storageLayout = new QHBoxLayout(storage);
    storageLayout->setContentsMargins(12, 10, 12, 10);
    auto* dot = new QLabel(QStringLiteral("●"));
    dot->setObjectName(QStringLiteral("hubStorageDot"));
    auto* storageCopy = new QVBoxLayout;
    storageCopy->setSpacing(0);
    auto* storageTitle = new QLabel(tr("Guardado local disponible"));
    storageTitle->setObjectName(QStringLiteral("hubStorageTitle"));
    auto* storageDetail = new QLabel(tr("Tus proyectos siguen siendo archivos locales."));
    storageDetail->setObjectName(QStringLiteral("hubStorageDetail"));
    storageCopy->addWidget(storageTitle);
    storageCopy->addWidget(storageDetail);
    storageLayout->addWidget(dot);
    storageLayout->addLayout(storageCopy, 1);
    intro->addWidget(storage, 0, Qt::AlignBottom);
    root->addLayout(intro);

    auto* filters = new QHBoxLayout;
    filters->setSpacing(7);
    search_ = new QLineEdit;
    search_->setObjectName(QStringLiteral("hubSearch"));
    search_->setPlaceholderText(tr("Buscar proyectos…"));
    search_->setClearButtonEnabled(true);
    search_->setMaximumWidth(320);
    statusFilter_ = new QComboBox;
    statusFilter_->setObjectName(QStringLiteral("hubStatusFilter"));
    statusFilter_->addItems({tr("Todos los estados"), tr("Planificación"), tr("Borrador"), tr("En desarrollo"), tr("Revisión"), tr("Final")});
    statusFilter_->setMaximumWidth(170);
    auto* compactImport = button(tr("Abrir .wbw"), QStringLiteral("hubSecondary"));
    auto* compactCreate = button(tr("+ Nueva obra"), QStringLiteral("hubPrimary"));
    filters->addWidget(search_, 1);
    filters->addWidget(statusFilter_);
    filters->addStretch();
    filters->addWidget(compactImport);
    filters->addWidget(compactCreate);
    root->addLayout(filters);

    cardsHost_ = new QWidget;
    cardsHost_->setObjectName(QStringLiteral("hubCardsHost"));
    cards_ = new QGridLayout(cardsHost_);
    cards_->setContentsMargins(0, 0, 0, 0);
    cards_->setHorizontalSpacing(14);
    cards_->setVerticalSpacing(14);
    root->addWidget(cardsHost_, 1);

    emptyState_ = new QLabel(tr("No hay proyectos que coincidan con los filtros."));
    emptyState_->setObjectName(QStringLiteral("hubEmptyState"));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->hide();
    root->addWidget(emptyState_);

    scroll->setWidget(shell);
    page->addWidget(scroll, 1);

    setStyleSheet(QStringLiteral(
        "#projectHubPage,#hubShell,#hubCardsHost{background:#eef2f6;}"
        "#hubDesktopBar{background:#ffffff;border-bottom:1px solid #d8dee7;}"
        "#hubAppMark,#projectCardMark{background:#17233a;color:#ffffff;border-radius:3px;font-family:'Georgia';font-weight:700;}"
        "#hubAppName{font-size:10.5pt;font-weight:700;color:#243247;}"
        "#hubAppMode{font-size:8pt;color:#8b95a5;}"
        "#hubKicker,#projectCardGenre,#newProjectMeta{color:#7c8797;font-size:8pt;font-weight:700;letter-spacing:.8px;}"
        "#hubTitle{font-family:'Georgia';font-size:28pt;font-weight:500;color:#344054;}"
        "#hubDescription{font-family:'Georgia';font-size:10pt;color:#667085;}"
        "#hubStorage{background:#f7faf8;border:1px solid #d7e6da;border-radius:3px;}"
        "#hubStorageDot{color:#2f855a;}#hubStorageTitle{font-weight:700;color:#344054;}#hubStorageDetail{color:#667085;font-size:8.5pt;}"
        "#projectCard{background:#ffffff;border:1px solid #d7dee8;border-radius:4px;}"
        "#projectCardCover{background:#1b2636;color:#9fb0c4;border:0;border-radius:4px;font-size:8pt;font-weight:700;letter-spacing:1px;}"
        "#projectCardState{background:#f2f4f7;color:#475467;border:1px solid #e1e5ea;border-radius:3px;padding:3px 7px;font-size:8pt;}"
        "#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:#344054;}"
        "#projectCardArchive,#newProjectCopy{color:#667085;font-size:9pt;}"
        "#projectStatValue{font-weight:700;color:#344054;font-size:10pt;}#projectStatLabel,#projectSaved{color:#98a2b3;font-size:8pt;}"
        "#hubEmptyState{color:#7f8b9b;padding:28px;}"
        "QFrame#projectCardNew{background:#f8fafc;border:1px dashed #b8c2cf;border-radius:4px;}"
        "QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{background:#f1f6ff;border-color:#8bb5ef;}"
        "#newProjectMark{background:#eaf2ff;color:#1668d4;border:1px solid #c8dbfb;border-radius:23px;font-size:20pt;font-weight:300;}"
    ));

    connect(create, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    connect(compactCreate, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    connect(import, &QPushButton::clicked, this, &ProjectHubPage::importProjectRequested);
    connect(compactImport, &QPushButton::clicked, this, &ProjectHubPage::importProjectRequested);
    connect(search_, &QLineEdit::textChanged, this, [this]() { rebuildCards(); });
    connect(statusFilter_, &QComboBox::currentTextChanged, this, [this]() { rebuildCards(); });
    connect(settings, &QPushButton::clicked, this, [this]() {
        SettingsDialog dialog(this);
        dialog.exec();
        emit settingsRequested();
    });
    refresh();
}

void ProjectHubPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

void ProjectHubPage::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
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
    int visibleProjects = 0;

    auto* newCard = new ClickableCard;
    newCard->setObjectName(QStringLiteral("projectCardNew"));
    newCard->setAccessibleName(tr("Nueva obra"));
    newCard->setMinimumSize(300, 322);
    newCard->setMaximumWidth(410);
    auto* newLayout = new QVBoxLayout(newCard);
    newLayout->setContentsMargins(22, 22, 22, 22);
    newLayout->setSpacing(10);
    auto* plus = new QLabel(QStringLiteral("+"));
    plus->setObjectName(QStringLiteral("newProjectMark"));
    plus->setAlignment(Qt::AlignCenter);
    plus->setFixedSize(46, 46);
    plus->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* newTitle = new QLabel(tr("Nueva obra"));
    newTitle->setObjectName(QStringLiteral("newProjectTitle"));
    newTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* newCopy = new QLabel(tr("Comienza un proyecto completamente nuevo desde cero."));
    newCopy->setObjectName(QStringLiteral("newProjectCopy"));
    newCopy->setWordWrap(true);
    newCopy->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* newMeta = new QLabel(tr("OBRA EN BLANCO"));
    newMeta->setObjectName(QStringLiteral("newProjectMeta"));
    newMeta->setAttribute(Qt::WA_TransparentForMouseEvents);
    newLayout->addWidget(plus, 0, Qt::AlignLeft);
    newLayout->addStretch(1);
    newLayout->addWidget(newTitle);
    newLayout->addWidget(newCopy);
    newLayout->addStretch(1);
    newLayout->addWidget(newMeta);
    newCard->activated = [this]() { emit newProjectRequested(); };
    cards_->addWidget(newCard, row, column++);

    const QString query = search_ ? search_->text().trimmed() : QString();
    const QString statusWanted = statusFilter_ && statusFilter_->currentIndex() > 0 ? statusFilter_->currentText() : QString();

    for (const QString& path : ProjectStore::projectFiles()) {
        ArchiveDocument document;
        QString error;
        const bool loaded = ProjectStore::loadJsonFile(path, document, &error);
        const QJsonObject profile = loaded ? document.object(QStringLiteral("profile")) : QJsonObject();
        const QString archiveTitle = loaded ? document.title() : QFileInfo(path).dir().dirName();
        const QString storyTitle = loaded && !document.storyTitle().isEmpty() ? document.storyTitle() : tr("Historia aún sin título");
        const QString genre = profile.value(QStringLiteral("genre")).toString(tr("Proyecto narrativo"));
        const QString status = profile.value(QStringLiteral("status")).toString(tr("Planificación"));
        if (!query.isEmpty() && !QStringLiteral("%1 %2 %3 %4").arg(archiveTitle, storyTitle, genre, status).contains(query, Qt::CaseInsensitive)) continue;
        if (!statusWanted.isEmpty() && status.compare(statusWanted, Qt::CaseInsensitive) != 0) continue;

        const int characters = loaded ? document.array(QStringLiteral("characters")).size() : 0;
        const int relationships = loaded ? document.array(QStringLiteral("relationships")).size() : 0;
        const int chapters = loaded ? document.array(QStringLiteral("writingChapters")).size() : 0;
        const int words = loaded ? manuscriptWords(document) : 0;

        if (column >= 3) { column = 0; ++row; }
        ++visibleProjects;

        auto* card = new ClickableCard;
        card->setObjectName(QStringLiteral("projectCard"));
        card->setMinimumSize(300, 322);
        card->setMaximumWidth(410);
        card->setAccessibleName(tr("Abrir %1").arg(archiveTitle));
        card->activated = [this, path]() { emit openProjectRequested(path); };
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(9);

        auto* cover = new QLabel;
        cover->setObjectName(QStringLiteral("projectCardCover"));
        cover->setAlignment(Qt::AlignCenter);
        cover->setFixedHeight(106);
        cover->setMinimumWidth(260);
        cover->setScaledContents(false);
        cover->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCover(cover, profile, genre);
        cardLayout->addWidget(cover);

        auto* cardTop = new QHBoxLayout;
        auto* genreLabel = new QLabel(genre.toUpper());
        genreLabel->setObjectName(QStringLiteral("projectCardGenre"));
        genreLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* state = new QLabel(status);
        state->setObjectName(QStringLiteral("projectCardState"));
        state->setAttribute(Qt::WA_TransparentForMouseEvents);
        cardTop->addWidget(genreLabel);
        cardTop->addStretch();
        cardTop->addWidget(state);
        cardLayout->addLayout(cardTop);

        auto* titleLabel = new QLabel(archiveTitle.isEmpty() ? tr("Proyecto sin nombre") : archiveTitle);
        titleLabel->setObjectName(QStringLiteral("projectCardTitle"));
        titleLabel->setWordWrap(true);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* storyLabel = new QLabel(storyTitle);
        storyLabel->setObjectName(QStringLiteral("projectCardArchive"));
        storyLabel->setWordWrap(true);
        storyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(storyLabel);

        auto* stats = new QGridLayout;
        stats->setHorizontalSpacing(13);
        const QStringList labels{tr("Fichas"), tr("Enlaces"), tr("Capítulos"), tr("Palabras")};
        const QList<int> values{characters, relationships, chapters, words};
        for (int i = 0; i < labels.size(); ++i) {
            auto* value = new QLabel(QLocale().toString(values.at(i)));
            value->setObjectName(QStringLiteral("projectStatValue"));
            value->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* label = new QLabel(labels.at(i));
            label->setObjectName(QStringLiteral("projectStatLabel"));
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            stats->addWidget(value, 0, i);
            stats->addWidget(label, 1, i);
        }
        cardLayout->addLayout(stats);
        cardLayout->addStretch(1);

        auto* footer = new QHBoxLayout;
        auto* saved = new QLabel(tr("Modificado: %1").arg(formatDate(path)));
        saved->setObjectName(QStringLiteral("projectSaved"));
        saved->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* remove = button(tr("Eliminar"), QStringLiteral("projectDelete"));
        auto* open = button(tr("Abrir →"), QStringLiteral("hubPrimary"));
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

    emptyState_->setVisible(visibleProjects == 0 && (!query.isEmpty() || !statusWanted.isEmpty()));
    cards_->setColumnStretch(0, 1);
    cards_->setColumnStretch(1, 1);
    cards_->setColumnStretch(2, 1);
    cards_->setRowStretch(row + 1, 1);
}

} // namespace wbw
