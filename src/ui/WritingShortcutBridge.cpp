#include <QMainWindow>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QWidget>

namespace wbw {

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

    window->setProperty("wbwWritingShortcutBridge", true);
}

} // namespace wbw
