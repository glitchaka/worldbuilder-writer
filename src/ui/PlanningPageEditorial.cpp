#include "ui/PlanningPage.h"

#include <QCheckBox>
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
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace wbw {
namespace {

QLabel* label(const QString& text, QWidget* parent) {
    auto* result = new QLabel(text, parent);
    result->setObjectName(QStringLiteral("planningEditorialLabel"));
    return result;
}

QWidget* field(const QString& title, QWidget* editor, QWidget* parent) {
    auto* host = new QWidget(parent);
    host->setObjectName(QStringLiteral("planningEditorialField"));
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addWidget(label(title, host));
    editor->setParent(host);
    layout->addWidget(editor);
    return host;
}

QFrame* card(const QString& title, const QString& copy, QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("planningEditorialCard"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 13, 14, 15);
    layout->setSpacing(8);
    auto* heading = new QLabel(title, frame);
    heading->setObjectName(QStringLiteral("planningEditorialCardTitle"));
    layout->addWidget(heading);
    if (!copy.isEmpty()) {
        auto* description = new QLabel(copy, frame);
        description->setObjectName(QStringLiteral("planningEditorialCardCopy"));
        description->setWordWrap(true);
        layout->addWidget(description);
    }
    return frame;
}

QPushButton* findButton(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QPushButton* button : root->findChildren<QPushButton*>()) {
        if (button && button->text().contains(text, Qt::CaseInsensitive)) return button;
    }
    return nullptr;
}

void insertSearch(QWidget* indexPanel, QListWidget* list, QLineEdit* search) {
    auto* layout = indexPanel ? qobject_cast<QVBoxLayout*>(indexPanel->layout()) : nullptr;
    if (!layout || !list || !search) return;
    const int position = qMax(0, layout->indexOf(list));
    layout->insertWidget(position, search);
}

} // namespace

void PlanningPage::configureEditorialSurface() {
    if (property("wbwPlanningEditorialV1").toBool() || !tabs_) return;
    setProperty("wbwPlanningEditorialV1", true);

    // ----- Characters --------------------------------------------------------
    if (auto* tab = findChild<QWidget*>(QStringLiteral("planningCharacters"))) {
        if (auto* hero = tab->findChild<QFrame*>(QStringLiteral("planningHero"))) hero->hide();
        auto* split = tab->findChild<QSplitter*>();
        auto* scroll = split && split->count() > 1 ? qobject_cast<QScrollArea*>(split->widget(1)) : nullptr;
        auto* indexPanel = split && split->count() > 0 ? split->widget(0) : nullptr;
        QWidget* oldEditor = scroll ? scroll->takeWidget() : nullptr;

        if (split && scroll && indexPanel && oldEditor && characterList_) {
            tab->setProperty("editorialSurface", true);
            auto* search = new QLineEdit(indexPanel);
            search->setObjectName(QStringLiteral("characterSearch"));
            search->setPlaceholderText(tr("Buscar personaje…"));
            search->setClearButtonEnabled(true);
            insertSearch(indexPanel, characterList_, search);
            connect(search, &QLineEdit::textChanged, this, [this, search](const QString&) {
                const QString query = search->text().trimmed();
                const QJsonArray characters = document_ ? document_->array(QStringLiteral("characters")) : QJsonArray();
                for (int row = 0; row < characterList_->count(); ++row) {
                    const QJsonObject character = row < characters.size() ? characters.at(row).toObject() : QJsonObject();
                    QStringList aliases;
                    for (const QJsonValue& value : character.value(QStringLiteral("aliases")).toArray()) aliases.append(value.toString());
                    const QString haystack = QStringLiteral("%1 %2 %3 %4 %5 %6")
                        .arg(character.value(QStringLiteral("name")).toString(), aliases.join(QLatin1Char(' ')),
                             character.value(QStringLiteral("role")).toString(), character.value(QStringLiteral("occupation")).toString(),
                             character.value(QStringLiteral("origin")).toString(), character.value(QStringLiteral("affiliation")).toString());
                    characterList_->item(row)->setHidden(!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive));
                }
            });

            QPushButton* imageButton = findButton(oldEditor, tr("Elegir imagen"));

            auto* center = new QWidget;
            center->setObjectName(QStringLiteral("characterEditorialCenter"));
            auto* centerLayout = new QVBoxLayout(center);
            centerLayout->setContentsMargins(22, 20, 22, 26);
            centerLayout->setSpacing(12);
            auto* kicker = new QLabel(tr("FICHA DE PERSONAJE"), center);
            kicker->setObjectName(QStringLiteral("planningEditorialKicker"));
            centerLayout->addWidget(kicker);

            characterName_->setParent(center);
            characterName_->setObjectName(QStringLiteral("characterEditorialName"));
            characterName_->setPlaceholderText(tr("Nombre del personaje"));
            centerLayout->addWidget(characterName_);

            auto* stateRow = new QHBoxLayout;
            stateRow->setSpacing(9);
            stateRow->addWidget(field(tr("Categoría"), characterCategory_, center), 1);
            stateRow->addWidget(field(tr("Estado"), characterStatus_, center), 1);
            centerLayout->addLayout(stateRow);

            auto* summary = card(tr("Resumen narrativo"), tr("La función del personaje en la historia, sin convertir la ficha en una planilla."), center);
            characterSummary_->setMinimumHeight(130);
            characterSummary_->setMaximumHeight(210);
            qobject_cast<QVBoxLayout*>(summary->layout())->addWidget(characterSummary_);
            centerLayout->addWidget(summary);

            auto* narrativeGrid = new QWidget(center);
            narrativeGrid->setObjectName(QStringLiteral("characterNarrativeGrid"));
            auto* grid = new QGridLayout(narrativeGrid);
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(12);
            grid->setVerticalSpacing(12);
            grid->setColumnStretch(0, 1);
            grid->setColumnStretch(1, 1);

            auto* background = card(tr("Trasfondo"), QString(), narrativeGrid);
            characterBackground_->setMinimumHeight(170);
            characterBackground_->setMaximumHeight(280);
            qobject_cast<QVBoxLayout*>(background->layout())->addWidget(characterBackground_);
            grid->addWidget(background, 0, 0);

            auto* physical = card(tr("Presencia física"), QString(), narrativeGrid);
            characterPhysical_->setMinimumHeight(170);
            characterPhysical_->setMaximumHeight(280);
            qobject_cast<QVBoxLayout*>(physical->layout())->addWidget(characterPhysical_);
            grid->addWidget(physical, 0, 1);
            centerLayout->addWidget(narrativeGrid);
            centerLayout->addStretch(1);
            scroll->setObjectName(QStringLiteral("characterEditorialScroll"));
            scroll->setWidget(center);

            auto* contextScroll = new QScrollArea(split);
            contextScroll->setObjectName(QStringLiteral("characterContextScroll"));
            contextScroll->setFrameShape(QFrame::NoFrame);
            contextScroll->setWidgetResizable(true);
            contextScroll->setMinimumWidth(280);
            contextScroll->setMaximumWidth(350);
            auto* context = new QWidget;
            context->setObjectName(QStringLiteral("characterContextRail"));
            auto* contextLayout = new QVBoxLayout(context);
            contextLayout->setContentsMargins(13, 16, 13, 18);
            contextLayout->setSpacing(10);
            auto* contextTitle = new QLabel(tr("Contexto"), context);
            contextTitle->setObjectName(QStringLiteral("planningEditorialContextTitle"));
            auto* contextCopy = new QLabel(tr("Identidad, vínculos y evidencias de la ficha actual."), context);
            contextCopy->setObjectName(QStringLiteral("planningEditorialCardCopy"));
            contextCopy->setWordWrap(true);
            contextLayout->addWidget(contextTitle);
            contextLayout->addWidget(contextCopy);

            auto* portrait = card(tr("Retrato"), QString(), context);
            characterImage_->setParent(portrait);
            characterImage_->setMinimumSize(210, 145);
            characterImage_->setMaximumSize(310, 210);
            qobject_cast<QVBoxLayout*>(portrait->layout())->addWidget(characterImage_, 0, Qt::AlignHCenter);
            if (imageButton) {
                imageButton->setParent(portrait);
                qobject_cast<QVBoxLayout*>(portrait->layout())->addWidget(imageButton);
            }
            contextLayout->addWidget(portrait);

            auto* identity = card(tr("Identidad"), QString(), context);
            auto* identityLayout = qobject_cast<QVBoxLayout*>(identity->layout());
            identityLayout->addWidget(field(tr("Rol"), characterRole_, identity));
            identityLayout->addWidget(field(tr("Ocupación"), characterOccupation_, identity));
            identityLayout->addWidget(field(tr("Origen"), characterOrigin_, identity));
            identityLayout->addWidget(field(tr("Afiliación"), characterAffiliation_, identity));
            identityLayout->addWidget(field(tr("Alias"), characterAliases_, identity));
            identityLayout->addWidget(field(tr("Rasgos"), characterTraits_, identity));
            contextLayout->addWidget(identity);

            auto* continuity = card(tr("Continuidad"), tr("Presencia y evidencias asociadas al manuscrito."), context);
            auto* continuityLayout = qobject_cast<QVBoxLayout*>(continuity->layout());
            continuityLayout->addWidget(field(tr("Presencia por capítulo"), characterPresence_, continuity));
            characterEvidence_->setMinimumHeight(115);
            characterEvidence_->setMaximumHeight(190);
            continuityLayout->addWidget(field(tr("Evidencias"), characterEvidence_, continuity));
            continuityLayout->addWidget(field(tr("Color de tablero"), characterColor_, continuity));
            characterBoardVisible_->setParent(continuity);
            continuityLayout->addWidget(characterBoardVisible_);
            contextLayout->addWidget(continuity);
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

    // ----- Theories ----------------------------------------------------------
    if (auto* tab = findChild<QWidget*>(QStringLiteral("planningTheories"))) {
        auto* split = tab->findChild<QSplitter*>();
        QWidget* indexPanel = split && split->count() > 0 ? split->widget(0) : nullptr;
        QWidget* oldEditor = split && split->count() > 1 ? split->widget(1) : nullptr;
        if (split && indexPanel && oldEditor && theoryList_) {
            auto* search = new QLineEdit(indexPanel);
            search->setObjectName(QStringLiteral("theorySearch"));
            search->setPlaceholderText(tr("Buscar hilo o teoría…"));
            search->setClearButtonEnabled(true);
            insertSearch(indexPanel, theoryList_, search);
            connect(search, &QLineEdit::textChanged, this, [this, search](const QString&) {
                const QString query = search->text().trimmed();
                const QJsonArray theories = document_ ? document_->array(QStringLiteral("theories")) : QJsonArray();
                for (int row = 0; row < theoryList_->count(); ++row) {
                    const QJsonObject theory = row < theories.size() ? theories.at(row).toObject() : QJsonObject();
                    const QString haystack = QStringLiteral("%1 %2 %3")
                        .arg(theory.value(QStringLiteral("title")).toString(), theory.value(QStringLiteral("thesis")).toString(), theory.value(QStringLiteral("counterpoint")).toString());
                    theoryList_->item(row)->setHidden(!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive));
                }
            });

            auto* center = new QWidget(split);
            center->setObjectName(QStringLiteral("theoryEditorialCenter"));
            auto* centerLayout = new QVBoxLayout(center);
            centerLayout->setContentsMargins(22, 20, 22, 22);
            centerLayout->setSpacing(11);
            auto* kicker = new QLabel(tr("HILO / TEORÍA"), center);
            kicker->setObjectName(QStringLiteral("planningEditorialKicker"));
            centerLayout->addWidget(kicker);
            theoryTitle_->setParent(center);
            theoryTitle_->setObjectName(QStringLiteral("theoryEditorialTitle"));
            centerLayout->addWidget(theoryTitle_);
            auto* state = new QHBoxLayout;
            state->addWidget(field(tr("Estado"), theoryStatus_, center), 1);
            state->addWidget(field(tr("Confianza"), theoryConfidence_, center), 1);
            centerLayout->addLayout(state);
            auto* thesis = card(tr("Tesis"), tr("Qué crees que ocurre y por qué merece seguimiento."), center);
            theoryThesis_->setMinimumHeight(190);
            qobject_cast<QVBoxLayout*>(thesis->layout())->addWidget(theoryThesis_);
            centerLayout->addWidget(thesis, 1);

            QWidget* displaced = split->replaceWidget(1, center);

            auto* context = new QFrame(split);
            context->setObjectName(QStringLiteral("theoryContextRail"));
            context->setMinimumWidth(285);
            context->setMaximumWidth(360);
            auto* contextLayout = new QVBoxLayout(context);
            contextLayout->setContentsMargins(13, 16, 13, 16);
            contextLayout->setSpacing(9);
            auto* evidence = card(tr("Evidencias"), QString(), context);
            theoryEvidence_->setMinimumHeight(140);
            qobject_cast<QVBoxLayout*>(evidence->layout())->addWidget(theoryEvidence_);
            contextLayout->addWidget(evidence);
            auto* counter = card(tr("Contraargumento"), QString(), context);
            theoryCounterpoint_->setMinimumHeight(120);
            qobject_cast<QVBoxLayout*>(counter->layout())->addWidget(theoryCounterpoint_);
            contextLayout->addWidget(counter);
            auto* links = card(tr("Vínculos"), QString(), context);
            auto* linksLayout = qobject_cast<QVBoxLayout*>(links->layout());
            linksLayout->addWidget(field(tr("Personajes"), theoryCharacters_, links));
            linksLayout->addWidget(field(tr("Etiquetas"), theoryTags_, links));
            contextLayout->addWidget(links);
            contextLayout->addStretch(1);
            split->addWidget(context);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes({245, 820, 320});
            if (displaced && displaced != center) displaced->deleteLater();
        }
    }

    // ----- Timeline ----------------------------------------------------------
    if (auto* tab = findChild<QWidget*>(QStringLiteral("planningTimeline"))) {
        auto* split = tab->findChild<QSplitter*>();
        QWidget* indexPanel = split && split->count() > 0 ? split->widget(0) : nullptr;
        QWidget* oldEditor = split && split->count() > 1 ? split->widget(1) : nullptr;
        if (split && indexPanel && oldEditor && timelineList_) {
            auto* search = new QLineEdit(indexPanel);
            search->setObjectName(QStringLiteral("timelineSearch"));
            search->setPlaceholderText(tr("Buscar evento…"));
            search->setClearButtonEnabled(true);
            insertSearch(indexPanel, timelineList_, search);
            connect(search, &QLineEdit::textChanged, this, [this, search](const QString&) {
                const QString query = search->text().trimmed();
                const QJsonArray timeline = document_ ? document_->array(QStringLiteral("timeline")) : QJsonArray();
                for (int row = 0; row < timelineList_->count(); ++row) {
                    const QJsonObject event = row < timeline.size() ? timeline.at(row).toObject() : QJsonObject();
                    const QString haystack = QStringLiteral("%1 %2 %3 %4")
                        .arg(event.value(QStringLiteral("chapter")).toString(), event.value(QStringLiteral("when")).toString(),
                             event.value(QStringLiteral("title")).toString(), event.value(QStringLiteral("summary")).toString());
                    timelineList_->item(row)->setHidden(!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive));
                }
            });

            auto* center = new QWidget(split);
            center->setObjectName(QStringLiteral("timelineEditorialCenter"));
            auto* centerLayout = new QVBoxLayout(center);
            centerLayout->setContentsMargins(22, 20, 22, 22);
            centerLayout->setSpacing(11);
            auto* kicker = new QLabel(tr("CRONOLOGÍA"), center);
            kicker->setObjectName(QStringLiteral("planningEditorialKicker"));
            centerLayout->addWidget(kicker);
            timelineTitle_->setParent(center);
            timelineTitle_->setObjectName(QStringLiteral("timelineEditorialTitle"));
            centerLayout->addWidget(timelineTitle_);
            auto* summary = card(tr("Evento"), tr("Describe qué cambia en la historia, no solo qué sucede."), center);
            timelineSummary_->setMinimumHeight(230);
            qobject_cast<QVBoxLayout*>(summary->layout())->addWidget(timelineSummary_);
            centerLayout->addWidget(summary, 1);
            QWidget* displaced = split->replaceWidget(1, center);

            auto* context = new QFrame(split);
            context->setObjectName(QStringLiteral("timelineContextRail"));
            context->setMinimumWidth(270);
            context->setMaximumWidth(340);
            auto* contextLayout = new QVBoxLayout(context);
            contextLayout->setContentsMargins(13, 16, 13, 16);
            contextLayout->setSpacing(9);
            auto* placement = card(tr("Posición"), tr("Ubica el evento dentro de la cronología narrativa."), context);
            auto* placementLayout = qobject_cast<QVBoxLayout*>(placement->layout());
            placementLayout->addWidget(field(tr("Capítulo"), timelineChapter_, placement));
            placementLayout->addWidget(field(tr("Cuándo"), timelineWhen_, placement));
            placementLayout->addWidget(field(tr("Intensidad"), timelineIntensity_, placement));
            contextLayout->addWidget(placement);
            auto* characters = card(tr("Personajes"), QString(), context);
            qobject_cast<QVBoxLayout*>(characters->layout())->addWidget(timelineCharacters_);
            contextLayout->addWidget(characters);
            contextLayout->addStretch(1);
            split->addWidget(context);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes({260, 820, 300});
            if (displaced && displaced != center) displaced->deleteLater();
        }
    }
}

} // namespace wbw
