#include "ui/WorldPage.h"

#include "core/ArchiveDocument.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace wbw {
namespace {

QString sceneText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument document;
    document.setHtml(scene.value(QStringLiteral("content")).toString());
    return document.toPlainText();
}

QStringList aliasesOf(const QJsonObject& entry) {
    QStringList aliases;
    const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty()) aliases.append(name);
    for (const QJsonValue& value : entry.value(QStringLiteral("aliases")).toArray()) {
        const QString alias = value.toString().trimmed();
        if (!alias.isEmpty() && !aliases.contains(alias, Qt::CaseInsensitive)) aliases.append(alias);
    }
    return aliases;
}

QString entrySearchText(const QJsonObject& entry) {
    QStringList values{
        entry.value(QStringLiteral("name")).toString(),
        entry.value(QStringLiteral("kind")).toString(),
        entry.value(QStringLiteral("summary")).toString()
    };
    for (const QJsonValue& value : entry.value(QStringLiteral("aliases")).toArray()) values.append(value.toString());
    for (const QJsonValue& value : entry.value(QStringLiteral("tags")).toArray()) values.append(value.toString());
    return values.join(QLatin1Char(' '));
}

QString filterKeyForKind(const QString& kind) {
    const QString lower = kind.toLower();
    if (lower.contains(QStringLiteral("región")) || lower.contains(QStringLiteral("region")) || lower.contains(QStringLiteral("país")) || lower.contains(QStringLiteral("reino")))
        return QStringLiteral("regions");
    if (lower.contains(QStringLiteral("ciudad")) || lower.contains(QStringLiteral("lugar")) || lower.contains(QStringLiteral("pueblo")))
        return QStringLiteral("places");
    if (lower.contains(QStringLiteral("organización")) || lower.contains(QStringLiteral("organizacion")) || lower.contains(QStringLiteral("religión")) || lower.contains(QStringLiteral("religion")))
        return QStringLiteral("groups");
    return QStringLiteral("other");
}

QPushButton* filterChip(const QString& text, const QString& key, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("atlasFilterChip"));
    button->setProperty("filterKey", key);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

void WorldPage::configureAtlasProductSurface() {
    applyAtlasChrome();
    if (property("wbwAtlasProductSurfaceV2").toBool() || !worldList_ || !document_) return;

    auto* index = findChild<QWidget*>(QStringLiteral("worldIndexPanel"));
    auto* contextRail = findChild<QFrame*>(QStringLiteral("atlasContextRail"));
    if (!index || !contextRail) return;
    auto* indexLayout = qobject_cast<QVBoxLayout*>(index->layout());
    auto* contextLayout = qobject_cast<QVBoxLayout*>(contextRail->layout());
    if (!indexLayout || !contextLayout) return;

    setProperty("wbwAtlasProductSurfaceV2", true);

    for (QLabel* label : index->findChildren<QLabel*>()) {
        if (label->objectName() == QStringLiteral("panelTitle")) label->hide();
    }

    auto* moduleTitle = new QLabel(tr("Mundo / Atlas"), index);
    moduleTitle->setObjectName(QStringLiteral("atlasIndexTitle"));
    auto* moduleCopy = new QLabel(tr("Lugares, regiones y documentación de tu mundo."), index);
    moduleCopy->setObjectName(QStringLiteral("atlasIndexCopy"));
    moduleCopy->setWordWrap(true);
    auto* search = new QLineEdit(index);
    search->setObjectName(QStringLiteral("atlasSearch"));
    search->setPlaceholderText(tr("Buscar lugares, regiones, etiquetas…"));
    search->setClearButtonEnabled(true);

    auto* filters = new QFrame(index);
    filters->setObjectName(QStringLiteral("atlasFilterRow"));
    auto* filterLayout = new QHBoxLayout(filters);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(5);
    auto* all = filterChip(tr("Todos"), QStringLiteral("all"), filters);
    auto* regions = filterChip(tr("Regiones"), QStringLiteral("regions"), filters);
    auto* places = filterChip(tr("Lugares"), QStringLiteral("places"), filters);
    auto* groups = filterChip(tr("Grupos"), QStringLiteral("groups"), filters);
    filterLayout->addWidget(all);
    filterLayout->addWidget(regions);
    filterLayout->addWidget(places);
    filterLayout->addWidget(groups);

    auto* filterGroup = new QButtonGroup(filters);
    filterGroup->setExclusive(true);
    for (QPushButton* button : {all, regions, places, groups}) filterGroup->addButton(button);
    all->setChecked(true);

    const int listIndex = qMax(0, indexLayout->indexOf(worldList_));
    indexLayout->insertWidget(listIndex, moduleTitle);
    indexLayout->insertWidget(listIndex + 1, moduleCopy);
    indexLayout->insertWidget(listIndex + 2, search);
    indexLayout->insertWidget(listIndex + 3, filters);

    worldList_->setMinimumWidth(0);
    worldList_->setMaximumWidth(QWIDGETSIZE_MAX);

    const auto applyFilter = [this, search, filterGroup]() {
        if (!document_ || !worldList_) return;
        const QString query = search->text().trimmed();
        QString key = QStringLiteral("all");
        if (auto* checked = qobject_cast<QPushButton*>(filterGroup->checkedButton())) key = checked->property("filterKey").toString();
        const QJsonArray world = document_->array(QStringLiteral("world"));
        for (int i = 0; i < worldList_->count(); ++i) {
            const QJsonObject entry = i < world.size() ? world.at(i).toObject() : QJsonObject();
            const bool queryMatches = query.isEmpty() || entrySearchText(entry).contains(query, Qt::CaseInsensitive);
            const QString kindKey = filterKeyForKind(entry.value(QStringLiteral("kind")).toString());
            const bool kindMatches = key == QStringLiteral("all") || kindKey == key;
            worldList_->item(i)->setHidden(!(queryMatches && kindMatches));
        }
    };
    connect(search, &QLineEdit::textChanged, this, [applyFilter](const QString&) { applyFilter(); });
    connect(filterGroup, &QButtonGroup::buttonClicked, this, [applyFilter](QAbstractButton*) { applyFilter(); });

    auto* connectedCard = new QFrame(contextRail);
    connectedCard->setObjectName(QStringLiteral("atlasRelationsProductCard"));
    auto* connectedLayout = new QVBoxLayout(connectedCard);
    connectedLayout->setContentsMargins(10, 10, 10, 10);
    connectedLayout->setSpacing(6);
    auto* connectedTitle = new QLabel(tr("Conectado con"), connectedCard);
    connectedTitle->setObjectName(QStringLiteral("atlasProductCardTitle"));
    auto* connected = new QListWidget(connectedCard);
    connected->setObjectName(QStringLiteral("atlasConnectionsList"));
    connected->setMaximumHeight(180);
    connectedLayout->addWidget(connectedTitle);
    connectedLayout->addWidget(connected);

    auto* mentionsCard = new QFrame(contextRail);
    mentionsCard->setObjectName(QStringLiteral("atlasMentionsProductCard"));
    auto* mentionsLayout = new QVBoxLayout(mentionsCard);
    mentionsLayout->setContentsMargins(10, 10, 10, 10);
    mentionsLayout->setSpacing(6);
    auto* mentionsTitle = new QLabel(tr("Menciones en el manuscrito"), mentionsCard);
    mentionsTitle->setObjectName(QStringLiteral("atlasProductCardTitle"));
    auto* mentions = new QListWidget(mentionsCard);
    mentions->setObjectName(QStringLiteral("atlasMentionsList"));
    mentions->setMaximumHeight(220);
    mentionsLayout->addWidget(mentionsTitle);
    mentionsLayout->addWidget(mentions);

    contextLayout->insertWidget(2, connectedCard);
    contextLayout->insertWidget(3, mentionsCard);

    auto refreshContext = std::make_shared<std::function<void()>>();
    *refreshContext = [this, connected, mentions]() {
        connected->clear();
        mentions->clear();
        if (!document_ || !worldList_) return;
        const QJsonArray world = document_->array(QStringLiteral("world"));
        const int row = worldList_->currentRow();
        if (row < 0 || row >= world.size()) return;
        const QJsonObject current = world.at(row).toObject();
        const QStringList aliases = aliasesOf(current);

        const QString relationText = current.value(QStringLiteral("relations")).toString()
                                   + QLatin1Char(' ') + current.value(QStringLiteral("locations")).toString()
                                   + QLatin1Char(' ') + current.value(QStringLiteral("conflicts")).toString();
        for (int i = 0; i < world.size(); ++i) {
            if (i == row) continue;
            const QJsonObject candidate = world.at(i).toObject();
            const QString name = candidate.value(QStringLiteral("name")).toString().trimmed();
            if (name.isEmpty() || !relationText.contains(name, Qt::CaseInsensitive)) continue;
            auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(name, candidate.value(QStringLiteral("kind")).toString()), connected);
            item->setData(Qt::UserRole, i);
        }
        if (connected->count() == 0) {
            auto* empty = new QListWidgetItem(tr("Sin vínculos explícitos todavía"), connected);
            empty->setFlags(Qt::NoItemFlags);
        }

        const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
        for (const QJsonValue& chapterValue : chapters) {
            const QJsonObject chapter = chapterValue.toObject();
            const QString chapterLabel = chapter.value(QStringLiteral("title")).toString(
                chapter.value(QStringLiteral("label")).toString(tr("Capítulo")));
            for (const QJsonValue& sceneValue : chapter.value(QStringLiteral("scenes")).toArray()) {
                const QJsonObject scene = sceneValue.toObject();
                const QString body = sceneText(scene);
                bool found = false;
                for (const QString& alias : aliases) {
                    if (alias.size() >= 2 && body.contains(alias, Qt::CaseInsensitive)) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
                const QString sceneTitle = scene.value(QStringLiteral("title")).toString(tr("Escena"));
                auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(sceneTitle, chapterLabel), mentions);
                item->setToolTip(tr("Esta entrada aparece en el texto de la escena."));
            }
        }
        if (mentions->count() == 0) {
            auto* empty = new QListWidgetItem(tr("Sin menciones en el manuscrito"), mentions);
            empty->setFlags(Qt::NoItemFlags);
        }
    };

    connect(worldList_, &QListWidget::currentRowChanged, this, [refreshContext](int) {
        QTimer::singleShot(0, [refreshContext]() { (*refreshContext)(); });
    });
    connect(worldName_, &QLineEdit::textChanged, this, [refreshContext](const QString&) { (*refreshContext)(); });
    connect(worldAliases_, &QLineEdit::textChanged, this, [refreshContext](const QString&) { (*refreshContext)(); });
    connect(worldRelations_, &QTextEdit::textChanged, this, [refreshContext]() { (*refreshContext)(); });
    connect(connected, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const int row = item->data(Qt::UserRole).toInt(-1);
        if (row >= 0 && row < worldList_->count()) worldList_->setCurrentRow(row);
    });

    applyFilter();
    (*refreshContext)();

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#atlasIndexTitle{color:#f0ece5;font-family:'Georgia';font-size:16pt;font-weight:600;margin:2px 4px 0 4px;}
#atlasIndexCopy{color:#77838f;font-size:8.5pt;margin:0 4px 6px 4px;}
#atlasSearch{background:#0d131a;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;padding:8px 9px;margin:0 4px 4px 4px;}
#atlasFilterRow{background:transparent;border:0;margin:0 4px 6px 4px;}
#atlasFilterChip{background:#121922;color:#8d99a5;border:1px solid #27323d;border-radius:6px;padding:5px 7px;font-size:7.5pt;}
#atlasFilterChip:hover{background:#18242e;color:#ffffff;}
#atlasFilterChip:checked{background:#17343f;color:#79d6da;border-color:#2a7880;}
#atlasRelationsProductCard,#atlasMentionsProductCard{background:#12171d;border:1px solid #28313b;border-radius:9px;}
#atlasProductCardTitle{color:#e7e3dc;font-family:'Georgia';font-size:11pt;font-weight:600;}
#atlasConnectionsList,#atlasMentionsList{background:transparent;color:#cbd3db;border:0;outline:0;}
#atlasConnectionsList::item,#atlasMentionsList::item{padding:7px 5px;border-bottom:1px solid #222b34;}
#atlasConnectionsList::item:hover,#atlasMentionsList::item:hover{background:#18222c;border-radius:5px;}
)QSS"));
}

} // namespace wbw
