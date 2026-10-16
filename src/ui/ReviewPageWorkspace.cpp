#include "ui/ReviewPage.h"

#include "core/ArchiveDocument.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString sceneText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument legacy;
    legacy.setHtml(scene.value(QStringLiteral("content")).toString());
    return legacy.toPlainText();
}

int wordCount(const QString& text) {
    return text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
}

QString legacySectionTitle(int index) {
    switch (index) {
        case 0: return QObject::tr("Proyecto");
        case 2: return QObject::tr("Maquetación y salida");
        case 3: return QObject::tr("Ambientación");
        default: return QObject::tr("Revisión");
    }
}

QString legacySectionCopy(int index) {
    switch (index) {
        case 0: return QObject::tr("Identidad, metadatos, portada, banner y presentación de la obra.");
        case 2: return QObject::tr("Página, márgenes, tipografía, estructura y preparación editorial.");
        case 3: return QObject::tr("Spotify, YouTube y medios locales para acompañar la escritura.");
        default: return QObject::tr("Diagnóstico editorial del manuscrito.");
    }
}

QFrame* modeCard(const QString& title, const QString& copy, QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("reviewModeCard"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(9);
    auto* heading = new QLabel(title, frame);
    heading->setObjectName(QStringLiteral("reviewModeTitle"));
    auto* description = new QLabel(copy, frame);
    description->setObjectName(QStringLiteral("reviewModeCopy"));
    description->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(description);
    return frame;
}

} // namespace

void ReviewPage::installWorkspaceNavigation() {
    applyApprovedReviewChrome();
    if (!tabs_ || property("wbwReviewWorkspaceV2").toBool()) return;

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    auto* analysisTab = findChild<QWidget*>(QStringLiteral("reviewAnalysisTab"));
    auto* analysisLayout = analysisTab ? qobject_cast<QVBoxLayout*>(analysisTab->layout()) : nullptr;
    if (!root || !analysisTab || !analysisLayout) return;

    setProperty("wbwReviewWorkspaceV2", true);
    if (tabs_->tabBar()) tabs_->tabBar()->hide();
    if (auto* oldHeader = findChild<QWidget*>(QStringLiteral("reviewPageHeader"))) oldHeader->hide();
    if (auto* oldSections = findChild<QWidget*>(QStringLiteral("reviewSectionBar"))) oldSections->hide();

    auto* dashboard = findChild<QWidget*>(QStringLiteral("reviewDashboard"));
    const auto directCards = analysisTab->findChildren<QFrame*>(QStringLiteral("reviewCard"), Qt::FindDirectChildrenOnly);
    QFrame* configCard = directCards.size() > 0 ? directCards.at(0) : nullptr;
    QFrame* resultsCard = directCards.size() > 1 ? directCards.at(1) : nullptr;

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("reviewWorkspaceHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(24, 15, 24, 0);
    headerLayout->setSpacing(10);

    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(16);
    auto* copy = new QVBoxLayout;
    copy->setSpacing(2);
    auto* title = new QLabel(tr("Revisión"), header);
    title->setObjectName(QStringLiteral("reviewWorkspaceTitle"));
    auto* subtitle = new QLabel(tr("Consistencia, lenguaje, estructura, estilo y notas del manuscrito en una sola superficie editorial."), header);
    subtitle->setObjectName(QStringLiteral("reviewWorkspaceSubtitle"));
    subtitle->setWordWrap(true);
    copy->addWidget(title);
    copy->addWidget(subtitle);
    titleRow->addLayout(copy, 1);

    auto* more = new QToolButton(header);
    more->setObjectName(QStringLiteral("reviewWorkspaceMore"));
    more->setText(tr("Herramientas  ⋯"));
    more->setPopupMode(QToolButton::InstantPopup);
    more->setCursor(Qt::PointingHandCursor);
    auto* moreMenu = new QMenu(more);
    QAction* projectAction = moreMenu->addAction(tr("Proyecto y metadatos"));
    QAction* layoutAction = moreMenu->addAction(tr("Maquetación y salida"));
    QAction* mediaAction = moreMenu->addAction(tr("Ambientación"));
    more->setMenu(moreMenu);
    titleRow->addWidget(more, 0, Qt::AlignTop);
    headerLayout->addLayout(titleRow);

    auto* nav = new QFrame(header);
    nav->setObjectName(QStringLiteral("reviewWorkspaceNav"));
    auto* navLayout = new QHBoxLayout(nav);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(2);
    auto* group = new QButtonGroup(nav);
    group->setExclusive(true);

    const QStringList modeNames{
        tr("Resumen"), tr("Consistencia"), tr("Lenguaje"),
        tr("Estructura"), tr("Estilo"), tr("Notas")
    };
    QList<QPushButton*> modeButtons;
    for (int i = 0; i < modeNames.size(); ++i) {
        auto* button = new QPushButton(modeNames.at(i), nav);
        button->setObjectName(QStringLiteral("reviewWorkspaceSection"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("reviewMode", i);
        button->setChecked(i == 0);
        group->addButton(button);
        navLayout->addWidget(button);
        modeButtons.append(button);
    }
    navLayout->addStretch(1);
    headerLayout->addWidget(nav);
    root->insertWidget(0, header);

    auto* modes = new QStackedWidget(analysisTab);
    modes->setObjectName(QStringLiteral("reviewModeStack"));

    // Resumen: reutiliza el diagnóstico y los resultados reales ya existentes.
    auto* summaryPage = new QWidget(modes);
    auto* summaryLayout = new QVBoxLayout(summaryPage);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(10);
    if (dashboard) {
        dashboard->setParent(summaryPage);
        summaryLayout->addWidget(dashboard);
    }
    if (resultsCard) {
        resultsCard->setParent(summaryPage);
        summaryLayout->addWidget(resultsCard, 1);
    }
    modes->addWidget(summaryPage);

    // Consistencia: comprobaciones reales de metadatos de escena y títulos.
    auto* consistencyPage = new QWidget(modes);
    auto* consistencyLayout = new QVBoxLayout(consistencyPage);
    consistencyLayout->setContentsMargins(0, 0, 0, 0);
    auto* consistencyCard = modeCard(tr("Consistencia"), tr("Detecta huecos de continuidad estructural que sí puede verificar la aplicación: POV, localización, títulos y capítulos vacíos."), consistencyPage);
    auto* consistencyList = new QListWidget(consistencyCard);
    consistencyList->setObjectName(QStringLiteral("reviewConsistencyList"));
    qobject_cast<QVBoxLayout*>(consistencyCard->layout())->addWidget(consistencyList, 1);
    consistencyLayout->addWidget(consistencyCard, 1);
    modes->addWidget(consistencyPage);

    // Lenguaje: deriva sus incidencias del mismo analizador local, sin reescribir texto.
    auto* languagePage = new QWidget(modes);
    auto* languageLayout = new QVBoxLayout(languagePage);
    languageLayout->setContentsMargins(0, 0, 0, 0);
    auto* languageCard = modeCard(tr("Lenguaje"), tr("Repeticiones, muletillas y errores mecánicos detectados por el analizador local. Ningún cambio se aplica automáticamente."), languagePage);
    auto* languageList = new QListWidget(languageCard);
    languageList->setObjectName(QStringLiteral("reviewLanguageList"));
    qobject_cast<QVBoxLayout*>(languageCard->layout())->addWidget(languageList, 1);
    languageLayout->addWidget(languageCard, 1);
    modes->addWidget(languagePage);

    // Estructura: medidas reales por capítulo y escena.
    auto* structurePage = new QWidget(modes);
    auto* structureLayout = new QVBoxLayout(structurePage);
    structureLayout->setContentsMargins(0, 0, 0, 0);
    auto* structureCard = modeCard(tr("Estructura"), tr("Distribución del manuscrito calculada desde los capítulos y escenas actuales."), structurePage);
    auto* structureSummary = new QLabel(structureCard);
    structureSummary->setObjectName(QStringLiteral("reviewStructureSummary"));
    structureSummary->setWordWrap(true);
    auto* structureList = new QListWidget(structureCard);
    structureList->setObjectName(QStringLiteral("reviewStructureList"));
    qobject_cast<QVBoxLayout*>(structureCard->layout())->addWidget(structureSummary);
    qobject_cast<QVBoxLayout*>(structureCard->layout())->addWidget(structureList, 1);
    structureLayout->addWidget(structureCard, 1);
    modes->addWidget(structurePage);

    // Estilo: los parámetros existentes pasan a una sección propia en vez de ocupar el resumen.
    auto* stylePage = new QWidget(modes);
    auto* styleLayout = new QVBoxLayout(stylePage);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(10);
    auto* styleIntro = modeCard(tr("Estilo"), tr("Ajusta la ventana de repetición y tu lista personal de muletillas. El análisis permanece local y no destructivo."), stylePage);
    styleLayout->addWidget(styleIntro);
    if (configCard) {
        configCard->setParent(stylePage);
        styleLayout->addWidget(configCard);
    }
    styleLayout->addStretch(1);
    modes->addWidget(stylePage);

    // Notas: persistencia real dentro del perfil del proyecto.
    auto* notesPage = new QWidget(modes);
    auto* notesLayout = new QVBoxLayout(notesPage);
    notesLayout->setContentsMargins(0, 0, 0, 0);
    auto* notesCard = modeCard(tr("Notas de revisión"), tr("Apuntes editoriales del proyecto. Se guardan junto al resto de la obra."), notesPage);
    auto* notes = new QTextEdit(notesCard);
    notes->setObjectName(QStringLiteral("reviewNotes"));
    notes->setPlaceholderText(tr("Decisiones pendientes, problemas de continuidad, objetivos para la siguiente pasada…"));
    qobject_cast<QVBoxLayout*>(notesCard->layout())->addWidget(notes, 1);
    notesLayout->addWidget(notesCard, 1);
    modes->addWidget(notesPage);

    // Retira de la columna principal los bloques antiguos que ahora viven dentro del stack.
    for (QWidget* child : analysisTab->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (child == modes || child == summaryPage || child == consistencyPage || child == languagePage ||
            child == structurePage || child == stylePage || child == notesPage) continue;
        if (child->objectName() == QStringLiteral("reviewHero") || child->objectName() == QStringLiteral("reviewSectionBar")) child->hide();
    }
    analysisLayout->addWidget(modes, 1);
    analysisLayout->setContentsMargins(18, 14, 18, 18);
    analysisLayout->setSpacing(10);

    const auto populateConsistency = [this, consistencyList]() {
        consistencyList->clear();
        if (!document_) return;
        QSet<QString> sceneTitles;
        int issues = 0;
        const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
        for (int c = 0; c < chapters.size(); ++c) {
            const QJsonObject chapter = chapters.at(c).toObject();
            const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
            const QString chapterName = chapter.value(QStringLiteral("title")).toString(
                chapter.value(QStringLiteral("label")).toString(tr("Capítulo %1").arg(c + 1)));
            if (scenes.isEmpty()) {
                consistencyList->addItem(tr("%1 no contiene escenas.").arg(chapterName));
                ++issues;
            }
            for (int s = 0; s < scenes.size(); ++s) {
                const QJsonObject scene = scenes.at(s).toObject();
                const QString title = scene.value(QStringLiteral("title")).toString().trimmed();
                const QString display = title.isEmpty() ? tr("Escena %1.%2").arg(c + 1).arg(s + 1) : title;
                if (title.isEmpty()) {
                    consistencyList->addItem(tr("%1 no tiene título.").arg(display));
                    ++issues;
                } else if (sceneTitles.contains(title.toCaseFolded())) {
                    consistencyList->addItem(tr("Título de escena duplicado: “%1”.").arg(title));
                    ++issues;
                } else sceneTitles.insert(title.toCaseFolded());
                if (scene.value(QStringLiteral("pov")).toString().trimmed().isEmpty()) {
                    consistencyList->addItem(tr("%1 no tiene POV definido.").arg(display));
                    ++issues;
                }
                if (scene.value(QStringLiteral("location")).toString().trimmed().isEmpty()) {
                    consistencyList->addItem(tr("%1 no tiene ubicación definida.").arg(display));
                    ++issues;
                }
            }
        }
        if (!issues) consistencyList->addItem(tr("No se encontraron huecos estructurales verificables con los datos actuales."));
    };

    const auto populateLanguage = [this, languageList]() {
        languageList->clear();
        runAnalysis();
        if (!analysisResults_) return;
        for (int i = 0; i < analysisResults_->count(); ++i) {
            const QString text = analysisResults_->item(i)->text();
            if (text.startsWith(tr("Repetición cercana"), Qt::CaseInsensitive) ||
                text.startsWith(tr("Muletilla"), Qt::CaseInsensitive) ||
                text.startsWith(tr("Palabra duplicada"), Qt::CaseInsensitive) ||
                text.startsWith(tr("Espacios dobles"), Qt::CaseInsensitive)) {
                languageList->addItem(text);
            }
        }
        if (!languageList->count()) languageList->addItem(tr("No se encontraron incidencias lingüísticas con la configuración actual."));
    };

    const auto populateStructure = [this, structureSummary, structureList]() {
        structureList->clear();
        if (!document_) return;
        const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
        int scenesTotal = 0;
        int wordsTotal = 0;
        int longestWords = -1;
        int shortestWords = -1;
        QString longestName;
        QString shortestName;
        for (int c = 0; c < chapters.size(); ++c) {
            const QJsonObject chapter = chapters.at(c).toObject();
            const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
            int chapterWords = 0;
            for (int s = 0; s < scenes.size(); ++s) {
                const QJsonObject scene = scenes.at(s).toObject();
                const int words = wordCount(sceneText(scene));
                chapterWords += words;
                ++scenesTotal;
                const QString name = scene.value(QStringLiteral("title")).toString(tr("Escena %1.%2").arg(c + 1).arg(s + 1));
                if (longestWords < 0 || words > longestWords) { longestWords = words; longestName = name; }
                if (shortestWords < 0 || words < shortestWords) { shortestWords = words; shortestName = name; }
            }
            wordsTotal += chapterWords;
            const QString chapterName = chapter.value(QStringLiteral("title")).toString(
                chapter.value(QStringLiteral("label")).toString(tr("Capítulo %1").arg(c + 1)));
            structureList->addItem(tr("%1 — %2 escenas · %3 palabras").arg(chapterName).arg(scenes.size()).arg(chapterWords));
        }
        const int average = scenesTotal > 0 ? wordsTotal / scenesTotal : 0;
        structureSummary->setText(tr("%1 capítulos · %2 escenas · %3 palabras · promedio %4 palabras por escena.")
                                      .arg(chapters.size()).arg(scenesTotal).arg(wordsTotal).arg(average));
        if (longestWords >= 0) structureList->addItem(tr("Escena más larga: %1 — %2 palabras").arg(longestName).arg(longestWords));
        if (shortestWords >= 0) structureList->addItem(tr("Escena más corta: %1 — %2 palabras").arg(shortestName).arg(shortestWords));
    };

    const auto loadNotes = [this, notes]() {
        if (!document_) {
            const QSignalBlocker blocker(notes);
            notes->clear();
            return;
        }
        const QSignalBlocker blocker(notes);
        notes->setPlainText(document_->object(QStringLiteral("profile")).value(QStringLiteral("reviewNotes")).toString());
    };

    connect(notes, &QTextEdit::textChanged, this, [this, notes]() {
        if (!document_) return;
        QJsonObject profile = document_->object(QStringLiteral("profile"));
        if (profile.value(QStringLiteral("reviewNotes")).toString() == notes->toPlainText()) return;
        profile.insert(QStringLiteral("reviewNotes"), notes->toPlainText());
        document_->setObject(QStringLiteral("profile"), profile);
        emit changed();
    });

    const auto activateMode = [this, modes, title, subtitle, populateConsistency, populateLanguage, populateStructure, loadNotes](int mode) {
        if (!tabs_) return;
        tabs_->setCurrentIndex(1);
        modes->setCurrentIndex(qBound(0, mode, modes->count() - 1));
        title->setText(tr("Revisión"));
        switch (mode) {
            case 0:
                subtitle->setText(tr("Panorama general del manuscrito, incidencias y salida editorial."));
                refreshReviewDashboard();
                break;
            case 1:
                subtitle->setText(tr("Huecos verificables de continuidad estructural y metadatos de escena."));
                populateConsistency();
                break;
            case 2:
                subtitle->setText(tr("Repeticiones, muletillas y errores mecánicos detectados localmente."));
                populateLanguage();
                break;
            case 3:
                subtitle->setText(tr("Distribución real de capítulos, escenas y extensión del manuscrito."));
                populateStructure();
                break;
            case 4:
                subtitle->setText(tr("Parámetros del análisis de estilo y repetición."));
                break;
            case 5:
                subtitle->setText(tr("Notas editoriales persistentes del proyecto."));
                loadNotes();
                break;
            default:
                break;
        }
    };

    for (QPushButton* button : modeButtons) {
        connect(button, &QPushButton::clicked, this, [button, activateMode]() {
            activateMode(button->property("reviewMode").toInt());
        });
    }

    const auto openLegacy = [this, title, subtitle, group](int target) {
        if (!tabs_ || target < 0 || target >= tabs_->count()) return;
        group->setExclusive(false);
        for (QAbstractButton* button : group->buttons()) button->setChecked(false);
        group->setExclusive(true);
        tabs_->setCurrentIndex(target);
        title->setText(legacySectionTitle(target));
        subtitle->setText(legacySectionCopy(target));
    };
    connect(projectAction, &QAction::triggered, this, [openLegacy]() { openLegacy(0); });
    connect(layoutAction, &QAction::triggered, this, [openLegacy]() { openLegacy(2); });
    connect(mediaAction, &QAction::triggered, this, [openLegacy]() { openLegacy(3); });

    tabs_->setCurrentIndex(1);
    modes->setCurrentIndex(0);
    refreshReviewDashboard();

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#reviewWorkspaceHeader[reviewTheme="dark"]{background:#0b1016;border:0;border-bottom:1px solid #26313b;}
#reviewWorkspaceHeader[reviewTheme="light"]{background:#f7f5f0;border:0;border-bottom:1px solid #d8d0c4;}
#reviewWorkspaceTitle[reviewTheme="dark"]{color:#f3efe8;font-family:'Georgia';font-size:24pt;font-weight:600;}
#reviewWorkspaceTitle[reviewTheme="light"]{color:#302d28;font-family:'Georgia';font-size:24pt;font-weight:600;}
#reviewWorkspaceSubtitle[reviewTheme="dark"]{color:#7f8b96;font-size:9pt;}
#reviewWorkspaceSubtitle[reviewTheme="light"]{color:#756f66;font-size:9pt;}
#reviewWorkspaceNav{background:transparent;border:0;}
#reviewWorkspaceSection[reviewTheme="dark"]{background:transparent;color:#8f9ba6;border:0;border-bottom:2px solid transparent;border-radius:0;padding:9px 13px;}
#reviewWorkspaceSection[reviewTheme="dark"]:hover{color:#eef4f8;}
#reviewWorkspaceSection[reviewTheme="dark"]:checked{color:#89d0e3;border-bottom-color:#3fc1c6;}
#reviewWorkspaceSection[reviewTheme="light"]{background:transparent;color:#6e675e;border:0;border-bottom:2px solid transparent;border-radius:0;padding:9px 13px;}
#reviewWorkspaceSection[reviewTheme="light"]:hover{color:#2e2a25;}
#reviewWorkspaceSection[reviewTheme="light"]:checked{color:#23696d;border-bottom-color:#4b9699;}
#reviewWorkspaceMore[reviewTheme="dark"]{background:#111922;color:#aeb9c3;border:1px solid #2e3a46;border-radius:7px;padding:7px 10px;}
#reviewWorkspaceMore[reviewTheme="light"]{background:#f8f4ed;color:#625c54;border:1px solid #d2c9bd;border-radius:7px;padding:7px 10px;}
#reviewModeStack{background:transparent;border:0;}
#reviewModeCard[reviewTheme="dark"]{background:#10161d;border:1px solid #27323d;border-radius:10px;}
#reviewModeCard[reviewTheme="light"]{background:#fffdf8;border:1px solid #d8d1c6;border-radius:10px;}
#reviewModeTitle[reviewTheme="dark"]{color:#eee9e1;font-family:'Georgia';font-size:15pt;font-weight:600;}
#reviewModeTitle[reviewTheme="light"]{color:#37322c;font-family:'Georgia';font-size:15pt;font-weight:600;}
#reviewModeCopy[reviewTheme="dark"]{color:#87939e;}
#reviewModeCopy[reviewTheme="light"]{color:#746d64;}
#reviewStructureSummary[reviewTheme="dark"]{background:#0d1319;color:#cbd5de;border:1px solid #27323d;border-radius:7px;padding:10px;}
#reviewStructureSummary[reviewTheme="light"]{background:#f7f3ec;color:#514b44;border:1px solid #d8d1c6;border-radius:7px;padding:10px;}

#reviewPage[reviewTheme="dark"],#reviewTabs[reviewTheme="dark"]::pane,#reviewProjectTab[reviewTheme="dark"],#reviewAnalysisTab[reviewTheme="dark"],#reviewLayoutTab[reviewTheme="dark"],#reviewMediaTab[reviewTheme="dark"]{background:#0b0f14;color:#dfe4ea;border:0;}
#reviewPage[reviewTheme="light"],#reviewTabs[reviewTheme="light"]::pane,#reviewProjectTab[reviewTheme="light"],#reviewAnalysisTab[reviewTheme="light"],#reviewLayoutTab[reviewTheme="light"],#reviewMediaTab[reviewTheme="light"]{background:#f3f1ec;color:#302d28;border:0;}
#reviewCard[reviewTheme="dark"],#reviewDiagnosis[reviewTheme="dark"],#reviewStats[reviewTheme="dark"],#reviewExportRail[reviewTheme="dark"],#reviewIssueCard[reviewTheme="dark"]{background:#10161d;border:1px solid #27323d;border-radius:10px;}
#reviewCard[reviewTheme="light"],#reviewDiagnosis[reviewTheme="light"],#reviewStats[reviewTheme="light"],#reviewExportRail[reviewTheme="light"],#reviewIssueCard[reviewTheme="light"]{background:#fffdf8;border:1px solid #d8d1c6;border-radius:10px;}
#reviewCardTitle[reviewTheme="dark"],#reviewFieldTitle[reviewTheme="dark"],#reviewExportTitle[reviewTheme="dark"],#reviewIssueTitle[reviewTheme="dark"],#reviewDiagnosisTitle[reviewTheme="dark"]{color:#eee9e1;}
#reviewCardTitle[reviewTheme="light"],#reviewFieldTitle[reviewTheme="light"],#reviewExportTitle[reviewTheme="light"],#reviewIssueTitle[reviewTheme="light"],#reviewDiagnosisTitle[reviewTheme="light"]{color:#37322c;}
#reviewDescription[reviewTheme="dark"],#reviewExportCopy[reviewTheme="dark"],#reviewDiagnosisCopy[reviewTheme="dark"]{color:#87939e;}
#reviewDescription[reviewTheme="light"],#reviewExportCopy[reviewTheme="light"],#reviewDiagnosisCopy[reviewTheme="light"]{color:#746d64;}
QLineEdit[reviewTheme="dark"],QTextEdit[reviewTheme="dark"],QComboBox[reviewTheme="dark"],QSpinBox[reviewTheme="dark"],QDoubleSpinBox[reviewTheme="dark"],QListWidget[reviewTheme="dark"]{background:#0d1319;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;selection-background-color:#254b63;}
QLineEdit[reviewTheme="light"],QTextEdit[reviewTheme="light"],QComboBox[reviewTheme="light"],QSpinBox[reviewTheme="light"],QDoubleSpinBox[reviewTheme="light"],QListWidget[reviewTheme="light"]{background:#fffefb;color:#302d28;border:1px solid #d4cdc2;border-radius:7px;selection-background-color:#cce0e1;selection-color:#203336;}
#reviewImage[reviewTheme="dark"]{background:#0d1319;color:#77838e;border:1px solid #2b3742;}
#reviewImage[reviewTheme="light"]{background:#f7f3ec;color:#81786e;border:1px solid #d4cdc2;}
#reviewScore[reviewTheme="light"]{background:#fffdf8;color:#302d28;border-color:#399b72;}
#reviewMetric[reviewTheme="light"]{background:#f8f5ef;border-color:#ded6ca;}
#reviewMetricValue[reviewTheme="light"]{color:#312e29;}
#reviewMetricLabel[reviewTheme="light"]{color:#756e65;}
#reviewIssueRow[reviewTheme="light"]{background:#f5f1ea;color:#514b44;}
#reviewAnalysisTab[reviewTheme="light"] QListWidget{background:#fffdf8;color:#37322d;border-color:#d8d1c6;}
#reviewAnalysisTab[reviewTheme="light"] QListWidget::item{border-bottom:1px solid #e5ded4;}
#reviewAnalysisTab[reviewTheme="light"] QListWidget::item:selected{background:#e0ecec;color:#285e62;}
QScrollArea[reviewTheme="dark"]>QWidget>QWidget{background:#0b0f14;}
QScrollArea[reviewTheme="light"]>QWidget>QWidget{background:#f3f1ec;}
)QSS"));

    refreshProductTheme();
}

void ReviewPage::refreshProductTheme() {
    const QString theme = (qApp && qApp->property("wbwDarkMode").toBool()) ? QStringLiteral("dark") : QStringLiteral("light");
    setProperty("reviewTheme", theme);
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget) continue;
        widget->setProperty("reviewTheme", theme);
        if (widget->style()) {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }
        widget->update();
    }
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }
    update();
}

} // namespace wbw
