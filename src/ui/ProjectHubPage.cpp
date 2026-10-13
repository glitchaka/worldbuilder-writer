#include "ui/ProjectHubPage.h"

#include "core/ArchiveDocument.h"
#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
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
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
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

QPushButton* button(const QString& text, const QString& objectName) {
    auto* b = new QPushButton(text);
    b->setObjectName(objectName);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

int manuscriptWords(const ArchiveDocument& document) {
    int words = 0;
    for (const QJsonValue chapterValue : document.array(QStringLiteral("writingChapters"))) {
        for (const QJsonValue sceneValue : chapterValue.toObject().value(QStringLiteral("scenes")).toArray()) {
            const QJsonObject scene = sceneValue.toObject();
            QString content = scene.contains(QStringLiteral("text"))
                ? scene.value(QStringLiteral("text")).toString()
                : scene.value(QStringLiteral("content")).toString();
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

QByteArray dataUrlBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive)
        ? QByteArray::fromBase64(payload)
        : QByteArray::fromPercentEncoding(payload);
}

void setCover(QLabel* label, const QJsonObject& profile, const QString& fallback) {
    QPixmap pixmap;
    const QByteArray bytes = dataUrlBytes(profile.value(QStringLiteral("coverImageDataUrl")).toString());
    if (!bytes.isEmpty()) pixmap.loadFromData(bytes);
    if (!pixmap.isNull()) {
        label->setText({});
        label->setPixmap(pixmap.scaled(QSize(356, 170), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        label->setText(fallback.toUpper());
    }
}

QString statusClass(QString status) {
    status = status.toLower();
    if (status.contains(QStringLiteral("plan"))) return QStringLiteral("plan");
    if (status.contains(QStringLiteral("borr"))) return QStringLiteral("draft");
    if (status.contains(QStringLiteral("desarrollo"))) return QStringLiteral("dev");
    if (status.contains(QStringLiteral("idea"))) return QStringLiteral("idea");
    if (status.contains(QStringLiteral("final"))) return QStringLiteral("final");
    return QStringLiteral("neutral");
}

} // namespace

ProjectHubPage::ProjectHubPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("projectHubPage"));

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("hubScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* shell = new QWidget(scroll);
    shell->setObjectName(QStringLiteral("hubShell"));
    auto* root = new QVBoxLayout(shell);
    root->setContentsMargins(34, 26, 34, 34);
    root->setSpacing(18);

    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(18);
    auto* heading = new QVBoxLayout;
    heading->setSpacing(3);
    auto* title = new QLabel(tr("Tus historias"), shell);
    title->setObjectName(QStringLiteral("hubTitle"));
    auto* description = new QLabel(tr("Crea, organiza y da vida a tus mundos. Todos tus proyectos en un solo lugar."), shell);
    description->setObjectName(QStringLiteral("hubDescription"));
    heading->addWidget(title);
    heading->addWidget(description);
    titleRow->addLayout(heading, 1);

    auto* viewGrid = button(QStringLiteral("▦"), QStringLiteral("hubViewButton"));
    auto* viewList = button(QStringLiteral("☷"), QStringLiteral("hubViewButton"));
    viewGrid->setCheckable(true);
    viewGrid->setChecked(true);
    viewGrid->setFixedSize(42, 38);
    viewList->setFixedSize(42, 38);
    titleRow->addWidget(viewGrid, 0, Qt::AlignBottom);
    titleRow->addWidget(viewList, 0, Qt::AlignBottom);

    auto* recents = new QComboBox(shell);
    recents->setObjectName(QStringLiteral("hubSort"));
    recents->addItems({tr("Recientes"), tr("Nombre"), tr("Más antiguos")});
    recents->setFixedWidth(136);
    titleRow->addWidget(recents, 0, Qt::AlignBottom);

    auto* genre = new QComboBox(shell);
    genre->setObjectName(QStringLiteral("hubGenreFilter"));
    genre->addItems({tr("Todos los géneros"), tr("Fantasía"), tr("Ciencia ficción"), tr("Misterio"), tr("Terror")});
    genre->setFixedWidth(166);
    titleRow->addWidget(genre, 0, Qt::AlignBottom);

    statusFilter_ = new QComboBox(shell);
    statusFilter_->setObjectName(QStringLiteral("hubStatusFilter"));
    statusFilter_->addItems({tr("Todos los estados"), tr("Planificación"), tr("Borrador"), tr("En desarrollo"), tr("Revisión"), tr("Final")});
    statusFilter_->setFixedWidth(172);
    titleRow->addWidget(statusFilter_, 0, Qt::AlignBottom);
    root->addLayout(titleRow);

    search_ = new QLineEdit(shell);
    search_->setObjectName(QStringLiteral("hubSearch"));
    search_->setPlaceholderText(tr("Buscar proyectos…"));
    search_->setClearButtonEnabled(true);
    search_->setMaximumWidth(420);
    search_->hide();
    root->addWidget(search_);

    cardsHost_ = new QWidget(shell);
    cardsHost_->setObjectName(QStringLiteral("hubCardsHost"));
    cards_ = new QGridLayout(cardsHost_);
    cards_->setContentsMargins(0, 0, 0, 0);
    cards_->setHorizontalSpacing(16);
    cards_->setVerticalSpacing(16);
    cards_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    root->addWidget(cardsHost_, 1);

    emptyState_ = new QLabel(tr("No hay proyectos que coincidan con los filtros."), shell);
    emptyState_->setObjectName(QStringLiteral("hubEmptyState"));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->hide();
    root->addWidget(emptyState_);

    scroll->setWidget(shell);
    page->addWidget(scroll, 1);

    setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubScroll,#hubScroll>QWidget>QWidget,#hubShell,#hubCardsHost{background:#0b0f14;color:#dfe4ea;}
#hubTitle{font-family:"Georgia";font-size:30pt;font-weight:600;color:#f3efe8;}
#hubDescription{font-size:10.5pt;color:#7f8995;}
#hubSort,#hubGenreFilter,#hubStatusFilter,#hubSearch{background:#0f151c;color:#dfe5eb;border:1px solid #2b3440;border-radius:8px;padding:8px 11px;min-height:20px;}
#hubSort:hover,#hubGenreFilter:hover,#hubStatusFilter:hover,#hubSearch:focus{border-color:#456786;}
#hubViewButton{background:#10161d;color:#9da9b7;border:1px solid #2a333d;border-radius:8px;font-size:15pt;}
#hubViewButton:checked,#hubViewButton:hover{background:#162334;color:#79b9f2;border-color:#3a6c98;}
#projectCard,#projectCardNew{background:#11171e;border:1px solid #2b343d;border-radius:12px;}
#projectCard:hover,#projectCard:focus{background:#141b23;border-color:#426b8d;}
#projectCardNew{border-style:dashed;border-color:#3b4651;}
#projectCardNew:hover,#projectCardNew:focus{background:#111a24;border-color:#6096c3;}
#projectCardCover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #263646,stop:.5 #151e28,stop:1 #0d1218);border:0;border-radius:9px;color:#a9bac9;font-size:9pt;font-weight:700;letter-spacing:1.2px;}
#projectCardState{border-radius:10px;padding:3px 8px;font-size:7.5pt;font-weight:700;}
#projectCardState[statusClass="plan"]{background:#173529;color:#7bd2ad;border:1px solid #2b6b55;}
#projectCardState[statusClass="draft"]{background:#3a2f14;color:#f2cf63;border:1px solid #78621f;}
#projectCardState[statusClass="dev"]{background:#2c2147;color:#c7a8ff;border:1px solid #62489a;}
#projectCardState[statusClass="idea"]{background:#17314d;color:#8ec7ff;border:1px solid #345f8b;}
#projectCardState[statusClass="final"]{background:#17362e;color:#7fdbbf;border:1px solid #2d705d;}
#projectCardState[statusClass="neutral"]{background:#202832;color:#b8c2cc;border:1px solid #394552;}
#projectCardTitle,#newProjectTitle{font-family:"Georgia";font-size:17pt;font-weight:700;color:#f3efe8;}
#projectCardArchive,#newProjectCopy{color:#89939e;font-size:9pt;}
#projectCardGenre{color:#8d98a4;font-size:8.5pt;}
#projectStatValue{font-weight:700;color:#edf2f7;font-size:10pt;}
#projectStatLabel,#projectSaved{color:#727d87;font-size:8pt;}
#newProjectMark{background:#10161e;color:#dfe9f3;border:1px solid #5e9ee0;border-radius:34px;font-size:24pt;font-weight:300;}
#newProjectOption{background:transparent;color:#8ebff1;border:0;padding:6px 8px;text-align:left;}
#newProjectOption:hover{color:#d7ecff;}
#projectDelete{background:transparent;color:#7f8b96;border:0;padding:5px 6px;}
#projectDelete:hover{color:#f08c8c;}
#hubEmptyState{color:#77828d;padding:36px;}
)QSS"));

    connect(search_, &QLineEdit::textChanged, this, [this]() { rebuildCards(); });
    connect(statusFilter_, &QComboBox::currentTextChanged, this, [this]() { rebuildCards(); });
    connect(genre, &QComboBox::currentTextChanged, this, [this]() { rebuildCards(); });
    refresh();
}

void ProjectHubPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() { rebuildCards(); });
}

void ProjectHubPage::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

void ProjectHubPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    static int lastWidth = 0;
    if (qAbs(width() - lastWidth) > 120) {
        lastWidth = width();
        QTimer::singleShot(0, this, [this]() { rebuildCards(); });
    }
}

int ProjectHubPage::cardColumns() const {
    const int w = cardsHost_ ? cardsHost_->width() : width();
    if (w >= 1320) return 4;
    if (w >= 980) return 3;
    if (w >= 650) return 2;
    return 1;
}

void ProjectHubPage::refresh() {
    rebuildCards();
}

void ProjectHubPage::rebuildCards() {
    while (QLayoutItem* item = cards_->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    const int columns = cardColumns();
    const int cardWidth = columns >= 4 ? 300 : columns == 3 ? 310 : 330;
    int itemIndex = 0;
    int visibleProjects = 0;

    auto* newCard = new ClickableCard(cardsHost_);
    newCard->setObjectName(QStringLiteral("projectCardNew"));
    newCard->setFixedSize(cardWidth, 350);
    newCard->setAccessibleName(tr("Nueva obra"));
    auto* newLayout = new QVBoxLayout(newCard);
    newLayout->setContentsMargins(24, 24, 24, 22);
    newLayout->setSpacing(10);
    newLayout->addStretch();
    auto* plus = new QLabel(QStringLiteral("+"), newCard);
    plus->setObjectName(QStringLiteral("newProjectMark"));
    plus->setAlignment(Qt::AlignCenter);
    plus->setFixedSize(68, 68);
    newLayout->addWidget(plus, 0, Qt::AlignHCenter);
    auto* newTitle = new QLabel(tr("Nueva obra"), newCard);
    newTitle->setObjectName(QStringLiteral("newProjectTitle"));
    newTitle->setAlignment(Qt::AlignCenter);
    newLayout->addWidget(newTitle);
    auto* newCopy = new QLabel(tr("Comienza un proyecto completamente\nnuevo desde cero."), newCard);
    newCopy->setObjectName(QStringLiteral("newProjectCopy"));
    newCopy->setAlignment(Qt::AlignCenter);
    newLayout->addWidget(newCopy);
    newLayout->addSpacing(14);
    auto* options = new QHBoxLayout;
    auto* blank = button(tr("▤  Obra en blanco"), QStringLiteral("newProjectOption"));
    auto* templ = button(tr("▣  Desde plantilla"), QStringLiteral("newProjectOption"));
    options->addWidget(blank);
    options->addWidget(templ);
    newLayout->addLayout(options);
    newLayout->addStretch();
    newCard->activated = [this]() { emit newProjectRequested(); };
    connect(blank, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    connect(templ, &QPushButton::clicked, this, &ProjectHubPage::newProjectRequested);
    cards_->addWidget(newCard, 0, 0, Qt::AlignTop | Qt::AlignLeft);
    ++itemIndex;

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

        const int chapters = loaded ? document.array(QStringLiteral("writingChapters")).size() : 0;
        const int words = loaded ? manuscriptWords(document) : 0;
        ++visibleProjects;

        auto* card = new ClickableCard(cardsHost_);
        card->setObjectName(QStringLiteral("projectCard"));
        card->setFixedSize(cardWidth, 350);
        card->setAccessibleName(tr("Abrir %1").arg(archiveTitle));
        card->activated = [this, path]() { emit openProjectRequested(path); };

        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        auto* cover = new QLabel(card);
        cover->setObjectName(QStringLiteral("projectCardCover"));
        cover->setFixedHeight(142);
        cover->setAlignment(Qt::AlignCenter);
        setCover(cover, profile, genre);
        layout->addWidget(cover);

        auto* statusBadge = new QLabel(status.toUpper(), cover);
        statusBadge->setObjectName(QStringLiteral("projectCardState"));
        statusBadge->setProperty("statusClass", statusClass(status));
        statusBadge->adjustSize();
        statusBadge->move(10, 10);
        statusBadge->raise();

        auto* titleLabel = new QLabel(archiveTitle.isEmpty() ? tr("Proyecto sin nombre") : archiveTitle, card);
        titleLabel->setObjectName(QStringLiteral("projectCardTitle"));
        titleLabel->setWordWrap(true);
        layout->addWidget(titleLabel);

        auto* meta = new QLabel(QStringLiteral("%1   •   %2").arg(genre, storyTitle), card);
        meta->setObjectName(QStringLiteral("projectCardGenre"));
        meta->setWordWrap(true);
        layout->addWidget(meta);

        auto* stats = new QHBoxLayout;
        auto* chaptersValue = new QLabel(QStringLiteral("▤  %1").arg(QLocale().toString(chapters)), card);
        chaptersValue->setObjectName(QStringLiteral("projectStatValue"));
        auto* chaptersLabel = new QLabel(tr("Capítulos"), card);
        chaptersLabel->setObjectName(QStringLiteral("projectStatLabel"));
        auto* chapterBox = new QVBoxLayout;
        chapterBox->setSpacing(0);
        chapterBox->addWidget(chaptersValue);
        chapterBox->addWidget(chaptersLabel);
        auto* wordsValue = new QLabel(QStringLiteral("✎  %1").arg(QLocale().toString(words)), card);
        wordsValue->setObjectName(QStringLiteral("projectStatValue"));
        auto* wordsLabel = new QLabel(tr("Palabras"), card);
        wordsLabel->setObjectName(QStringLiteral("projectStatLabel"));
        auto* wordBox = new QVBoxLayout;
        wordBox->setSpacing(0);
        wordBox->addWidget(wordsValue);
        wordBox->addWidget(wordsLabel);
        stats->addLayout(chapterBox);
        stats->addStretch();
        stats->addLayout(wordBox);
        layout->addLayout(stats);

        auto* footer = new QHBoxLayout;
        auto* saved = new QLabel(QStringLiteral("▣  %1").arg(formatDate(path)), card);
        saved->setObjectName(QStringLiteral("projectSaved"));
        auto* remove = button(QStringLiteral("⋯"), QStringLiteral("projectDelete"));
        remove->setFixedWidth(30);
        footer->addWidget(saved);
        footer->addStretch();
        footer->addWidget(remove);
        layout->addLayout(footer);

        connect(remove, &QPushButton::clicked, this, [this, path, archiveTitle, remove]() {
            QMenu menu(this);
            QAction* openAction = menu.addAction(tr("Abrir proyecto"));
            QAction* exportAction = menu.addAction(tr("Exportar proyecto .wbw…"));
            menu.addSeparator();
            QAction* deleteAction = menu.addAction(tr("Eliminar definitivamente"));
            QAction* chosen = menu.exec(remove->mapToGlobal(QPoint(0, remove->height())));
            if (chosen == openAction) {
                emit openProjectRequested(path);
                return;
            }
            if (chosen == exportAction) {
                ArchiveDocument source;
                QString error;
                if (!ProjectStore::loadJsonFile(path, source, &error)) {
                    QMessageBox::critical(this, tr("No se pudo abrir"), error);
                    return;
                }
                QString target = QFileDialog::getSaveFileName(this, tr("Exportar proyecto .wbw"),
                    (archiveTitle.trimmed().isEmpty() ? tr("proyecto") : archiveTitle) + QStringLiteral(".wbw"),
                    tr("Proyecto Worldbuilder Writer (*.wbw)"));
                if (target.isEmpty()) return;
                if (!target.endsWith(QStringLiteral(".wbw"), Qt::CaseInsensitive)) target += QStringLiteral(".wbw");
                if (!WbwPackage::exportPackage(target, source, &error)) {
                    QMessageBox::critical(this, tr("No se pudo exportar"), error);
                    return;
                }
                return;
            }
            if (chosen != deleteAction) return;
            if (QMessageBox::question(this, tr("Eliminar proyecto"), tr("¿Eliminar definitivamente “%1”? Esta acción no se puede deshacer.").arg(archiveTitle)) != QMessageBox::Yes) return;
            QString error;
            if (!ProjectStore::removeProject(path, &error)) {
                QMessageBox::critical(this, tr("No se pudo eliminar"), error);
                return;
            }
            refresh();
            emit projectDeleted();
        });

        cards_->addWidget(card, itemIndex / columns, itemIndex % columns, Qt::AlignTop | Qt::AlignLeft);
        ++itemIndex;
    }

    emptyState_->setVisible(visibleProjects == 0 && (!query.isEmpty() || !statusWanted.isEmpty()));
    for (int c = 0; c < columns; ++c) cards_->setColumnStretch(c, 0);
    cards_->setColumnStretch(columns, 1);
    cards_->setRowStretch((itemIndex + columns - 1) / columns, 1);
}

} // namespace wbw

