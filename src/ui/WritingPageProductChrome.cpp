#include "ui/WritingPage.h"

#include "core/ArchiveDocument.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace wbw {
namespace {

QLabel* inspectorLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sceneInspectorLabel"));
    return label;
}

void refillFilter(QComboBox* combo, const QStringList& values, const QString& allText) {
    if (!combo) return;
    const QString previous = combo->currentText();
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(allText);
    QStringList unique = values;
    unique.removeAll(QString());
    unique.removeDuplicates();
    unique.sort(Qt::CaseInsensitive);
    combo->addItems(unique);
    const int oldIndex = combo->findText(previous);
    combo->setCurrentIndex(oldIndex >= 0 ? oldIndex : 0);
}

} // namespace

void WritingPage::applyProductChrome() {
    if (property("wbwProductWritingChromeV1").toBool()) return;
    setProperty("wbwProductWritingChromeV1", true);

    // The mandatory application chrome is the planning reference. Give it the
    // canonical shell name so the existing real fullscreen focus mode hides it.
    if (QWidget* productTop = window() ? window()->findChild<QWidget*>(QStringLiteral("productTopBar")) : nullptr)
        productTop->setObjectName(QStringLiteral("topShell"));

    if (tabs_ && tabs_->tabBar()) tabs_->tabBar()->hide();
    if (heroPanel_) heroPanel_->hide();

    // Manuscript: index | writing surface | contextual scene inspector.
    if (editorOuterLayout_) {
        editorOuterLayout_->setContentsMargins(0, 0, 0, 0);
        editorOuterLayout_->setSpacing(0);
    }
    if (commandBarPanel_) commandBarPanel_->setMinimumHeight(44);

    QWidget* editorCenter = editor_ ? editor_->parentWidget() : nullptr;
    if (editorCenter) {
        if (auto* centerLayout = qobject_cast<QVBoxLayout*>(editorCenter->layout())) {
            auto* manuscriptHeader = new QFrame(editorCenter);
            manuscriptHeader->setObjectName(QStringLiteral("manuscriptHeader"));
            auto* headerLayout = new QHBoxLayout(manuscriptHeader);
            headerLayout->setContentsMargins(28, 18, 28, 12);
            headerLayout->setSpacing(12);

            auto* title = new QLabel(tr("Escena"), manuscriptHeader);
            title->setObjectName(QStringLiteral("manuscriptSceneTitle"));
            auto* breadcrumb = new QLabel(tr("Manuscrito"), manuscriptHeader);
            breadcrumb->setObjectName(QStringLiteral("manuscriptBreadcrumb"));
            auto* titleColumn = new QVBoxLayout;
            titleColumn->setSpacing(2);
            titleColumn->addWidget(breadcrumb);
            titleColumn->addWidget(title);
            headerLayout->addLayout(titleColumn, 1);

            auto* focus = new QPushButton(tr("Modo sin distracciones"), manuscriptHeader);
            focus->setObjectName(QStringLiteral("writingFocusAction"));
            focus->setCursor(Qt::PointingHandCursor);
            headerLayout->addWidget(focus, 0, Qt::AlignBottom);
            connect(focus, &QPushButton::clicked, this, &WritingPage::openFocusMode);
            connect(sceneTitle_, &QLineEdit::textChanged, title, [title](const QString& value) {
                title->setText(value.trimmed().isEmpty() ? QObject::tr("Escena") : value.trimmed());
            });
            if (!sceneTitle_->text().trimmed().isEmpty()) title->setText(sceneTitle_->text().trimmed());

            centerLayout->insertWidget(0, manuscriptHeader);
        }
    }

    if (metadataPanel_ && editorSplit_) {
        metadataPanel_->setParent(editorSplit_);
        metadataPanel_->setObjectName(QStringLiteral("sceneInspector"));
        metadataPanel_->setMinimumWidth(275);
        metadataPanel_->setMaximumWidth(340);

        if (auto* grid = qobject_cast<QGridLayout*>(metadataPanel_->layout())) {
            grid->setContentsMargins(16, 16, 16, 18);
            grid->setHorizontalSpacing(8);
            grid->setVerticalSpacing(7);

            grid->removeWidget(sceneTitle_);
            grid->removeWidget(sceneStatus_);
            grid->removeWidget(scenePov_);
            grid->removeWidget(sceneLocation_);
            grid->removeWidget(sceneLayer_);

            auto* inspectorTitle = new QLabel(tr("Escena"), metadataPanel_);
            inspectorTitle->setObjectName(QStringLiteral("sceneInspectorTitle"));
            auto* inspectorCopy = new QLabel(tr("Contexto de la escena actual"), metadataPanel_);
            inspectorCopy->setObjectName(QStringLiteral("sceneInspectorCopy"));
            grid->addWidget(inspectorTitle, 0, 0, 1, 2);
            grid->addWidget(inspectorCopy, 1, 0, 1, 2);
            grid->addWidget(inspectorLabel(tr("Título"), metadataPanel_), 2, 0, 1, 2);
            grid->addWidget(sceneTitle_, 3, 0, 1, 2);
            grid->addWidget(inspectorLabel(tr("Estado"), metadataPanel_), 4, 0, 1, 2);
            grid->addWidget(sceneStatus_, 5, 0, 1, 2);
            grid->addWidget(inspectorLabel(tr("POV"), metadataPanel_), 6, 0, 1, 2);
            grid->addWidget(scenePov_, 7, 0, 1, 2);
            grid->addWidget(inspectorLabel(tr("Ubicación"), metadataPanel_), 8, 0, 1, 2);
            grid->addWidget(sceneLocation_, 9, 0, 1, 2);
            grid->addWidget(inspectorLabel(tr("Capa narrativa / tiempo"), metadataPanel_), 10, 0, 1, 2);
            grid->addWidget(sceneLayer_, 11, 0, 1, 2);

            auto* semanticTitle = new QLabel(tr("Referencias semánticas"), metadataPanel_);
            semanticTitle->setObjectName(QStringLiteral("sceneInspectorSection"));
            auto* semanticCopy = new QLabel(tr("Las referencias se marcan dentro del manuscrito. Interactúa con ellas para abrir su tarjeta contextual y saltar al Atlas."), metadataPanel_);
            semanticCopy->setObjectName(QStringLiteral("sceneInspectorHint"));
            semanticCopy->setWordWrap(true);
            grid->addWidget(semanticTitle, 12, 0, 1, 2);
            grid->addWidget(semanticCopy, 13, 0, 1, 2);
            grid->setRowStretch(14, 1);
        }

        editorSplit_->addWidget(metadataPanel_);
        editorSplit_->setStretchFactor(0, 0);
        editorSplit_->setStretchFactor(1, 1);
        editorSplit_->setStretchFactor(2, 0);
        editorSplit_->setSizes({265, 930, 310});
        metadataPanel_->show();
        detailsWereVisible_ = true;
        if (toggleDetails_) {
            QSignalBlocker blocker(toggleDetails_);
            toggleDetails_->setChecked(true);
        }
    }

    // Planning: reuse the real filters and move them into a dedicated left rail
    // instead of leaving controls scattered across the toolbar.
    auto* planningPanel = findChild<QWidget*>(QStringLiteral("sceneBoardOutlinePanel"));
    auto* boardSearch = findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch"));
    auto* boardStatus = findChild<QComboBox*>(QStringLiteral("sceneBoardStatus"));
    auto* boardOutline = findChild<QTreeWidget*>(QStringLiteral("sceneBoardOutline"));
    if (planningPanel && boardSearch && boardStatus) {
        auto* layout = qobject_cast<QVBoxLayout*>(planningPanel->layout());
        if (layout) {
            while (QLayoutItem* item = layout->takeAt(0)) {
                if (QWidget* widget = item->widget()) widget->hide();
                delete item;
            }

            auto* heading = new QLabel(tr("Planificación"), planningPanel);
            heading->setObjectName(QStringLiteral("planningFilterTitle"));
            auto* copy = new QLabel(tr("Organiza y visualiza tus escenas"), planningPanel);
            copy->setObjectName(QStringLiteral("planningFilterCopy"));
            auto* searchLabel = inspectorLabel(tr("BUSCAR ESCENAS"), planningPanel);
            layout->addWidget(heading);
            layout->addWidget(copy);
            layout->addSpacing(8);
            layout->addWidget(searchLabel);
            boardSearch->setParent(planningPanel);
            boardSearch->setFixedWidth(QWIDGETSIZE_MAX);
            boardSearch->show();
            layout->addWidget(boardSearch);

            auto* statusTitle = inspectorLabel(tr("ESTADO"), planningPanel);
            layout->addSpacing(8);
            layout->addWidget(statusTitle);
            boardStatus->setParent(planningPanel);
            boardStatus->setFixedWidth(QWIDGETSIZE_MAX);
            boardStatus->show();
            layout->addWidget(boardStatus);

            auto* povTitle = inspectorLabel(tr("PUNTO DE VISTA (POV)"), planningPanel);
            auto* povFilter = new QComboBox(planningPanel);
            povFilter->setObjectName(QStringLiteral("sceneBoardPovFilter"));
            auto* locationTitle = inspectorLabel(tr("UBICACIÓN"), planningPanel);
            auto* locationFilter = new QComboBox(planningPanel);
            locationFilter->setObjectName(QStringLiteral("sceneBoardLocationFilter"));
            layout->addSpacing(8);
            layout->addWidget(povTitle);
            layout->addWidget(povFilter);
            layout->addSpacing(8);
            layout->addWidget(locationTitle);
            layout->addWidget(locationFilter);

            auto* clearFilters = new QPushButton(tr("Limpiar filtros"), planningPanel);
            clearFilters->setObjectName(QStringLiteral("planningClearFilters"));
            clearFilters->setCursor(Qt::PointingHandCursor);
            layout->addSpacing(8);
            layout->addWidget(clearFilters);
            layout->addStretch(1);

            if (boardOutline) boardOutline->hide();

            const auto repopulate = [this, povFilter, locationFilter]() {
                QStringList povs;
                QStringList locations;
                if (document_) {
                    for (const QJsonValue& chapterValue : document_->array(QStringLiteral("writingChapters"))) {
                        for (const QJsonValue& sceneValue : chapterValue.toObject().value(QStringLiteral("scenes")).toArray()) {
                            const QJsonObject scene = sceneValue.toObject();
                            povs.append(scene.value(QStringLiteral("pov")).toString().trimmed());
                            locations.append(scene.value(QStringLiteral("location")).toString().trimmed());
                        }
                    }
                }
                refillFilter(povFilter, povs, tr("Todos"));
                refillFilter(locationFilter, locations, tr("Todas"));
            };
            repopulate();
            connect(tabs_, &QTabWidget::currentChanged, this, [repopulate](int) { repopulate(); });

            // Filters stay functional without a second scene model: the existing
            // board search already indexes title, POV, location and status.
            connect(povFilter, &QComboBox::currentTextChanged, this, [boardSearch, povFilter, locationFilter](const QString&) {
                const QString pov = povFilter->currentIndex() > 0 ? povFilter->currentText() : QString();
                const QString location = locationFilter->currentIndex() > 0 ? locationFilter->currentText() : QString();
                boardSearch->setText(!pov.isEmpty() ? pov : location);
            });
            connect(locationFilter, &QComboBox::currentTextChanged, this, [boardSearch, povFilter, locationFilter](const QString&) {
                const QString pov = povFilter->currentIndex() > 0 ? povFilter->currentText() : QString();
                const QString location = locationFilter->currentIndex() > 0 ? locationFilter->currentText() : QString();
                boardSearch->setText(!location.isEmpty() ? location : pov);
            });
            connect(clearFilters, &QPushButton::clicked, this, [boardSearch, boardStatus, povFilter, locationFilter]() {
                boardSearch->clear();
                boardStatus->setCurrentIndex(0);
                povFilter->setCurrentIndex(0);
                locationFilter->setCurrentIndex(0);
            });
        }
    }

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#writingEditorTab{background:#0b1016;}
#writingCommandBar{background:#0d141c;border:0;border-bottom:1px solid #26313b;min-height:42px;}
#writingCommandBar QToolButton,#writingCommandBar QPushButton{background:transparent;color:#9ba8b4;border:0;border-radius:6px;padding:6px 9px;}
#writingCommandBar QToolButton:hover,#writingCommandBar QPushButton:hover{background:#17222c;color:#ffffff;}
#writingCommandBar QToolButton:checked{background:#17364b;color:#72d4db;}
#indexPanel{background:#0c1219;border:0;border-right:1px solid #26313b;}
#indexTitle{color:#7e8a96;font-size:8pt;font-weight:700;letter-spacing:.8px;}
#manuscriptTree{background:transparent;color:#c9d2da;border:0;outline:0;}
#manuscriptTree::item{padding:8px 7px;border-radius:5px;margin:1px 0;}
#manuscriptTree::item:hover{background:#131d27;color:#ffffff;}
#manuscriptTree::item:selected{background:#12333f;color:#79d6da;border-left:2px solid #32c7c9;}
#editorPanel{background:#0b1016;border:0;}
#manuscriptHeader{background:#0b1016;border:0;}
#manuscriptBreadcrumb{color:#71808c;font-size:8pt;}
#manuscriptSceneTitle{color:#f1ece4;font-family:'Georgia';font-size:24pt;font-weight:500;}
#writingFocusAction{background:transparent;color:#8e9aa5;border:1px solid #2b3742;border-radius:7px;padding:7px 10px;}
#writingFocusAction:hover{background:#15202a;color:#ffffff;border-color:#42627d;}
#sceneEditor{background:#0b1016;color:#e7e0d6;border:0;padding:24px 42px 50px 42px;font-family:'Georgia';font-size:13pt;selection-background-color:#315a70;selection-color:#ffffff;}
#sceneInspector{background:#0d131a;border:0;border-left:1px solid #26313b;}
#sceneInspectorTitle{color:#f0ebe3;font-family:'Georgia';font-size:16pt;font-weight:600;}
#sceneInspectorCopy{color:#72808c;font-size:8pt;}
#sceneInspectorLabel{color:#758390;font-size:7.5pt;font-weight:700;letter-spacing:.7px;}
#sceneInspectorSection{color:#d7dde3;font-family:'Georgia';font-size:11pt;font-weight:600;margin-top:7px;}
#sceneInspectorHint{background:#101820;color:#85939f;border:1px solid #26333e;border-radius:7px;padding:9px;}
#sceneInspector QLineEdit,#sceneInspector QComboBox{background:#101820;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;padding:7px 9px;}
#sceneBoardTab{background:#0b0f14;}
#sceneBoardToolbar{background:#0d1218;border:0;border-bottom:1px solid #26313b;}
#sceneBoardHeading{font-family:'Georgia';font-size:20pt;font-weight:600;color:#f0ece5;}
#sceneBoardSubheading{color:#778590;}
#sceneBoardOutlinePanel{background:#0d1218;border:0;border-right:1px solid #26313b;min-width:250px;max-width:290px;}
#planningFilterTitle{color:#f0ece5;font-size:13pt;font-weight:700;}
#planningFilterCopy{color:#778590;font-size:8.5pt;}
#sceneBoardOutlinePanel QLineEdit,#sceneBoardOutlinePanel QComboBox{background:#111922;color:#d7e0e7;border:1px solid #2b3742;border-radius:7px;padding:8px 9px;}
#planningClearFilters{background:#111922;color:#aeb9c3;border:1px solid #2e3a46;border-radius:7px;padding:8px;}
#planningClearFilters:hover{background:#182530;color:#ffffff;}
#sceneBoardCanvas{background:#0a0f15;border:0;}
)QSS"));
}

} // namespace wbw
