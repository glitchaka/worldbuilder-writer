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

QWidget* scrollForm(QFormLayout*& form) {
    auto* content = new QWidget;
    form = new QFormLayout(content);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
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
        {QStringLiteral("preset"), preset},
        {QStringLiteral("pageWidthMm"), 152.4},
        {QStringLiteral("pageHeightMm"), 228.6},
        {QStringLiteral("marginTopMm"), 20.0},
        {QStringLiteral("marginRightMm"), 19.0},
        {QStringLiteral("marginBottomMm"), 22.0},
        {QStringLiteral("marginLeftMm"), 19.0},
        {QStringLiteral("fontFamily"), QStringLiteral("Garamond")},
        {QStringLiteral("fontSizePt"), 11.0},
        {QStringLiteral("lineHeight"), 1.35},
        {QStringLiteral("paragraphIndentMm"), 5.0},
        {QStringLiteral("chapterOpening"), QStringLiteral("Página nueva")},
        {QStringLiteral("sceneSeparator"), QStringLiteral("⁂")},
        {QStringLiteral("headerText"), QStringLiteral("{título}")},
        {QStringLiteral("footerText"), QStringLiteral("{página}")}
    };
    if (preset == QStringLiteral("novela")) {
        layout.insert(QStringLiteral("pageWidthMm"), 140.0);
        layout.insert(QStringLiteral("pageHeightMm"), 216.0);
        layout.insert(QStringLiteral("fontSizePt"), 11.0);
        layout.insert(QStringLiteral("lineHeight"), 1.3);
    } else if (preset == QStringLiteral("bolsillo")) {
        layout.insert(QStringLiteral("pageWidthMm"), 120.0);
        layout.insert(QStringLiteral("pageHeightMm"), 190.0);
        layout.insert(QStringLiteral("marginTopMm"), 16.0);
        layout.insert(QStringLiteral("marginRightMm"), 15.0);
        layout.insert(QStringLiteral("marginBottomMm"), 18.0);
        layout.insert(QStringLiteral("marginLeftMm"), 15.0);
        layout.insert(QStringLiteral("fontSizePt"), 9.5);
        layout.insert(QStringLiteral("lineHeight"), 1.2);
    } else if (preset == QStringLiteral("fantasia")) {
        layout.insert(QStringLiteral("pageWidthMm"), 152.4);
        layout.insert(QStringLiteral("pageHeightMm"), 228.6);
        layout.insert(QStringLiteral("fontFamily"), QStringLiteral("Georgia"));
        layout.insert(QStringLiteral("fontSizePt"), 11.0);
        layout.insert(QStringLiteral("lineHeight"), 1.4);
        layout.insert(QStringLiteral("chapterOpening"), QStringLiteral("Página impar"));
    } else if (preset == QStringLiteral("cronica")) {
        layout.insert(QStringLiteral("pageWidthMm"), 170.0);
        layout.insert(QStringLiteral("pageHeightMm"), 240.0);
        layout.insert(QStringLiteral("fontFamily"), QStringLiteral("Atkinson"));
        layout.insert(QStringLiteral("fontSizePt"), 10.5);
        layout.insert(QStringLiteral("lineHeight"), 1.3);
        layout.insert(QStringLiteral("paragraphIndentMm"), 0.0);
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
        const QJsonObject chapter = chapterValue.toObject();
        for (const QJsonValue sceneValue : chapter.value(QStringLiteral("scenes")).toArray()) parts.append(scenePlainText(sceneValue.toObject()));
    }
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace

ReviewPage::ReviewPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->addTab(buildProjectTab(), tr("Proyecto"));
    tabs_->addTab(buildAnalysisTab(), tr("Revisión"));
    tabs_->addTab(buildLayoutTab(), tr("Maquetación y salida"));
    tabs_->addTab(buildMediaTab(), tr("Ambientación"));
    root->addWidget(tabs_);
}

void ReviewPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    refresh();
}

QWidget* ReviewPage::buildProjectTab() {
    QFormLayout* form = nullptr;
    QWidget* tab = scrollForm(form);
    archiveTitle_ = new QLineEdit;
    storyTitle_ = new QLineEdit;
    subtitle_ = new QLineEdit;
    projectLabel_ = new QLineEdit;
    homeHeading_ = new QLineEdit;
    location_ = new QLineEdit;
    author_ = new QLineEdit;
    genre_ = new QLineEdit;
    projectStatus_ = new QLineEdit;
    synopsis_ = new QTextEdit;
    synopsis_->setMaximumHeight(160);
    chapterLabels_ = new QLineEdit;
    chapterLabels_->setPlaceholderText(tr("P, 1, 2, 3…"));
    theme_ = new QComboBox;
    theme_->addItem(tr("Oscuro"), QStringLiteral("grim"));
    theme_->addItem(tr("Crónica"), QStringLiteral("chronicle"));
    theme_->addItem(tr("Escritorio"), QStringLiteral("desk"));
    theme_->addItem(tr("Clásico"), QStringLiteral("classic"));
    theme_->addItem(tr("Claro"), QStringLiteral("kawaii"));
    form->addRow(tr("Título del archivo"), archiveTitle_);
    form->addRow(tr("Título de la historia"), storyTitle_);
    form->addRow(tr("Subtítulo"), subtitle_);
    form->addRow(tr("Etiqueta de proyecto"), projectLabel_);
    form->addRow(tr("Encabezado de inicio"), homeHeading_);
    form->addRow(tr("Localización"), location_);
    form->addRow(tr("Autor"), author_);
    form->addRow(tr("Género"), genre_);
    form->addRow(tr("Estado"), projectStatus_);
    form->addRow(tr("Sinopsis"), synopsis_);
    form->addRow(tr("Etiquetas de capítulos"), chapterLabels_);
    form->addRow(tr("Tema"), theme_);

    auto imageRow = [this](QFormLayout* target, const QString& labelText, const QString& key, QLabel*& preview) {
        preview = new QLabel(tr("Sin imagen"));
        preview->setMinimumSize(240, 120);
        preview->setMaximumHeight(180);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet(QStringLiteral("border:1px solid #393a32;border-radius:4px;"));
        auto* choose = makeButton(tr("Elegir…"));
        auto* clear = makeButton(tr("Quitar"));
        auto* row = new QHBoxLayout;
        row->addWidget(preview, 1);
        auto* actions = new QVBoxLayout;
        actions->addWidget(choose);
        actions->addWidget(clear);
        actions->addStretch();
        row->addLayout(actions);
        target->addRow(labelText, row);
        connect(choose, &QPushButton::clicked, this, [this, key]() { chooseProfileImage(key); });
        connect(clear, &QPushButton::clicked, this, [this, key]() { clearProfileImage(key); });
    };
    imageRow(form, tr("Portada"), QStringLiteral("coverImageDataUrl"), coverPreview_);
    imageRow(form, tr("Banner"), QStringLiteral("bannerImageDataUrl"), bannerPreview_);
    imageRow(form, tr("Icono de tablero"), QStringLiteral("boardIconDataUrl"), iconPreview_);

    for (QLineEdit* field : {archiveTitle_, storyTitle_, subtitle_, projectLabel_, homeHeading_, location_, author_, genre_, projectStatus_, chapterLabels_})
        connect(field, &QLineEdit::editingFinished, this, &ReviewPage::applyProfile);
    connect(synopsis_, &QTextEdit::textChanged, this, &ReviewPage::applyProfile);
    connect(theme_, &QComboBox::currentIndexChanged, this, &ReviewPage::applyProfile);
    return tab;
}

QWidget* ReviewPage::buildAnalysisTab() {
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    auto* settings = new QHBoxLayout;
    repetitionWindow_ = new QSpinBox;
    repetitionWindow_->setRange(5, 200);
    repetitionWindow_->setValue(40);
    fillerPhrases_ = new QTextEdit;
    fillerPhrases_->setMaximumHeight(90);
    fillerPhrases_->setPlaceholderText(tr("Una muletilla por línea"));
    auto* analyze = makeButton(tr("Analizar manuscrito"));
    settings->addWidget(new QLabel(tr("Ventana de repetición")));
    settings->addWidget(repetitionWindow_);
    settings->addWidget(new QLabel(tr("Muletillas")));
    settings->addWidget(fillerPhrases_, 1);
    settings->addWidget(analyze, 0, Qt::AlignBottom);
    layout->addLayout(settings);
    analysisSummary_ = new QLabel(tr("Ejecuta el análisis para revisar el manuscrito."));
    analysisSummary_->setWordWrap(true);
    layout->addWidget(analysisSummary_);
    analysisResults_ = new QListWidget;
    layout->addWidget(analysisResults_, 1);
    connect(repetitionWindow_, &QSpinBox::valueChanged, this, &ReviewPage::applyAnalysisSettings);
    connect(fillerPhrases_, &QTextEdit::textChanged, this, &ReviewPage::applyAnalysisSettings);
    connect(analyze, &QPushButton::clicked, this, &ReviewPage::runAnalysis);
    return tab;
}

QWidget* ReviewPage::buildLayoutTab() {
    auto* tab = new QWidget;
    auto* root = new QVBoxLayout(tab);
    QFormLayout* form = nullptr;
    QWidget* scroll = scrollForm(form);
    layoutPreset_ = new QComboBox;
    layoutPreset_->addItem(tr("Editorial"), QStringLiteral("editorial"));
    layoutPreset_->addItem(tr("Novela"), QStringLiteral("novela"));
    layoutPreset_->addItem(tr("Bolsillo"), QStringLiteral("bolsillo"));
    layoutPreset_->addItem(tr("Fantasía"), QStringLiteral("fantasia"));
    layoutPreset_->addItem(tr("Crónica"), QStringLiteral("cronica"));
    layoutPreset_->addItem(tr("Personalizada"), QStringLiteral("personalizada"));
    auto number = [](double min, double max, double step) {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(min, max);
        spin->setDecimals(2);
        spin->setSingleStep(step);
        return spin;
    };
    pageWidth_ = number(80.0, 400.0, 1.0);
    pageHeight_ = number(100.0, 500.0, 1.0);
    marginTop_ = number(0.0, 80.0, 1.0);
    marginRight_ = number(0.0, 80.0, 1.0);
    marginBottom_ = number(0.0, 80.0, 1.0);
    marginLeft_ = number(0.0, 80.0, 1.0);
    fontFamily_ = new QComboBox;
    fontFamily_->addItems({QStringLiteral("Garamond"), QStringLiteral("Georgia"), QStringLiteral("Literata"), QStringLiteral("Bookerly"), QStringLiteral("Atkinson")});
    fontSize_ = number(6.0, 30.0, 0.5);
    lineHeight_ = number(0.8, 3.0, 0.05);
    paragraphIndent_ = number(0.0, 30.0, 0.5);
    chapterOpening_ = new QComboBox;
    chapterOpening_->addItems({tr("Página nueva"), tr("Página impar"), tr("Continuo")});
    sceneSeparator_ = new QLineEdit;
    headerText_ = new QLineEdit;
    footerText_ = new QLineEdit;
    form->addRow(tr("Preajuste"), layoutPreset_);
    form->addRow(tr("Ancho de página (mm)"), pageWidth_);
    form->addRow(tr("Alto de página (mm)"), pageHeight_);
    form->addRow(tr("Margen superior (mm)"), marginTop_);
    form->addRow(tr("Margen derecho (mm)"), marginRight_);
    form->addRow(tr("Margen inferior (mm)"), marginBottom_);
    form->addRow(tr("Margen izquierdo (mm)"), marginLeft_);
    form->addRow(tr("Tipografía"), fontFamily_);
    form->addRow(tr("Tamaño (pt)"), fontSize_);
    form->addRow(tr("Interlineado"), lineHeight_);
    form->addRow(tr("Sangría (mm)"), paragraphIndent_);
    form->addRow(tr("Inicio de capítulo"), chapterOpening_);
    form->addRow(tr("Separador de escena"), sceneSeparator_);
    form->addRow(tr("Encabezado"), headerText_);
    form->addRow(tr("Pie"), footerText_);
    root->addWidget(scroll, 1);
    auto* actions = new QHBoxLayout;
    auto* pdf = makeButton(tr("Exportar PDF…"));
    auto* wbw = makeButton(tr("Exportar proyecto .wbw…"));
    auto* backup = makeButton(tr("Crear copia de seguridad…"));
    actions->addWidget(pdf);
    actions->addWidget(wbw);
    actions->addWidget(backup);
    actions->addStretch();
    root->addLayout(actions);

    connect(layoutPreset_, &QComboBox::currentIndexChanged, this, [this]() {
        if (!refreshing_) applyLayoutPreset(layoutPreset_->currentData().toString());
    });
    for (QDoubleSpinBox* field : {pageWidth_, pageHeight_, marginTop_, marginRight_, marginBottom_, marginLeft_, fontSize_, lineHeight_, paragraphIndent_})
        connect(field, &QDoubleSpinBox::valueChanged, this, &ReviewPage::applyLayout);
    connect(fontFamily_, &QComboBox::currentTextChanged, this, &ReviewPage::applyLayout);
    connect(chapterOpening_, &QComboBox::currentTextChanged, this, &ReviewPage::applyLayout);
    connect(sceneSeparator_, &QLineEdit::editingFinished, this, &ReviewPage::applyLayout);
    connect(headerText_, &QLineEdit::editingFinished, this, &ReviewPage::applyLayout);
    connect(footerText_, &QLineEdit::editingFinished, this, &ReviewPage::applyLayout);
    connect(pdf, &QPushButton::clicked, this, &ReviewPage::requestExportPdf);
    connect(wbw, &QPushButton::clicked, this, &ReviewPage::requestExportWbw);
    connect(backup, &QPushButton::clicked, this, &ReviewPage::requestBackup);
    return tab;
}

QWidget* ReviewPage::buildMediaTab() {
    auto* tab = new QWidget;
    auto* root = new QVBoxLayout(tab);
    auto* links = new QFormLayout;
    spotifyUrl_ = new QLineEdit;
    youtubeUrl_ = new QLineEdit;
    auto* spotifyRow = new QHBoxLayout;
    auto* openSpotify = makeButton(tr("Abrir"));
    spotifyRow->addWidget(spotifyUrl_, 1);
    spotifyRow->addWidget(openSpotify);
    auto* youtubeRow = new QHBoxLayout;
    auto* openYoutube = makeButton(tr("Abrir"));
    youtubeRow->addWidget(youtubeUrl_, 1);
    youtubeRow->addWidget(openYoutube);
    links->addRow(tr("Spotify"), spotifyRow);
    links->addRow(tr("YouTube / ambiente"), youtubeRow);
    root->addLayout(links);

    auto* split = new QHBoxLayout;
    auto* left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("Archivos locales")));
    mediaList_ = new QListWidget;
    left->addWidget(mediaList_, 1);
    auto* mediaActions = new QHBoxLayout;
    auto* add = makeButton(tr("Añadir…"));
    auto* remove = makeButton(tr("Quitar"));
    auto* play = makeButton(tr("Reproducir"));
    auto* stop = makeButton(tr("Detener"));
    mediaActions->addWidget(add);
    mediaActions->addWidget(remove);
    mediaActions->addWidget(play);
    mediaActions->addWidget(stop);
    left->addLayout(mediaActions);
    split->addLayout(left, 1);
    videoWidget_ = new QVideoWidget;
    videoWidget_->setMinimumSize(420, 240);
    videoWidget_->setStyleSheet(QStringLiteral("background:#111;"));
    split->addWidget(videoWidget_, 1);
    root->addLayout(split, 1);

    player_ = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOutput_);
    player_->setVideoOutput(videoWidget_);
    audioOutput_->setVolume(0.65f);

    connect(spotifyUrl_, &QLineEdit::editingFinished, this, &ReviewPage::applyMediaUrls);
    connect(youtubeUrl_, &QLineEdit::editingFinished, this, &ReviewPage::applyMediaUrls);
    connect(openSpotify, &QPushButton::clicked, this, [this]() { openExternalUrl(spotifyUrl_->text()); });
    connect(openYoutube, &QPushButton::clicked, this, [this]() { openExternalUrl(youtubeUrl_->text()); });
    connect(add, &QPushButton::clicked, this, &ReviewPage::addLocalMedia);
    connect(remove, &QPushButton::clicked, this, &ReviewPage::removeLocalMedia);
    connect(play, &QPushButton::clicked, this, &ReviewPage::playSelectedMedia);
    connect(stop, &QPushButton::clicked, this, &ReviewPage::stopMedia);
    connect(mediaList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { playSelectedMedia(); });
    return tab;
}

void ReviewPage::refresh() {
    if (!document_) return;
    refreshing_ = true;
    const QJsonObject profile = document_->object(QStringLiteral("profile"));
    archiveTitle_->setText(profile.value(QStringLiteral("archiveTitle")).toString());
    storyTitle_->setText(profile.value(QStringLiteral("storyTitle")).toString());
    subtitle_->setText(profile.value(QStringLiteral("subtitle")).toString());
    projectLabel_->setText(profile.value(QStringLiteral("projectLabel")).toString());
    homeHeading_->setText(profile.value(QStringLiteral("homeHeading")).toString());
    location_->setText(profile.value(QStringLiteral("location")).toString());
    author_->setText(profile.value(QStringLiteral("author")).toString());
    genre_->setText(profile.value(QStringLiteral("genre")).toString());
    projectStatus_->setText(profile.value(QStringLiteral("status")).toString());
    synopsis_->setPlainText(profile.value(QStringLiteral("synopsis")).toString());
    chapterLabels_->setText(jsonStrings(profile.value(QStringLiteral("chapterLabels")).toArray()));
    const int themeIndex = theme_->findData(profile.value(QStringLiteral("theme")).toString(QStringLiteral("grim")));
    theme_->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    setPreview(coverPreview_, profile.value(QStringLiteral("coverImageDataUrl")).toString(), tr("Sin portada"));
    setPreview(bannerPreview_, profile.value(QStringLiteral("bannerImageDataUrl")).toString(), tr("Sin banner"));
    setPreview(iconPreview_, profile.value(QStringLiteral("boardIconDataUrl")).toString(), tr("Sin icono"));

    const QJsonObject analysis = profile.value(QStringLiteral("writingAnalysis")).toObject();
    repetitionWindow_->setValue(analysis.value(QStringLiteral("repetitionWindow")).toInt(40));
    QStringList filler;
    for (const QJsonValue value : analysis.value(QStringLiteral("fillerPhrases")).toArray()) filler.append(value.toString());
    if (filler.isEmpty()) filler = {QStringLiteral("de repente"), QStringLiteral("entonces"), QStringLiteral("bueno"), QStringLiteral("en realidad"), QStringLiteral("de alguna manera"), QStringLiteral("era"), QStringLiteral("estaba"), QStringLiteral("había")};
    fillerPhrases_->setPlainText(filler.join(QLatin1Char('\n')));

    QJsonObject layout = profile.value(QStringLiteral("manuscriptLayout")).toObject();
    if (layout.isEmpty()) layout = defaultLayoutFor(QStringLiteral("editorial"));
    int presetIndex = layoutPreset_->findData(layout.value(QStringLiteral("preset")).toString(QStringLiteral("editorial")));
    layoutPreset_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
    pageWidth_->setValue(layout.value(QStringLiteral("pageWidthMm")).toDouble(152.4));
    pageHeight_->setValue(layout.value(QStringLiteral("pageHeightMm")).toDouble(228.6));
    marginTop_->setValue(layout.value(QStringLiteral("marginTopMm")).toDouble(20.0));
    marginRight_->setValue(layout.value(QStringLiteral("marginRightMm")).toDouble(19.0));
    marginBottom_->setValue(layout.value(QStringLiteral("marginBottomMm")).toDouble(22.0));
    marginLeft_->setValue(layout.value(QStringLiteral("marginLeftMm")).toDouble(19.0));
    int fontIndex = fontFamily_->findText(layout.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Garamond")));
    fontFamily_->setCurrentIndex(fontIndex >= 0 ? fontIndex : 0);
    fontSize_->setValue(layout.value(QStringLiteral("fontSizePt")).toDouble(11.0));
    lineHeight_->setValue(layout.value(QStringLiteral("lineHeight")).toDouble(1.35));
    paragraphIndent_->setValue(layout.value(QStringLiteral("paragraphIndentMm")).toDouble(5.0));
    int openingIndex = chapterOpening_->findText(layout.value(QStringLiteral("chapterOpening")).toString(QStringLiteral("Página nueva")));
    chapterOpening_->setCurrentIndex(openingIndex >= 0 ? openingIndex : 0);
    sceneSeparator_->setText(layout.value(QStringLiteral("sceneSeparator")).toString(QStringLiteral("⁂")));
    headerText_->setText(layout.value(QStringLiteral("headerText")).toString(QStringLiteral("{título}")));
    footerText_->setText(layout.value(QStringLiteral("footerText")).toString(QStringLiteral("{página}")));

    spotifyUrl_->setText(profile.value(QStringLiteral("spotifyPlaylistUrl")).toString());
    youtubeUrl_->setText(profile.value(QStringLiteral("youtubeAmbientUrl")).toString());
    mediaList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("mediaTracks"))) {
        const QJsonObject track = value.toObject();
        auto* item = new QListWidgetItem(track.value(QStringLiteral("name")).toString(QFileInfo(track.value(QStringLiteral("path")).toString()).fileName()));
        item->setData(Qt::UserRole, track.value(QStringLiteral("id")).toString());
        item->setToolTip(track.value(QStringLiteral("path")).toString());
        mediaList_->addItem(item);
    }
    refreshing_ = false;
}

void ReviewPage::applyProfile() {
    if (refreshing_ || !document_) return;
    QJsonObject profile = document_->object(QStringLiteral("profile"));
    profile.insert(QStringLiteral("archiveTitle"), archiveTitle_->text());
    profile.insert(QStringLiteral("storyTitle"), storyTitle_->text());
    profile.insert(QStringLiteral("subtitle"), subtitle_->text());
    profile.insert(QStringLiteral("projectLabel"), projectLabel_->text());
    profile.insert(QStringLiteral("homeHeading"), homeHeading_->text());
    profile.insert(QStringLiteral("location"), location_->text());
    profile.insert(QStringLiteral("author"), author_->text());
    profile.insert(QStringLiteral("genre"), genre_->text());
    profile.insert(QStringLiteral("status"), projectStatus_->text());
    profile.insert(QStringLiteral("synopsis"), synopsis_->toPlainText());
    profile.insert(QStringLiteral("chapterLabels"), stringsJson(chapterLabels_->text()));
    profile.insert(QStringLiteral("theme"), theme_->currentData().toString());
    document_->setObject(QStringLiteral("profile"), profile);
    emit changed();
}

void ReviewPage::chooseProfileImage(const QString& key) {
    if (!document_) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Elegir imagen"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp *.gif)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QByteArray bytes = file.readAll();
    if (bytes.size() > 20LL * 1024LL * 1024LL) {
        QMessageBox::warning(this, tr("Imagen demasiado grande"), tr("El límite para una imagen de proyecto es 20 MB."));
        return;
    }
    QString mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    if (mime.isEmpty()) mime = QStringLiteral("application/octet-stream");
    QJsonObject profile = document_->object(QStringLiteral("profile"));
    profile.insert(key, QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(bytes.toBase64())));
    document_->setObject(QStringLiteral("profile"), profile);
    refresh();
    emit changed();
}

void ReviewPage::clearProfileImage(const QString& key) {
    if (!document_) return;
    QJsonObject profile = document_->object(QStringLiteral("profile"));
    profile.remove(key);
    document_->setObject(QStringLiteral("profile"), profile);
    refresh();
    emit changed();
}

void ReviewPage::applyAnalysisSettings() {
    if (refreshing_ || !document_) return;
    QJsonObject profile = document_->object(QStringLiteral("profile"));
    QJsonObject settings;
    settings.insert(QStringLiteral("repetitionWindow"), repetitionWindow_->value());
    QJsonArray filler;
    for (QString line : fillerPhrases_->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (!line.isEmpty()) filler.append(line);
    }
    settings.insert(QStringLiteral("fillerPhrases"), filler);
    profile.insert(QStringLiteral("writingAnalysis"), settings);
    document_->setObject(QStringLiteral("profile"), profile);
    emit changed();
}

void ReviewPage::runAnalysis() {
    analysisResults_->clear();
    if (!document_) return;
    const QString text = allManuscriptText(*document_);
    const QString simplified = text.simplified();
    const QStringList words = simplified.split(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}'’-]+")), Qt::SkipEmptyParts);
    int repetitions = 0;
    int fillers = 0;
    int mechanical = 0;
    const int window = repetitionWindow_->value();
    QHash<QString, int> lastSeen;
    for (int i = 0; i < words.size(); ++i) {
        const QString word = words.at(i).toCaseFolded();
        if (word.size() < 4) continue;
        if (lastSeen.contains(word) && i - lastSeen.value(word) <= window) {
            ++repetitions;
            if (analysisResults_->count() < 250) analysisResults_->addItem(tr("Repetición cercana: “%1” (%2 palabras de distancia)").arg(words.at(i)).arg(i - lastSeen.value(word)));
        }
        lastSeen.insert(word, i);
    }

    for (QString phrase : fillerPhrases_->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        phrase = phrase.trimmed();
        if (phrase.isEmpty()) continue;
        QRegularExpression expression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(phrase)), QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        int count = 0;
        auto iterator = expression.globalMatch(text);
        while (iterator.hasNext()) { iterator.next(); ++count; }
        if (count > 0) {
            fillers += count;
            analysisResults_->addItem(tr("Muletilla: “%1” — %2 apariciones").arg(phrase).arg(count));
        }
    }

    const QRegularExpression doubledWord(QStringLiteral("\\b([\\p{L}]{2,})\\s+\\1\\b"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    auto doubled = doubledWord.globalMatch(text);
    while (doubled.hasNext()) {
        const auto match = doubled.next();
        ++mechanical;
        analysisResults_->addItem(tr("Palabra duplicada consecutiva: “%1”").arg(match.captured(1)));
    }
    const int doubleSpaces = text.count(QRegularExpression(QStringLiteral(" {2,}")));
    if (doubleSpaces > 0) {
        mechanical += doubleSpaces;
        analysisResults_->addItem(tr("Espacios dobles o múltiples: %1").arg(doubleSpaces));
    }
    const int wordCount = words.size();
    analysisSummary_->setText(tr("%1 palabras · %2 repeticiones cercanas · %3 muletillas · %4 incidencias mecánicas").arg(wordCount).arg(repetitions).arg(fillers).arg(mechanical));
    if (analysisResults_->count() == 0) analysisResults_->addItem(tr("No se encontraron incidencias con la configuración actual."));
}

void ReviewPage::applyLayoutPreset(const QString& preset) {
    if (!document_) return;
    if (preset == QStringLiteral("personalizada")) {
        applyLayout();
        return;
    }
    const QJsonObject layout = defaultLayoutFor(preset);
    refreshing_ = true;
    pageWidth_->setValue(layout.value(QStringLiteral("pageWidthMm")).toDouble());
    pageHeight_->setValue(layout.value(QStringLiteral("pageHeightMm")).toDouble());
    marginTop_->setValue(layout.value(QStringLiteral("marginTopMm")).toDouble());
    marginRight_->setValue(layout.value(QStringLiteral("marginRightMm")).toDouble());
    marginBottom_->setValue(layout.value(QStringLiteral("marginBottomMm")).toDouble());
    marginLeft_->setValue(layout.value(QStringLiteral("marginLeftMm")).toDouble());
    fontFamily_->setCurrentText(layout.value(QStringLiteral("fontFamily")).toString());
    fontSize_->setValue(layout.value(QStringLiteral("fontSizePt")).toDouble());
    lineHeight_->setValue(layout.value(QStringLiteral("lineHeight")).toDouble());
    paragraphIndent_->setValue(layout.value(QStringLiteral("paragraphIndentMm")).toDouble());
    chapterOpening_->setCurrentText(layout.value(QStringLiteral("chapterOpening")).toString());
    sceneSeparator_->setText(layout.value(QStringLiteral("sceneSeparator")).toString());
    headerText_->setText(layout.value(QStringLiteral("headerText")).toString());
    footerText_->setText(layout.value(QStringLiteral("footerText")).toString());
    refreshing_ = false;
    applyLayout();
}

void ReviewPage::applyLayout() {
    if (refreshing_ || !document_) return;
    QJsonObject layout;
    layout.insert(QStringLiteral("preset"), layoutPreset_->currentData().toString());
    layout.insert(QStringLiteral("pageWidthMm"), pageWidth_->value());
    layout.insert(QStringLiteral("pageHeightMm"), pageHeight_->value());
    layout.insert(QStringLiteral("marginTopMm"), marginTop_->value());
    layout.insert(QStringLiteral("marginRightMm"), marginRight_->value());
    layout.insert(QStringLiteral("marginBottomMm"), marginBottom_->value());
    layout.insert(QStringLiteral("marginLeftMm"), marginLeft_->value());
    layout.insert(QStringLiteral("fontFamily"), fontFamily_->currentText());
    layout.insert(QStringLiteral("fontSizePt"), fontSize_->value());
    layout.insert(QStringLiteral("lineHeight"), lineHeight_->value());
    layout.insert(QStringLiteral("paragraphIndentMm"), paragraphIndent_->value());
    layout.insert(QStringLiteral("chapterOpening"), chapterOpening_->currentText());
    layout.insert(QStringLiteral("sceneSeparator"), sceneSeparator_->text());
    layout.insert(QStringLiteral("headerText"), headerText_->text());
    layout.insert(QStringLiteral("footerText"), footerText_->text());
    QJsonObject profile = document_->object(QStringLiteral("profile"));
    profile.insert(QStringLiteral("manuscriptLayout"), layout);
    document_->setObject(QStringLiteral("profile"), profile);
    emit changed();
}

void ReviewPage::applyMediaUrls() {
    if (refreshing_ || !document_) return;
    QJsonObject profile = document_->object(QStringLiteral("profile"));
    profile.insert(QStringLiteral("spotifyPlaylistUrl"), spotifyUrl_->text().trimmed());
    profile.insert(QStringLiteral("youtubeAmbientUrl"), youtubeUrl_->text().trimmed());
    document_->setObject(QStringLiteral("profile"), profile);
    emit changed();
}

void ReviewPage::addLocalMedia() {
    if (!document_) return;
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Añadir audio o vídeo"), QString(), tr("Multimedia (*.mp3 *.ogg *.wav *.m4a *.mp4 *.mkv *.webm *.avi *.mov *.vob);;Todos los archivos (*)"));
    if (paths.isEmpty()) return;
    QJsonArray media = document_->array(QStringLiteral("mediaTracks"));
    for (const QString& path : paths) {
        media.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("media"))}, {QStringLiteral("name"), QFileInfo(path).fileName()}, {QStringLiteral("path"), QFileInfo(path).absoluteFilePath()}});
    }
    document_->setArray(QStringLiteral("mediaTracks"), media);
    refresh();
    emit changed();
}

void ReviewPage::removeLocalMedia() {
    if (!document_) return;
    const int row = mediaList_->currentRow();
    QJsonArray media = document_->array(QStringLiteral("mediaTracks"));
    if (row < 0 || row >= media.size()) return;
    media.removeAt(row);
    document_->setArray(QStringLiteral("mediaTracks"), media);
    stopMedia();
    refresh();
    emit changed();
}

void ReviewPage::playSelectedMedia() {
    if (!document_) return;
    const int row = mediaList_->currentRow();
    const QJsonArray media = document_->array(QStringLiteral("mediaTracks"));
    if (row < 0 || row >= media.size()) return;
    const QString path = media.at(row).toObject().value(QStringLiteral("path")).toString();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Archivo no encontrado"), tr("El archivo local ya no existe en esta ruta:\n%1").arg(path));
        return;
    }
    player_->setSource(QUrl::fromLocalFile(path));
    player_->play();
}

void ReviewPage::stopMedia() {
    player_->stop();
}

void ReviewPage::openExternalUrl(const QString& url) {
    const QUrl target = QUrl::fromUserInput(url.trimmed());
    if (!target.isValid() || target.isEmpty()) return;
    QDesktopServices::openUrl(target);
}

} // namespace wbw
