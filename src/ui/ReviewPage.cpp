#include "ui/ReviewPage.h"

#include "core/ArchiveDocument.h"

#include <QAudioOutput>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace wbw {
namespace {

QPushButton* makeButton(const QString& text) {
    auto* button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QString jsonStrings(const QJsonArray& array) {
    QStringList values;
    for (const QJsonValue value : array) if (!value.toString().trimmed().isEmpty()) values.append(value.toString().trimmed());
    return values.join(QStringLiteral(", "));
}

QJsonArray stringsJson(const QString& text) {
    QJsonArray result;
    for (QString value : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        value = value.trimmed();
        if (!value.isEmpty()) result.append(value);
    }
    return result;
}

QByteArray dataUrlBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive) ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload);
}

void setPreview(QLabel* label, const QString& dataUrl, const QString& emptyText) {
    QPixmap pixmap;
    const QByteArray bytes = dataUrlBytes(dataUrl);
    if (!bytes.isEmpty()) pixmap.loadFromData(bytes);
    if (pixmap.isNull()) {
        label->setPixmap(QPixmap());
        label->setText(emptyText);
    } else {
        label->setText(QString());
        label->setPixmap(pixmap.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

QJsonObject defaultLayoutFor(const QString& preset) {
    QJsonObject layout{
        {QStringLiteral("preset"), preset}, {QStringLiteral("pageWidthMm"), 152.4}, {QStringLiteral("pageHeightMm"), 228.6},
        {QStringLiteral("marginTopMm"), 20.0}, {QStringLiteral("marginRightMm"), 19.0}, {QStringLiteral("marginBottomMm"), 22.0},
        {QStringLiteral("marginLeftMm"), 19.0}, {QStringLiteral("fontFamily"), QStringLiteral("Garamond")},
        {QStringLiteral("fontSizePt"), 11.0}, {QStringLiteral("lineHeight"), 1.35}, {QStringLiteral("paragraphIndentMm"), 5.0},
        {QStringLiteral("chapterOpening"), QStringLiteral("Página nueva")}, {QStringLiteral("sceneSeparator"), QStringLiteral("⁂")},
        {QStringLiteral("headerText"), QStringLiteral("{título}")}, {QStringLiteral("footerText"), QStringLiteral("{página}")}
    };
    if (preset == QStringLiteral("novela")) {
        layout.insert(QStringLiteral("pageWidthMm"), 140.0); layout.insert(QStringLiteral("pageHeightMm"), 216.0); layout.insert(QStringLiteral("lineHeight"), 1.3);
    } else if (preset == QStringLiteral("bolsillo")) {
        layout.insert(QStringLiteral("pageWidthMm"), 120.0); layout.insert(QStringLiteral("pageHeightMm"), 190.0);
        layout.insert(QStringLiteral("marginTopMm"), 16.0); layout.insert(QStringLiteral("marginRightMm"), 15.0);
        layout.insert(QStringLiteral("marginBottomMm"), 18.0); layout.insert(QStringLiteral("marginLeftMm"), 15.0);
        layout.insert(QStringLiteral("fontSizePt"), 9.5); layout.insert(QStringLiteral("lineHeight"), 1.2);
    } else if (preset == QStringLiteral("fantasia")) {
        layout.insert(QStringLiteral("fontFamily"), QStringLiteral("Georgia")); layout.insert(QStringLiteral("lineHeight"), 1.4);
        layout.insert(QStringLiteral("chapterOpening"), QStringLiteral("Página impar"));
    } else if (preset == QStringLiteral("cronica")) {
        layout.insert(QStringLiteral("pageWidthMm"), 170.0); layout.insert(QStringLiteral("pageHeightMm"), 240.0);
        layout.insert(QStringLiteral("fontFamily"), QStringLiteral("Atkinson")); layout.insert(QStringLiteral("fontSizePt"), 10.5);
        layout.insert(QStringLiteral("lineHeight"), 1.3); layout.insert(QStringLiteral("paragraphIndentMm"), 0.0);
    }
    return layout;
}

QString scenePlainText(const QJsonObject& scene) {
    QTextDocument document;
    document.setHtml(scene.value(QStringLiteral("content")).toString());
    return document.toPlainText();
}

QString allManuscriptText(const ArchiveDocument& document) {
    QStringList parts;
    for (const QJsonValue chapterValue : document.array(QStringLiteral("writingChapters"))) {
        for (const QJsonValue sceneValue : chapterValue.toObject().value(QStringLiteral("scenes")).toArray())
            parts.append(scenePlainText(sceneValue.toObject()));
    }
    return parts.join(QStringLiteral("\n\n"));
}

QFrame* card(const QString& title, QLayout* body) {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("reviewCard"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(10);
    auto* heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("reviewCardTitle"));
    layout->addWidget(heading);
    layout->addLayout(body);
    return frame;
}

QWidget* labeled(const QString& title, QWidget* field) {
    auto* box = new QWidget;
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto* label = new QLabel(title);
    label->setObjectName(QStringLiteral("reviewFieldTitle"));
    layout->addWidget(label);
    layout->addWidget(field);
    return box;
}

} // namespace

ReviewPage::ReviewPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("reviewPage"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("reviewTabs"));
    tabs_->addTab(buildProjectTab(), tr("Proyecto"));
    tabs_->addTab(buildAnalysisTab(), tr("Revisión"));
    tabs_->addTab(buildLayoutTab(), tr("Maquetación y salida"));
    tabs_->addTab(buildMediaTab(), tr("Ambientación"));
    root->addWidget(tabs_);

    setStyleSheet(QStringLiteral(
        "#reviewPage{background:#edf1f5;}"
        "#reviewTabs::pane{border:0;background:#edf1f5;}"
        "#reviewProjectTab,#reviewAnalysisTab,#reviewLayoutTab,#reviewMediaTab{background:#edf1f5;}"
        "#reviewHero{background:#f5f8fc;border:1px solid #d7dee8;}"
        "#reviewKicker,#reviewFieldTitle{color:#667085;font-size:8pt;font-weight:700;letter-spacing:.6px;}"
        "#reviewTitle{font-family:'Georgia';font-size:25pt;color:#344054;}"
        "#reviewDescription{font-family:'Georgia';color:#667085;}"
        "#reviewCard{background:#ffffff;border:1px solid #d5dce5;}"
        "#reviewCardTitle{font-family:'Georgia';font-size:14pt;color:#344054;}"
        "#reviewPrimary{background:#1668d4;color:white;border:1px solid #1668d4;font-weight:600;}"
        "#reviewPrimary:hover{background:#0f5fc8;}"
        "#reviewImage{background:#f8fafc;border:1px solid #d8dee7;color:#667085;}"
    ));
}

void ReviewPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    refresh();
}

QWidget* ReviewPage::buildProjectTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("reviewProjectTab"));
    auto* outer = new QVBoxLayout(tab);
    outer->setContentsMargins(30, 22, 30, 30);
    outer->setSpacing(16);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    auto* hero = new QFrame;
    hero->setObjectName(QStringLiteral("reviewHero"));
    auto* heroLayout = new QVBoxLayout(hero);
    auto* kicker = new QLabel(tr("METADATOS / IDENTIDAD / PRESENTACIÓN"));
    kicker->setObjectName(QStringLiteral("reviewKicker"));
    auto* title = new QLabel(tr("Proyecto"));
    title->setObjectName(QStringLiteral("reviewTitle"));
    auto* desc = new QLabel(tr("Datos editoriales y visuales de la historia. La apariencia de la aplicación permanece clara y consistente."));
    desc->setObjectName(QStringLiteral("reviewDescription"));
    desc->setWordWrap(true);
    heroLayout->addWidget(kicker); heroLayout->addWidget(title); heroLayout->addWidget(desc);
    root->addWidget(hero);

    archiveTitle_ = new QLineEdit; storyTitle_ = new QLineEdit; subtitle_ = new QLineEdit; projectLabel_ = new QLineEdit;
    homeHeading_ = new QLineEdit; location_ = new QLineEdit; author_ = new QLineEdit; genre_ = new QLineEdit; projectStatus_ = new QLineEdit;
    synopsis_ = new QTextEdit; synopsis_->setMaximumHeight(150);
    chapterLabels_ = new QLineEdit; chapterLabels_->setPlaceholderText(tr("P, 1, 2, 3…"));
    theme_ = new QComboBox;
    theme_->addItem(tr("Interfaz clara"), QStringLiteral("light"));

    auto* identityGrid = new QGridLayout;
    identityGrid->setHorizontalSpacing(12); identityGrid->setVerticalSpacing(10);
    identityGrid->addWidget(labeled(tr("Título del archivo"), archiveTitle_), 0, 0);
    identityGrid->addWidget(labeled(tr("Título de la historia"), storyTitle_), 0, 1);
    identityGrid->addWidget(labeled(tr("Subtítulo"), subtitle_), 0, 2);
    identityGrid->addWidget(labeled(tr("Autor"), author_), 1, 0);
    identityGrid->addWidget(labeled(tr("Género"), genre_), 1, 1);
    identityGrid->addWidget(labeled(tr("Estado"), projectStatus_), 1, 2);
    identityGrid->addWidget(labeled(tr("Etiqueta de proyecto"), projectLabel_), 2, 0);
    identityGrid->addWidget(labeled(tr("Encabezado de inicio"), homeHeading_), 2, 1);
    identityGrid->addWidget(labeled(tr("Localización"), location_), 2, 2);
    identityGrid->addWidget(labeled(tr("Etiquetas de capítulos"), chapterLabels_), 3, 0, 1, 3);
    identityGrid->addWidget(labeled(tr("Sinopsis"), synopsis_), 4, 0, 1, 3);
    root->addWidget(card(tr("Identidad editorial"), identityGrid));

    auto* visualGrid = new QGridLayout;
    visualGrid->setHorizontalSpacing(12);
    auto setupImage = [this, visualGrid](int column, const QString& labelText, const QString& key, QLabel*& preview) {
        auto* cell = new QWidget;
        auto* v = new QVBoxLayout(cell);
        v->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel(labelText); label->setObjectName(QStringLiteral("reviewFieldTitle"));
        preview = new QLabel(tr("Sin imagen")); preview->setObjectName(QStringLiteral("reviewImage"));
        preview->setMinimumSize(220, 125); preview->setAlignment(Qt::AlignCenter);
        auto* actions = new QHBoxLayout;
        auto* choose = makeButton(tr("Elegir…")); auto* clear = makeButton(tr("Quitar"));
        actions->addWidget(choose); actions->addWidget(clear); actions->addStretch();
        v->addWidget(label); v->addWidget(preview); v->addLayout(actions);
        visualGrid->addWidget(cell, 0, column);
        connect(choose, &QPushButton::clicked, this, [this, key]() { chooseProfileImage(key); });
        connect(clear, &QPushButton::clicked, this, [this, key]() { clearProfileImage(key); });
    };
    setupImage(0, tr("Portada"), QStringLiteral("coverImageDataUrl"), coverPreview_);
    setupImage(1, tr("Banner"), QStringLiteral("bannerImageDataUrl"), bannerPreview_);
    setupImage(2, tr("Icono de tablero"), QStringLiteral("boardIconDataUrl"), iconPreview_);
    root->addWidget(card(tr("Recursos visuales"), visualGrid));
    root->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    for (QLineEdit* field : {archiveTitle_, storyTitle_, subtitle_, projectLabel_, homeHeading_, location_, author_, genre_, projectStatus_, chapterLabels_})
        connect(field, &QLineEdit::editingFinished, this, &ReviewPage::applyProfile);
    connect(synopsis_, &QTextEdit::textChanged, this, &ReviewPage::applyProfile);
    return tab;
}

QWidget* ReviewPage::buildAnalysisTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("reviewAnalysisTab"));
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(30, 22, 30, 30);
    layout->setSpacing(14);
    auto* hero = new QFrame; hero->setObjectName(QStringLiteral("reviewHero"));
    auto* hv = new QVBoxLayout(hero);
    auto* title = new QLabel(tr("Revisión local")); title->setObjectName(QStringLiteral("reviewTitle"));
    auto* desc = new QLabel(tr("Análisis mecánico del manuscrito: repeticiones cercanas, muletillas y duplicaciones.")); desc->setObjectName(QStringLiteral("reviewDescription"));
    hv->addWidget(title); hv->addWidget(desc); layout->addWidget(hero);

    auto* settingsGrid = new QGridLayout;
    repetitionWindow_ = new QSpinBox; repetitionWindow_->setRange(5, 200); repetitionWindow_->setValue(40);
    fillerPhrases_ = new QTextEdit; fillerPhrases_->setMaximumHeight(105); fillerPhrases_->setPlaceholderText(tr("Una muletilla por línea"));
    auto* analyze = makeButton(tr("Analizar manuscrito")); analyze->setObjectName(QStringLiteral("reviewPrimary"));
    settingsGrid->addWidget(labeled(tr("Ventana de repetición"), repetitionWindow_), 0, 0);
    settingsGrid->addWidget(labeled(tr("Muletillas"), fillerPhrases_), 0, 1);
    settingsGrid->addWidget(analyze, 0, 2, Qt::AlignBottom);
    layout->addWidget(card(tr("Configuración"), settingsGrid));
    analysisSummary_ = new QLabel(tr("Ejecuta el análisis para revisar el manuscrito.")); analysisSummary_->setWordWrap(true);
    analysisResults_ = new QListWidget;
    auto* resultLayout = new QVBoxLayout; resultLayout->addWidget(analysisSummary_); resultLayout->addWidget(analysisResults_, 1);
    layout->addWidget(card(tr("Resultados"), resultLayout), 1);
    connect(repetitionWindow_, &QSpinBox::valueChanged, this, &ReviewPage::applyAnalysisSettings);
    connect(fillerPhrases_, &QTextEdit::textChanged, this, &ReviewPage::applyAnalysisSettings);
    connect(analyze, &QPushButton::clicked, this, &ReviewPage::runAnalysis);
    return tab;
}

QWidget* ReviewPage::buildLayoutTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("reviewLayoutTab"));
    auto* outer = new QVBoxLayout(tab);
    outer->setContentsMargins(30, 22, 30, 30);
    outer->setSpacing(14);

    auto* hero = new QFrame; hero->setObjectName(QStringLiteral("reviewHero"));
    auto* heroRow = new QHBoxLayout(hero);
    auto* copy = new QVBoxLayout;
    auto* kicker = new QLabel(tr("FORMATO DE PÁGINA / TIPOGRAFÍA / ESTRUCTURA")); kicker->setObjectName(QStringLiteral("reviewKicker"));
    auto* title = new QLabel(tr("Maquetación y salida")); title->setObjectName(QStringLiteral("reviewTitle"));
    auto* desc = new QLabel(tr("Configura el manuscrito final y exporta desde una superficie compacta, no desde un formulario vertical.")); desc->setObjectName(QStringLiteral("reviewDescription")); desc->setWordWrap(true);
    copy->addWidget(kicker); copy->addWidget(title); copy->addWidget(desc);
    heroRow->addLayout(copy, 1);
    auto* exports = new QHBoxLayout;
    auto* pdf = makeButton(tr("PDF")); pdf->setObjectName(QStringLiteral("reviewPrimary"));
    auto* wbw = makeButton(tr("Proyecto .wbw"));
    auto* backup = makeButton(tr("Copia de seguridad"));
    exports->addWidget(pdf); exports->addWidget(wbw); exports->addWidget(backup);
    heroRow->addLayout(exports);
    outer->addWidget(hero);

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* root = new QVBoxLayout(content); root->setContentsMargins(0,0,0,0); root->setSpacing(14);

    layoutPreset_ = new QComboBox;
    layoutPreset_->addItem(tr("Editorial"), QStringLiteral("editorial")); layoutPreset_->addItem(tr("Novela"), QStringLiteral("novela"));
    layoutPreset_->addItem(tr("Bolsillo"), QStringLiteral("bolsillo")); layoutPreset_->addItem(tr("Fantasía"), QStringLiteral("fantasia"));
    layoutPreset_->addItem(tr("Crónica"), QStringLiteral("cronica")); layoutPreset_->addItem(tr("Personalizada"), QStringLiteral("personalizada"));
    auto number = [](double min, double max, double step) { auto* spin = new QDoubleSpinBox; spin->setRange(min,max); spin->setDecimals(2); spin->setSingleStep(step); return spin; };
    pageWidth_ = number(80,400,1); pageHeight_ = number(100,500,1); marginTop_ = number(0,80,1); marginRight_ = number(0,80,1);
    marginBottom_ = number(0,80,1); marginLeft_ = number(0,80,1); fontSize_ = number(6,30,.5); lineHeight_ = number(.8,3,.05); paragraphIndent_ = number(0,30,.5);
    fontFamily_ = new QComboBox; fontFamily_->addItems({QStringLiteral("Garamond"), QStringLiteral("Georgia"), QStringLiteral("Literata"), QStringLiteral("Bookerly"), QStringLiteral("Atkinson")});
    chapterOpening_ = new QComboBox; chapterOpening_->addItems({tr("Página nueva"), tr("Página impar"), tr("Continuo")});
    sceneSeparator_ = new QLineEdit; headerText_ = new QLineEdit; footerText_ = new QLineEdit;

    auto* pageGrid = new QGridLayout; pageGrid->setHorizontalSpacing(12); pageGrid->setVerticalSpacing(10);
    pageGrid->addWidget(labeled(tr("Preajuste"), layoutPreset_),0,0,1,2); pageGrid->addWidget(labeled(tr("Ancho (mm)"), pageWidth_),1,0); pageGrid->addWidget(labeled(tr("Alto (mm)"), pageHeight_),1,1);
    root->addWidget(card(tr("Página"), pageGrid));

    auto* marginGrid = new QGridLayout; marginGrid->setHorizontalSpacing(12);
    marginGrid->addWidget(labeled(tr("Superior"), marginTop_),0,0); marginGrid->addWidget(labeled(tr("Derecho"), marginRight_),0,1);
    marginGrid->addWidget(labeled(tr("Inferior"), marginBottom_),1,0); marginGrid->addWidget(labeled(tr("Izquierdo"), marginLeft_),1,1);
    root->addWidget(card(tr("Márgenes (mm)"), marginGrid));

    auto* typeGrid = new QGridLayout; typeGrid->setHorizontalSpacing(12); typeGrid->setVerticalSpacing(10);
    typeGrid->addWidget(labeled(tr("Tipografía"), fontFamily_),0,0); typeGrid->addWidget(labeled(tr("Tamaño (pt)"), fontSize_),0,1);
    typeGrid->addWidget(labeled(tr("Interlineado"), lineHeight_),1,0); typeGrid->addWidget(labeled(tr("Sangría (mm)"), paragraphIndent_),1,1);
    root->addWidget(card(tr("Tipografía"), typeGrid));

    auto* structureGrid = new QGridLayout; structureGrid->setHorizontalSpacing(12); structureGrid->setVerticalSpacing(10);
    structureGrid->addWidget(labeled(tr("Inicio de capítulo"), chapterOpening_),0,0); structureGrid->addWidget(labeled(tr("Separador de escena"), sceneSeparator_),0,1);
    structureGrid->addWidget(labeled(tr("Encabezado"), headerText_),1,0); structureGrid->addWidget(labeled(tr("Pie"), footerText_),1,1);
    root->addWidget(card(tr("Estructura"), structureGrid));
    root->addStretch(); scroll->setWidget(content); outer->addWidget(scroll,1);

    connect(layoutPreset_, &QComboBox::currentIndexChanged, this, [this]() { if (!refreshing_) applyLayoutPreset(layoutPreset_->currentData().toString()); });
    for (QDoubleSpinBox* field : {pageWidth_,pageHeight_,marginTop_,marginRight_,marginBottom_,marginLeft_,fontSize_,lineHeight_,paragraphIndent_}) connect(field,&QDoubleSpinBox::valueChanged,this,&ReviewPage::applyLayout);
    connect(fontFamily_,&QComboBox::currentTextChanged,this,&ReviewPage::applyLayout); connect(chapterOpening_,&QComboBox::currentTextChanged,this,&ReviewPage::applyLayout);
    connect(sceneSeparator_,&QLineEdit::editingFinished,this,&ReviewPage::applyLayout); connect(headerText_,&QLineEdit::editingFinished,this,&ReviewPage::applyLayout); connect(footerText_,&QLineEdit::editingFinished,this,&ReviewPage::applyLayout);
    connect(pdf,&QPushButton::clicked,this,&ReviewPage::requestExportPdf); connect(wbw,&QPushButton::clicked,this,&ReviewPage::requestExportWbw); connect(backup,&QPushButton::clicked,this,&ReviewPage::requestBackup);
    return tab;
}

QWidget* ReviewPage::buildMediaTab() {
    auto* tab = new QWidget; tab->setObjectName(QStringLiteral("reviewMediaTab"));
    auto* root = new QVBoxLayout(tab); root->setContentsMargins(30,22,30,30); root->setSpacing(14);
    auto* hero = new QFrame; hero->setObjectName(QStringLiteral("reviewHero")); auto* hv = new QVBoxLayout(hero);
    auto* title = new QLabel(tr("Ambientación")); title->setObjectName(QStringLiteral("reviewTitle"));
    auto* desc = new QLabel(tr("Enlaces de referencia y archivos locales de audio o vídeo para acompañar la escritura.")); desc->setObjectName(QStringLiteral("reviewDescription"));
    hv->addWidget(title); hv->addWidget(desc); root->addWidget(hero);
    spotifyUrl_ = new QLineEdit; youtubeUrl_ = new QLineEdit;
    auto* linkGrid = new QGridLayout; auto* openSpotify = makeButton(tr("Abrir")); auto* openYoutube = makeButton(tr("Abrir"));
    linkGrid->addWidget(labeled(tr("Spotify"),spotifyUrl_),0,0); linkGrid->addWidget(openSpotify,0,1,Qt::AlignBottom);
    linkGrid->addWidget(labeled(tr("YouTube / ambiente"),youtubeUrl_),1,0); linkGrid->addWidget(openYoutube,1,1,Qt::AlignBottom);
    root->addWidget(card(tr("Enlaces"),linkGrid));
    auto* mediaGrid = new QGridLayout; mediaList_ = new QListWidget; videoWidget_ = new QVideoWidget; videoWidget_->setMinimumSize(420,240); videoWidget_->setStyleSheet(QStringLiteral("background:#111;"));
    mediaGrid->addWidget(mediaList_,0,0); mediaGrid->addWidget(videoWidget_,0,1); mediaGrid->setColumnStretch(0,1); mediaGrid->setColumnStretch(1,1);
    auto* actions = new QHBoxLayout; auto* add=makeButton(tr("Añadir…")); auto* remove=makeButton(tr("Quitar")); auto* play=makeButton(tr("Reproducir")); auto* stop=makeButton(tr("Detener"));
    actions->addWidget(add); actions->addWidget(remove); actions->addWidget(play); actions->addWidget(stop); actions->addStretch();
    mediaGrid->addLayout(actions,1,0,1,2); root->addWidget(card(tr("Archivos locales"),mediaGrid),1);
    player_=new QMediaPlayer(this); audioOutput_=new QAudioOutput(this); player_->setAudioOutput(audioOutput_); player_->setVideoOutput(videoWidget_); audioOutput_->setVolume(.65f);
    connect(spotifyUrl_,&QLineEdit::editingFinished,this,&ReviewPage::applyMediaUrls); connect(youtubeUrl_,&QLineEdit::editingFinished,this,&ReviewPage::applyMediaUrls);
    connect(openSpotify,&QPushButton::clicked,this,[this](){openExternalUrl(spotifyUrl_->text());}); connect(openYoutube,&QPushButton::clicked,this,[this](){openExternalUrl(youtubeUrl_->text());});
    connect(add,&QPushButton::clicked,this,&ReviewPage::addLocalMedia); connect(remove,&QPushButton::clicked,this,&ReviewPage::removeLocalMedia); connect(play,&QPushButton::clicked,this,&ReviewPage::playSelectedMedia); connect(stop,&QPushButton::clicked,this,&ReviewPage::stopMedia);
    connect(mediaList_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem*){playSelectedMedia();});
    return tab;
}

void ReviewPage::refresh() {
    if (!document_) return;
    refreshing_=true;
    const QJsonObject profile=document_->object(QStringLiteral("profile"));
    archiveTitle_->setText(profile.value(QStringLiteral("archiveTitle")).toString()); storyTitle_->setText(profile.value(QStringLiteral("storyTitle")).toString()); subtitle_->setText(profile.value(QStringLiteral("subtitle")).toString());
    projectLabel_->setText(profile.value(QStringLiteral("projectLabel")).toString()); homeHeading_->setText(profile.value(QStringLiteral("homeHeading")).toString()); location_->setText(profile.value(QStringLiteral("location")).toString());
    author_->setText(profile.value(QStringLiteral("author")).toString()); genre_->setText(profile.value(QStringLiteral("genre")).toString()); projectStatus_->setText(profile.value(QStringLiteral("status")).toString());
    synopsis_->setPlainText(profile.value(QStringLiteral("synopsis")).toString()); chapterLabels_->setText(jsonStrings(profile.value(QStringLiteral("chapterLabels")).toArray())); theme_->setCurrentIndex(0);
    setPreview(coverPreview_,profile.value(QStringLiteral("coverImageDataUrl")).toString(),tr("Sin portada")); setPreview(bannerPreview_,profile.value(QStringLiteral("bannerImageDataUrl")).toString(),tr("Sin banner")); setPreview(iconPreview_,profile.value(QStringLiteral("boardIconDataUrl")).toString(),tr("Sin icono"));
    const QJsonObject analysis=profile.value(QStringLiteral("writingAnalysis")).toObject(); repetitionWindow_->setValue(analysis.value(QStringLiteral("repetitionWindow")).toInt(40));
    QStringList filler; for(const QJsonValue value:analysis.value(QStringLiteral("fillerPhrases")).toArray()) filler.append(value.toString()); if(filler.isEmpty()) filler={QStringLiteral("de repente"),QStringLiteral("entonces"),QStringLiteral("bueno"),QStringLiteral("en realidad"),QStringLiteral("de alguna manera"),QStringLiteral("era"),QStringLiteral("estaba"),QStringLiteral("había")}; fillerPhrases_->setPlainText(filler.join(QLatin1Char('\n')));
    QJsonObject layout=profile.value(QStringLiteral("manuscriptLayout")).toObject(); if(layout.isEmpty()) layout=defaultLayoutFor(QStringLiteral("editorial"));
    int presetIndex=layoutPreset_->findData(layout.value(QStringLiteral("preset")).toString(QStringLiteral("editorial"))); layoutPreset_->setCurrentIndex(presetIndex>=0?presetIndex:0);
    pageWidth_->setValue(layout.value(QStringLiteral("pageWidthMm")).toDouble(152.4)); pageHeight_->setValue(layout.value(QStringLiteral("pageHeightMm")).toDouble(228.6)); marginTop_->setValue(layout.value(QStringLiteral("marginTopMm")).toDouble(20)); marginRight_->setValue(layout.value(QStringLiteral("marginRightMm")).toDouble(19)); marginBottom_->setValue(layout.value(QStringLiteral("marginBottomMm")).toDouble(22)); marginLeft_->setValue(layout.value(QStringLiteral("marginLeftMm")).toDouble(19));
    int fontIndex=fontFamily_->findText(layout.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Garamond"))); fontFamily_->setCurrentIndex(fontIndex>=0?fontIndex:0); fontSize_->setValue(layout.value(QStringLiteral("fontSizePt")).toDouble(11)); lineHeight_->setValue(layout.value(QStringLiteral("lineHeight")).toDouble(1.35)); paragraphIndent_->setValue(layout.value(QStringLiteral("paragraphIndentMm")).toDouble(5));
    int openingIndex=chapterOpening_->findText(layout.value(QStringLiteral("chapterOpening")).toString(QStringLiteral("Página nueva"))); chapterOpening_->setCurrentIndex(openingIndex>=0?openingIndex:0); sceneSeparator_->setText(layout.value(QStringLiteral("sceneSeparator")).toString(QStringLiteral("⁂"))); headerText_->setText(layout.value(QStringLiteral("headerText")).toString(QStringLiteral("{título}"))); footerText_->setText(layout.value(QStringLiteral("footerText")).toString(QStringLiteral("{página}")));
    spotifyUrl_->setText(profile.value(QStringLiteral("spotifyPlaylistUrl")).toString()); youtubeUrl_->setText(profile.value(QStringLiteral("youtubeAmbientUrl")).toString()); mediaList_->clear();
    for(const QJsonValue value:document_->array(QStringLiteral("mediaTracks"))){const QJsonObject track=value.toObject(); auto* item=new QListWidgetItem(track.value(QStringLiteral("name")).toString(QFileInfo(track.value(QStringLiteral("path")).toString()).fileName())); item->setData(Qt::UserRole,track.value(QStringLiteral("id")).toString()); item->setToolTip(track.value(QStringLiteral("path")).toString()); mediaList_->addItem(item);} refreshing_=false;
}

void ReviewPage::applyProfile(){if(refreshing_||!document_)return; QJsonObject p=document_->object(QStringLiteral("profile")); p.insert(QStringLiteral("archiveTitle"),archiveTitle_->text());p.insert(QStringLiteral("storyTitle"),storyTitle_->text());p.insert(QStringLiteral("subtitle"),subtitle_->text());p.insert(QStringLiteral("projectLabel"),projectLabel_->text());p.insert(QStringLiteral("homeHeading"),homeHeading_->text());p.insert(QStringLiteral("location"),location_->text());p.insert(QStringLiteral("author"),author_->text());p.insert(QStringLiteral("genre"),genre_->text());p.insert(QStringLiteral("status"),projectStatus_->text());p.insert(QStringLiteral("synopsis"),synopsis_->toPlainText());p.insert(QStringLiteral("chapterLabels"),stringsJson(chapterLabels_->text()));p.insert(QStringLiteral("theme"),QStringLiteral("light"));document_->setObject(QStringLiteral("profile"),p);emit changed();}

void ReviewPage::chooseProfileImage(const QString& key){if(!document_)return;const QString path=QFileDialog::getOpenFileName(this,tr("Elegir imagen"),QString(),tr("Imágenes (*.png *.jpg *.jpeg *.webp *.gif)"));if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::ReadOnly))return;const QByteArray bytes=file.readAll();if(bytes.size()>20LL*1024LL*1024LL){QMessageBox::warning(this,tr("Imagen demasiado grande"),tr("El límite para una imagen de proyecto es 20 MB."));return;}QString mime=QMimeDatabase().mimeTypeForFile(path,QMimeDatabase::MatchContent).name();if(mime.isEmpty())mime=QStringLiteral("application/octet-stream");QJsonObject p=document_->object(QStringLiteral("profile"));p.insert(key,QStringLiteral("data:%1;base64,%2").arg(mime,QString::fromLatin1(bytes.toBase64())));document_->setObject(QStringLiteral("profile"),p);refresh();emit changed();}
void ReviewPage::clearProfileImage(const QString& key){if(!document_)return;QJsonObject p=document_->object(QStringLiteral("profile"));p.remove(key);document_->setObject(QStringLiteral("profile"),p);refresh();emit changed();}
void ReviewPage::applyAnalysisSettings(){if(refreshing_||!document_)return;QJsonObject p=document_->object(QStringLiteral("profile"));QJsonObject s;s.insert(QStringLiteral("repetitionWindow"),repetitionWindow_->value());QJsonArray f;for(QString line:fillerPhrases_->toPlainText().split(QLatin1Char('\n'),Qt::SkipEmptyParts)){line=line.trimmed();if(!line.isEmpty())f.append(line);}s.insert(QStringLiteral("fillerPhrases"),f);p.insert(QStringLiteral("writingAnalysis"),s);document_->setObject(QStringLiteral("profile"),p);emit changed();}

void ReviewPage::runAnalysis(){analysisResults_->clear();if(!document_)return;const QString text=allManuscriptText(*document_);const QStringList words=text.simplified().split(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}'’-]+")),Qt::SkipEmptyParts);int repetitions=0,fillers=0,mechanical=0;QHash<QString,int> lastSeen;for(int i=0;i<words.size();++i){const QString word=words.at(i).toCaseFolded();if(word.size()<4)continue;if(lastSeen.contains(word)&&i-lastSeen.value(word)<=repetitionWindow_->value()){++repetitions;if(analysisResults_->count()<250)analysisResults_->addItem(tr("Repetición cercana: “%1” (%2 palabras de distancia)").arg(words.at(i)).arg(i-lastSeen.value(word)));}lastSeen.insert(word,i);}for(QString phrase:fillerPhrases_->toPlainText().split(QLatin1Char('\n'),Qt::SkipEmptyParts)){phrase=phrase.trimmed();if(phrase.isEmpty())continue;QRegularExpression e(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(phrase)),QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption);int count=0;auto it=e.globalMatch(text);while(it.hasNext()){it.next();++count;}if(count){fillers+=count;analysisResults_->addItem(tr("Muletilla: “%1” — %2 apariciones").arg(phrase).arg(count));}}QRegularExpression doubledWord(QStringLiteral("\\b([\\p{L}]{2,})\\s+\\1\\b"),QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption);auto doubled=doubledWord.globalMatch(text);while(doubled.hasNext()){auto m=doubled.next();++mechanical;analysisResults_->addItem(tr("Palabra duplicada consecutiva: “%1”").arg(m.captured(1)));}const int doubleSpaces=text.count(QRegularExpression(QStringLiteral(" {2,}")));if(doubleSpaces){mechanical+=doubleSpaces;analysisResults_->addItem(tr("Espacios dobles o múltiples: %1").arg(doubleSpaces));}analysisSummary_->setText(tr("%1 palabras · %2 repeticiones cercanas · %3 muletillas · %4 incidencias mecánicas").arg(words.size()).arg(repetitions).arg(fillers).arg(mechanical));if(!analysisResults_->count())analysisResults_->addItem(tr("No se encontraron incidencias con la configuración actual."));}

void ReviewPage::applyLayoutPreset(const QString& preset){if(!document_)return;if(preset==QStringLiteral("personalizada")){applyLayout();return;}const QJsonObject l=defaultLayoutFor(preset);refreshing_=true;pageWidth_->setValue(l.value(QStringLiteral("pageWidthMm")).toDouble());pageHeight_->setValue(l.value(QStringLiteral("pageHeightMm")).toDouble());marginTop_->setValue(l.value(QStringLiteral("marginTopMm")).toDouble());marginRight_->setValue(l.value(QStringLiteral("marginRightMm")).toDouble());marginBottom_->setValue(l.value(QStringLiteral("marginBottomMm")).toDouble());marginLeft_->setValue(l.value(QStringLiteral("marginLeftMm")).toDouble());fontFamily_->setCurrentText(l.value(QStringLiteral("fontFamily")).toString());fontSize_->setValue(l.value(QStringLiteral("fontSizePt")).toDouble());lineHeight_->setValue(l.value(QStringLiteral("lineHeight")).toDouble());paragraphIndent_->setValue(l.value(QStringLiteral("paragraphIndentMm")).toDouble());chapterOpening_->setCurrentText(l.value(QStringLiteral("chapterOpening")).toString());sceneSeparator_->setText(l.value(QStringLiteral("sceneSeparator")).toString());headerText_->setText(l.value(QStringLiteral("headerText")).toString());footerText_->setText(l.value(QStringLiteral("footerText")).toString());refreshing_=false;applyLayout();}
void ReviewPage::applyLayout(){if(refreshing_||!document_)return;QJsonObject l;l.insert(QStringLiteral("preset"),layoutPreset_->currentData().toString());l.insert(QStringLiteral("pageWidthMm"),pageWidth_->value());l.insert(QStringLiteral("pageHeightMm"),pageHeight_->value());l.insert(QStringLiteral("marginTopMm"),marginTop_->value());l.insert(QStringLiteral("marginRightMm"),marginRight_->value());l.insert(QStringLiteral("marginBottomMm"),marginBottom_->value());l.insert(QStringLiteral("marginLeftMm"),marginLeft_->value());l.insert(QStringLiteral("fontFamily"),fontFamily_->currentText());l.insert(QStringLiteral("fontSizePt"),fontSize_->value());l.insert(QStringLiteral("lineHeight"),lineHeight_->value());l.insert(QStringLiteral("paragraphIndentMm"),paragraphIndent_->value());l.insert(QStringLiteral("chapterOpening"),chapterOpening_->currentText());l.insert(QStringLiteral("sceneSeparator"),sceneSeparator_->text());l.insert(QStringLiteral("headerText"),headerText_->text());l.insert(QStringLiteral("footerText"),footerText_->text());QJsonObject p=document_->object(QStringLiteral("profile"));p.insert(QStringLiteral("manuscriptLayout"),l);document_->setObject(QStringLiteral("profile"),p);emit changed();}
void ReviewPage::applyMediaUrls(){if(refreshing_||!document_)return;QJsonObject p=document_->object(QStringLiteral("profile"));p.insert(QStringLiteral("spotifyPlaylistUrl"),spotifyUrl_->text().trimmed());p.insert(QStringLiteral("youtubeAmbientUrl"),youtubeUrl_->text().trimmed());document_->setObject(QStringLiteral("profile"),p);emit changed();}
void ReviewPage::addLocalMedia(){if(!document_)return;const QStringList paths=QFileDialog::getOpenFileNames(this,tr("Añadir audio o vídeo"),QString(),tr("Multimedia (*.mp3 *.ogg *.wav *.m4a *.mp4 *.mkv *.webm *.avi *.mov *.vob);;Todos los archivos (*)"));if(paths.isEmpty())return;QJsonArray media=document_->array(QStringLiteral("mediaTracks"));for(const QString& path:paths)media.append(QJsonObject{{QStringLiteral("id"),uid(QStringLiteral("media"))},{QStringLiteral("name"),QFileInfo(path).fileName()},{QStringLiteral("path"),QFileInfo(path).absoluteFilePath()}});document_->setArray(QStringLiteral("mediaTracks"),media);refresh();emit changed();}
void ReviewPage::removeLocalMedia(){if(!document_)return;const int row=mediaList_->currentRow();QJsonArray media=document_->array(QStringLiteral("mediaTracks"));if(row<0||row>=media.size())return;media.removeAt(row);document_->setArray(QStringLiteral("mediaTracks"),media);stopMedia();refresh();emit changed();}
void ReviewPage::playSelectedMedia(){if(!document_)return;const int row=mediaList_->currentRow();const QJsonArray media=document_->array(QStringLiteral("mediaTracks"));if(row<0||row>=media.size())return;const QString path=media.at(row).toObject().value(QStringLiteral("path")).toString();if(!QFileInfo::exists(path)){QMessageBox::warning(this,tr("Archivo no encontrado"),tr("El archivo local ya no existe en esta ruta:\n%1").arg(path));return;}player_->setSource(QUrl::fromLocalFile(path));player_->play();}
void ReviewPage::stopMedia(){player_->stop();}
void ReviewPage::openExternalUrl(const QString& url){const QUrl target=QUrl::fromUserInput(url.trimmed());if(!target.isValid()||target.isEmpty())return;QDesktopServices::openUrl(target);}

} // namespace wbw
