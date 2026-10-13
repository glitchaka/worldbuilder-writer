#include "ui/PilinReyEditor.h"
#include "ui/SettingsWorkspace.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QTextDocument>
#include <QTextEdit>
#include <QWidget>

namespace wbw {
namespace {

void hideFieldAndLabel(QWidget* field) {
    if (!field) return;
    field->hide();
    QWidget* scope = field->parentWidget();
    if (!scope) return;
    for (QLabel* label : scope->findChildren<QLabel*>()) {
        if (label->buddy() == field) {
            label->hide();
            break;
        }
    }
}

void removeInertSettings(SettingsWorkspace* settings) {
    if (!settings || settings->property("wbwInertSettingsRemoved").toBool()) return;
    settings->setProperty("wbwInertSettingsRemoved", true);

    const QStringList inertChecks{
        QObject::tr("Corrección gramatical"),
        QObject::tr("Sugerencias de estilo"),
        QObject::tr("Comillas tipográficas"),
        QObject::tr("Incluir notas y referencias"),
        QObject::tr("Incluir mundo y personajes"),
        QObject::tr("Incluir portada"),
        QObject::tr("Incluir índice")
    };
    for (QCheckBox* check : settings->findChildren<QCheckBox*>()) {
        if (inertChecks.contains(check->text())) hideFieldAndLabel(check);
    }

    for (QComboBox* combo : settings->findChildren<QComboBox*>()) {
        QStringList items;
        for (int i = 0; i < combo->count(); ++i) items << combo->itemText(i);
        const bool suggestionLevel = items.contains(QObject::tr("Conservador")) && items.contains(QObject::tr("Creativo"));
        const bool backupRetention = items.contains(QObject::tr("Durante 30 días")) && items.contains(QObject::tr("Durante 1 año"));
        const bool defaultExport = items.contains(QObject::tr("PDF formateado")) && items.contains(QObject::tr("WBW nativo"));
        const bool renderQuality = items.contains(QObject::tr("Rápida")) && items.contains(QObject::tr("Alta"));
        const bool accentSelector = items.contains(QObject::tr("Azul")) && items.contains(QObject::tr("Turquesa")) && items.contains(QObject::tr("Magenta"));
        if (suggestionLevel || backupRetention || defaultExport || renderQuality || accentSelector) hideFieldAndLabel(combo);
    }
}

void applyWritingPreferences(QWidget* page, QShortcut* focusShortcut, QShortcut* proofShortcut) {
    if (!page) return;
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));

    if (focusShortcut) focusShortcut->setKey(QKeySequence(settings.value(QStringLiteral("shortcut/focus"), QStringLiteral("Ctrl+Shift+F")).toString()));
    if (proofShortcut) proofShortcut->setKey(QKeySequence(settings.value(QStringLiteral("shortcut/proofread"), QStringLiteral("F7")).toString()));

    auto* editor = page->findChild<QTextEdit*>(QStringLiteral("sceneEditor"));
    if (!editor) return;

    QFont font(settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString());
    font.setPointSize(settings.value(QStringLiteral("editor/fontSize"), 14).toInt());
    editor->setFont(font);
    editor->document()->setDefaultFont(font);

    const int lineHeight = qBound(110, settings.value(QStringLiteral("editor/lineHeight"), 150).toInt(), 220);
    editor->document()->setDefaultStyleSheet(QStringLiteral("p { line-height: %1%; }").arg(lineHeight));

    const int targetWidth = qBound(520, settings.value(QStringLiteral("editor/lineWidth"), 680).toInt(), 960);
    const int horizontalPadding = qBound(28, (960 - targetWidth) / 2 + 34, 240);
    editor->setStyleSheet(QStringLiteral(
        "QTextEdit#sceneEditor{padding-left:%1px;padding-right:%1px;}"
    ).arg(horizontalPadding));
}

} // namespace

void installWritingShortcutBridge(QMainWindow* window) {
    if (!window || window->property("wbwWritingShortcutBridge").toBool()) return;
    QWidget* page = window->findChild<QWidget*>(QStringLiteral("writingPage"));
    if (!page) return;

    auto findButton = [page](const QString& text) -> QPushButton* {
        for (QPushButton* button : page->findChildren<QPushButton*>())
            if (button->text() == text) return button;
        return nullptr;
    };

    QPushButton* focus = findButton(QObject::tr("Enfoque"));
    QPushButton* proof = findButton(QObject::tr("Revisar"));
    if (!focus || !proof) return;

    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));

    auto* focusShortcut = new QShortcut(QKeySequence(settings.value(QStringLiteral("shortcut/focus"), QStringLiteral("Ctrl+Shift+F")).toString()), page);
    focusShortcut->setObjectName(QStringLiteral("wbwFocusShortcut"));
    focusShortcut->setContext(Qt::WindowShortcut);
    QObject::connect(focusShortcut, &QShortcut::activated, focus, &QPushButton::click);

    auto* proofShortcut = new QShortcut(QKeySequence(settings.value(QStringLiteral("shortcut/proofread"), QStringLiteral("F7")).toString()), page);
    proofShortcut->setObjectName(QStringLiteral("wbwProofShortcut"));
    proofShortcut->setContext(Qt::WindowShortcut);
    QObject::connect(proofShortcut, &QShortcut::activated, proof, &QPushButton::click);

    applyWritingPreferences(page, focusShortcut, proofShortcut);

    PilinReyEditor* mapEditor = window->findChild<PilinReyEditor*>();
    if (mapEditor) mapEditor->applyPersistentCanvasPreferences();

    if (auto* settingsPage = window->findChild<SettingsWorkspace*>()) {
        removeInertSettings(settingsPage);
        QObject::connect(settingsPage, &SettingsWorkspace::preferencesChanged, page,
                         [page, focusShortcut, proofShortcut, mapEditor]() {
            applyWritingPreferences(page, focusShortcut, proofShortcut);
            if (mapEditor) mapEditor->applyPersistentCanvasPreferences();
        });
    }

    window->setProperty("wbwWritingShortcutBridge", true);
}

} // namespace wbw
