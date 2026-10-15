#include "ui/WorldPage.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace wbw {
namespace {

QPushButton* knowledgeButton(const QString& title, const QString& copy, int target, QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setObjectName(QStringLiteral("knowledgeNavButton"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("targetTab", target);
    button->setText(QStringLiteral("%1\n%2").arg(title, copy));
    button->setMinimumHeight(58);
    return button;
}

void applyKnowledgeTheme(QWidget* shell) {
    if (!shell) return;
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    if (dark) {
        shell->setStyleSheet(QStringLiteral(R"QSS(
#knowledgeWorkspace{background:#0a0e13;}
#knowledgeNavigation{background:#0d131a;border-right:1px solid #25303a;}
#knowledgeKicker{color:#8a98a5;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;}
#knowledgeTitle{color:#f0ebe3;font-family:'Georgia';font-size:16pt;font-weight:600;}
#knowledgeCopy{color:#74818d;font-size:8.5pt;}
#knowledgeNavButton{background:transparent;color:#8f9ba6;border:0;border-left:2px solid transparent;border-radius:0;padding:10px 12px;text-align:left;font-size:8.5pt;}
#knowledgeNavButton:hover{background:#131c24;color:#d9e2e8;}
#knowledgeNavButton:checked{background:#13212a;color:#8bdfe2;border-left-color:#42c4c8;}
#knowledgeHint{background:#11171e;color:#75828e;border:1px solid #26313b;border-radius:8px;padding:10px;font-size:8pt;}
#worldTabs::pane{border:0;background:#0a0e13;}
)QSS"));
    } else {
        shell->setStyleSheet(QStringLiteral(R"QSS(
#knowledgeWorkspace{background:#ece8e0;}
#knowledgeNavigation{background:#f5f1ea;border-right:1px solid #d4cabd;}
#knowledgeKicker{color:#85796c;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;}
#knowledgeTitle{color:#302a24;font-family:'Georgia';font-size:16pt;font-weight:600;}
#knowledgeCopy{color:#796f65;font-size:8.5pt;}
#knowledgeNavButton{background:transparent;color:#655e56;border:0;border-left:2px solid transparent;border-radius:0;padding:10px 12px;text-align:left;font-size:8.5pt;}
#knowledgeNavButton:hover{background:#e8e1d6;color:#28231f;}
#knowledgeNavButton:checked{background:#dce8e9;color:#315f68;border-left-color:#4d969c;}
#knowledgeHint{background:#eee8de;color:#73695f;border:1px solid #d3c8ba;border-radius:8px;padding:10px;font-size:8pt;}
#worldTabs::pane{border:0;background:#ece8e0;}
)QSS"));
    }
}

} // namespace

void WorldPage::installKnowledgeWorkspace() {
    if (!tabs_) return;

    if (property("wbwKnowledgeWorkspaceInstalled").toBool()) {
        if (auto* shell = findChild<QWidget*>(QStringLiteral("knowledgeWorkspace"))) applyKnowledgeTheme(shell);
        return;
    }

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if (!root) return;
    setProperty("wbwKnowledgeWorkspaceInstalled", true);

    if (tabs_->tabBar()) tabs_->tabBar()->hide();
    tabs_->setDocumentMode(true);

    QWidget* planning = findChild<QWidget*>(QStringLiteral("planningPage"));
    QWidget* atlas = findChild<QWidget*>(QStringLiteral("atlasTab"));
    QWidget* magic = findChild<QWidget*>(QStringLiteral("magicTab"));
    QWidget* maps = findChild<QWidget*>(QStringLiteral("mapsTab"));
    QWidget* texts = findChild<QWidget*>(QStringLiteral("textsTab"));
    if (!texts) texts = findChild<QWidget*>(QStringLiteral("referenceTextsTab"));

    const int planningIndex = planning ? tabs_->indexOf(planning) : -1;
    const int atlasIndex = atlas ? tabs_->indexOf(atlas) : 0;
    const int magicIndex = magic ? tabs_->indexOf(magic) : 1;
    const int mapsIndex = maps ? tabs_->indexOf(maps) : -1;
    const int textsIndex = texts ? tabs_->indexOf(texts) : (tabs_->count() > 3 ? tabs_->count() - 1 : -1);
    if (mapsIndex >= 0) tabs_->setTabVisible(mapsIndex, false);

    root->removeWidget(tabs_);

    auto* shell = new QFrame(this);
    shell->setObjectName(QStringLiteral("knowledgeWorkspace"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* navigation = new QFrame(shell);
    navigation->setObjectName(QStringLiteral("knowledgeNavigation"));
    navigation->setFixedWidth(224);
    auto* navLayout = new QVBoxLayout(navigation);
    navLayout->setContentsMargins(18, 24, 16, 18);
    navLayout->setSpacing(8);

    auto* kicker = new QLabel(tr("DOCUMENTACIÓN"), navigation);
    kicker->setObjectName(QStringLiteral("knowledgeKicker"));
    auto* title = new QLabel(tr("Mundo"), navigation);
    title->setObjectName(QStringLiteral("knowledgeTitle"));
    auto* copy = new QLabel(tr("Atlas narrativo, fichas, sistemas y material de referencia en un único espacio editorial."), navigation);
    copy->setObjectName(QStringLiteral("knowledgeCopy"));
    copy->setWordWrap(true);
    navLayout->addWidget(kicker);
    navLayout->addWidget(title);
    navLayout->addWidget(copy);
    navLayout->addSpacing(10);

    auto* group = new QButtonGroup(navigation);
    group->setExclusive(true);

    if (planningIndex >= 0) {
        auto* characters = knowledgeButton(tr("Personajes y tramas"), tr("Fichas, relaciones, teorías y cronología"), planningIndex, navigation);
        group->addButton(characters);
        navLayout->addWidget(characters);
    }

    auto* atlasButton = knowledgeButton(tr("Atlas"), tr("Lugares, culturas y organizaciones"), atlasIndex, navigation);
    auto* magicButton = knowledgeButton(tr("Magia"), tr("Sistemas, costes y límites"), magicIndex, navigation);
    group->addButton(atlasButton);
    group->addButton(magicButton);
    navLayout->addWidget(atlasButton);
    navLayout->addWidget(magicButton);

    if (textsIndex >= 0) {
        auto* textsButton = knowledgeButton(tr("Referencias"), tr("Textos, fuentes y documentación"), textsIndex, navigation);
        group->addButton(textsButton);
        navLayout->addWidget(textsButton);
    }

    navLayout->addStretch(1);
    auto* hint = new QLabel(tr("Mapas tiene su propio estudio cartográfico y no se duplica dentro del Atlas."), navigation);
    hint->setObjectName(QStringLiteral("knowledgeHint"));
    hint->setWordWrap(true);
    navLayout->addWidget(hint);

    shellLayout->addWidget(navigation);
    tabs_->setParent(shell);
    shellLayout->addWidget(tabs_, 1);
    root->addWidget(shell, 1);

    const auto updateChecked = [group](int index) {
        for (QAbstractButton* abstract : group->buttons()) {
            auto* button = qobject_cast<QPushButton*>(abstract);
            if (!button) continue;
            button->setChecked(button->property("targetTab").toInt() == index);
        }
    };

    for (QAbstractButton* abstract : group->buttons()) {
        auto* button = qobject_cast<QPushButton*>(abstract);
        if (!button) continue;
        connect(button, &QPushButton::clicked, this, [this, button]() {
            if (!tabs_) return;
            const int target = button->property("targetTab").toInt();
            if (target >= 0 && target < tabs_->count()) tabs_->setCurrentIndex(target);
        });
    }
    connect(tabs_, &QTabWidget::currentChanged, this, updateChecked);

    if (atlasIndex >= 0) tabs_->setCurrentIndex(atlasIndex);
    updateChecked(tabs_->currentIndex());
    applyKnowledgeTheme(shell);
}

} // namespace wbw
