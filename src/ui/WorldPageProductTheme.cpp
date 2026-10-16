#include "ui/WorldPage.h"

#include <QApplication>
#include <QWidget>

namespace wbw {

void WorldPage::refreshProductTheme() {
    applyAtlasChrome();
    installKnowledgeWorkspace();

    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    auto* atlas = findChild<QWidget*>(QStringLiteral("atlasTab"));
    auto* magic = findChild<QWidget*>(QStringLiteral("magicTab"));
    QWidget* texts = findChild<QWidget*>(QStringLiteral("textsTab"));
    if (!texts) texts = findChild<QWidget*>(QStringLiteral("referenceTextsTab"));

    if (atlas) {
        atlas->setStyleSheet(dark ? QStringLiteral(R"QSS(
#atlasTab,#worldEditorCard,#atlasCenterScroll,#atlasCenterEditorial,#atlasContentGrid{background:#0d1014;color:#e8ebef;border:0;}
#worldIndexPanel{background:#11161c;border:0;border-right:1px solid #252d37;}
#worldIndexPanel QListWidget{background:transparent;color:#d8dde5;border:0;outline:0;padding:6px;}
#worldIndexPanel QListWidget::item{padding:9px 8px;border-radius:6px;margin:1px 0;}
#worldIndexPanel QListWidget::item:hover{background:#171e27;}
#worldIndexPanel QListWidget::item:selected{background:#23313a;color:#8edfe2;}
#atlasContextScroll,#atlasContextRail{background:#11161c;border:0;}
#atlasContextRail{border-left:1px solid #252d37;}
#atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#c79b60;font-size:8pt;font-weight:700;letter-spacing:1px;}
#atlasEditorialTitle{color:#f2f3f5;font-family:'Georgia';font-size:24pt;font-weight:500;}
#atlasEditorialSubtitle,#atlasContextCopy,#atlasIndexCopy{color:#7d8998;}
#atlasIndexTitle{color:#f0ece5;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard,#atlasRelationsProductCard,#atlasMentionsProductCard{background:#12171d;border:1px solid #28313b;border-radius:9px;}
#atlasIdentityField,#atlasSummaryField{background:transparent;border:0;}
#atlasIdentityField #fieldTitle,#atlasEditorialSection #fieldTitle,#atlasContextCard #fieldTitle{color:#8e99a7;}
#atlasTab QLineEdit,#atlasTab QTextEdit,#atlasTab QComboBox,#atlasTab QListWidget{background:#0d1116;color:#e2e6eb;border:1px solid #28323c;border-radius:6px;selection-background-color:#315b67;selection-color:#fff;}
#atlasTab QLineEdit,#atlasTab QComboBox{padding:7px 9px;}
#atlasTab QTextEdit{padding:7px;}
#atlasSearch{background:#0d131a;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;padding:8px 9px;}
#atlasFilterChip{background:#121922;color:#8d99a5;border:1px solid #27323d;border-radius:6px;padding:5px 7px;}
#atlasFilterChip:checked{background:#17343f;color:#79d6da;border-color:#2a7880;}
#atlasConnectionsList,#atlasMentionsList{background:transparent;color:#cbd3db;border:0;}
#atlasEditorialSplit::handle{background:#252d37;}
)QSS") : QStringLiteral(R"QSS(
#atlasTab,#worldEditorCard,#atlasCenterScroll,#atlasCenterEditorial,#atlasContentGrid{background:#ece8e0;color:#342e28;border:0;}
#worldIndexPanel{background:#f5f1ea;border:0;border-right:1px solid #d4cabd;}
#worldIndexPanel QListWidget{background:transparent;color:#514a43;border:0;outline:0;padding:6px;}
#worldIndexPanel QListWidget::item{padding:9px 8px;border-radius:6px;margin:1px 0;}
#worldIndexPanel QListWidget::item:hover{background:#e8e1d6;}
#worldIndexPanel QListWidget::item:selected{background:#dce8e9;color:#315f68;}
#atlasContextScroll,#atlasContextRail{background:#f5f1ea;border:0;}
#atlasContextRail{border-left:1px solid #d4cabd;}
#atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#8d673e;font-size:8pt;font-weight:700;letter-spacing:1px;}
#atlasEditorialTitle{color:#302a24;font-family:'Georgia';font-size:24pt;font-weight:500;}
#atlasEditorialSubtitle,#atlasContextCopy,#atlasIndexCopy{color:#786f66;}
#atlasIndexTitle{color:#302a24;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard,#atlasRelationsProductCard,#atlasMentionsProductCard{background:#fbf8f2;border:1px solid #d4c9bb;border-radius:9px;}
#atlasIdentityField,#atlasSummaryField{background:transparent;border:0;}
#atlasIdentityField #fieldTitle,#atlasEditorialSection #fieldTitle,#atlasContextCard #fieldTitle{color:#80766d;}
#atlasTab QLineEdit,#atlasTab QTextEdit,#atlasTab QComboBox,#atlasTab QListWidget{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:6px;selection-background-color:#d3e5e6;selection-color:#315f68;}
#atlasTab QLineEdit,#atlasTab QComboBox{padding:7px 9px;}
#atlasTab QTextEdit{padding:7px;}
#atlasSearch{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:7px;padding:8px 9px;}
#atlasFilterChip{background:#f4efe7;color:#6c635a;border:1px solid #d4c9bb;border-radius:6px;padding:5px 7px;}
#atlasFilterChip:checked{background:#dce8e9;color:#315f68;border-color:#91aeb3;}
#atlasConnectionsList,#atlasMentionsList{background:transparent;color:#514a43;border:0;}
#atlasEditorialSplit::handle{background:#d4cabd;}
)QSS"));
    }

    const QString genericDark = QStringLiteral(R"QSS(
QWidget{background:#0b1016;color:#d9e1e8;}
QLineEdit,QTextEdit,QComboBox,QListWidget{background:#0f161e;color:#d8e0e6;border:1px solid #2b3742;border-radius:7px;selection-background-color:#315b67;selection-color:#fff;}
QLineEdit,QComboBox{padding:7px 9px;}
QTextEdit{padding:8px;}
QListWidget{outline:0;}
QListWidget::item{padding:8px 7px;}
QListWidget::item:hover{background:#16212a;}
QListWidget::item:selected{background:#183442;color:#8edee1;}
QPushButton{background:#111922;color:#c8d1d9;border:1px solid #2d3944;border-radius:7px;padding:7px 10px;}
QPushButton:hover{background:#1a2731;color:#fff;}
#pageTitle{color:#f0ebe3;} #pageKicker,#fieldTitle,#panelTitle{color:#7f8c97;} #pageDescription{color:#85919c;}
#magicIndexPanel,#magicEditorCard{background:#0d131a;border:1px solid #26313b;border-radius:9px;}

#magicEditorialScroll,#magicEditorialCenter,#magicEditorialGrid,#referenceEditorSurface{background:#0b1016;border:0;}
#magicContextScroll,#magicContextRail,#referenceIndexPanel,#referenceContextRail{background:#0d131a;border:0;}
#magicContextRail,#referenceContextRail{border-left:1px solid #25303b;}
#referenceIndexPanel{border-right:1px solid #25303b;}
#knowledgeEditorialCard{background:#10171e;border:1px solid #29343f;border-radius:9px;}
#knowledgeEditorialCardTitle,#knowledgeContextTitle{color:#eee9e1;font-family:'Georgia';font-size:13pt;font-weight:600;}
#knowledgeEditorialCardCopy,#knowledgeContextCopy{color:#7f8b96;font-size:8.3pt;}
#knowledgeEditorialKicker,#knowledgeFieldLabel{color:#8f9ba6;font-size:7.7pt;font-weight:700;letter-spacing:.7px;}
#magicEditorialName,#referenceEditorialTitle{background:transparent;border:0;border-bottom:1px solid #29343f;border-radius:0;color:#f2eee8;font-family:'Georgia';font-size:19pt;padding:8px 2px;}
#magicEditorialStatus{min-width:130px;}
#magicSearch,#referenceSearch{background:#0f161e;color:#d8e0e6;border:1px solid #2b3742;border-radius:7px;padding:8px 9px;}
#referenceEditorialContent{font-family:'Georgia';font-size:11pt;line-height:1.4;}
#referenceMentions{background:transparent;border:0;color:#cbd3db;}
#textsTab::pane{border:0;background:#0b1016;}
#textsTab QTabBar::tab{background:transparent;color:#7f8b96;border:0;border-bottom:2px solid transparent;padding:10px 16px;}
#textsTab QTabBar::tab:selected{color:#8bdfe2;border-bottom-color:#42c4c8;}
)QSS");
    const QString genericLight = QStringLiteral(R"QSS(
QWidget{background:#ece8e0;color:#342e28;}
QLineEdit,QTextEdit,QComboBox,QListWidget{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:7px;selection-background-color:#d3e5e6;selection-color:#315f68;}
QLineEdit,QComboBox{padding:7px 9px;}
QTextEdit{padding:8px;}
QListWidget{outline:0;}
QListWidget::item{padding:8px 7px;}
QListWidget::item:hover{background:#e8e1d6;}
QListWidget::item:selected{background:#dce8e9;color:#315f68;}
QPushButton{background:#f4efe7;color:#4d453d;border:1px solid #d4cabc;border-radius:7px;padding:7px 10px;}
QPushButton:hover{background:#e8dfd4;color:#29251f;}
#pageTitle{color:#302a24;} #pageKicker,#fieldTitle,#panelTitle{color:#80766d;} #pageDescription{color:#786f66;}
#magicIndexPanel,#magicEditorCard{background:#fbf8f2;border:1px solid #d4c9bb;border-radius:9px;}

#magicEditorialScroll,#magicEditorialCenter,#magicEditorialGrid,#referenceEditorSurface{background:#ece8e0;border:0;}
#magicContextScroll,#magicContextRail,#referenceIndexPanel,#referenceContextRail{background:#f5f1ea;border:0;}
#magicContextRail,#referenceContextRail{border-left:1px solid #d4cabd;}
#referenceIndexPanel{border-right:1px solid #d4cabd;}
#knowledgeEditorialCard{background:#fbf8f2;border:1px solid #d5cbbf;border-radius:9px;}
#knowledgeEditorialCardTitle,#knowledgeContextTitle{color:#342e28;font-family:'Georgia';font-size:13pt;font-weight:600;}
#knowledgeEditorialCardCopy,#knowledgeContextCopy{color:#796f65;font-size:8.3pt;}
#knowledgeEditorialKicker,#knowledgeFieldLabel{color:#83776a;font-size:7.7pt;font-weight:700;letter-spacing:.7px;}
#magicEditorialName,#referenceEditorialTitle{background:transparent;border:0;border-bottom:1px solid #d5cbbf;border-radius:0;color:#302a24;font-family:'Georgia';font-size:19pt;padding:8px 2px;}
#magicEditorialStatus{min-width:130px;}
#magicSearch,#referenceSearch{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:7px;padding:8px 9px;}
#referenceEditorialContent{font-family:'Georgia';font-size:11pt;}
#referenceMentions{background:transparent;border:0;color:#514a43;}
#textsTab::pane{border:0;background:#ece8e0;}
#textsTab QTabBar::tab{background:transparent;color:#796f65;border:0;border-bottom:2px solid transparent;padding:10px 16px;}
#textsTab QTabBar::tab:selected{color:#315f68;border-bottom-color:#4d969c;}
)QSS");
    if (magic) magic->setStyleSheet(dark ? genericDark : genericLight);
    if (texts) texts->setStyleSheet(dark ? genericDark : genericLight);
}

} // namespace wbw
