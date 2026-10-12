#include "ui/WritingPerformanceController.h"

#include "ui/WritingPage.h"
#include "ui/SemanticTextEdit.h"

#include <QEvent>
#include <QMainWindow>
#include <QPointer>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTimer>
#include <QTreeWidget>

namespace wbw {
namespace {

class ManuscriptGuard final : public QObject {
public:
    explicit ManuscriptGuard(QMainWindow* window) : QObject(window), window_(window) {
        page_ = window ? window->findChild<WritingPage*>(QStringLiteral("writingPage")) : nullptr;
        editor_ = window ? window->findChild<SemanticTextEdit*>(QStringLiteral("sceneEditor")) : nullptr;
        tree_ = window ? window->findChild<QTreeWidget*>(QStringLiteral("manuscriptTree")) : nullptr;
        if (!page_ || !editor_) return;

        commitTimer_.setSingleShot(true);
        commitTimer_.setInterval(380);
        marginTimer_.setSingleShot(true);
        marginTimer_.setInterval(60);

        // No persistir/reconstruir una escena completa por cada carácter. El editor conserva el
        // QTextDocument vivo y el modelo se sincroniza sólo después de una pausa breve.
        QObject::disconnect(editor_, &QTextEdit::textChanged, page_, &WritingPage::applyScene);
        connect(editor_, &QTextEdit::textChanged, this, [this]() {
            if (page_) commitTimer_.start();
        });
        connect(&commitTimer_, &QTimer::timeout, this, [this]() { flushPendingEdit(); });
        connect(&marginTimer_, &QTimer::timeout, this, [this]() { applyMargins(); });

        window_->installEventFilter(this);
        editor_->installEventFilter(this);
        if (editor_->viewport()) editor_->viewport()->installEventFilter(this);
        if (tree_) {
            tree_->installEventFilter(this);
            if (tree_->viewport()) tree_->viewport()->installEventFilter(this);
        }
        applyMargins();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        // LayoutRequest se dispara en cascada cuando Qt recalcula textos y splitters. Escucharlo
        // aquí generaba ciclos de maquetación. Sólo recalculamos la columna cuando cambia realmente
        // el tamaño o cuando el editor vuelve a mostrarse.
        if ((watched == window_ || watched == editor_ || watched == editor_->viewport()) &&
            (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            marginTimer_.start();
        }

        if (watched == window_ && event->type() == QEvent::Close && commitTimer_.isActive())
            flushPendingEdit();

        const bool onTree = tree_ && (watched == tree_ || watched == tree_->viewport());
        if (onTree && commitTimer_.isActive() &&
            (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress || event->type() == QEvent::FocusOut)) {
            flushPendingEdit();
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void flushPendingEdit() {
        if (!page_) return;
        commitTimer_.stop();
        page_->applyScene();
    }

    void applyMargins() {
        if (!editor_ || !editor_->document() || !editor_->document()->rootFrame()) return;

        const int viewportWidth = editor_->viewport() ? editor_->viewport()->width() : editor_->width();
        if (viewportWidth <= 0) return;

        // Columna editorial estable: similar al mockup de escritura, sin líneas pegadas a los
        // bordes aunque se abran/cierre el índice o el panel contextual.
        const int targetTextWidth = qBound(640, viewportWidth - 104, 780);
        const int side = qMax(32, (viewportWidth - targetTextWidth) / 2);

        editor_->document()->setDocumentMargin(0.0);
        QTextFrameFormat format = editor_->document()->rootFrame()->frameFormat();
        format.setLeftMargin(side);
        format.setRightMargin(side);
        format.setTopMargin(34);
        format.setBottomMargin(56);
        editor_->document()->rootFrame()->setFrameFormat(format);

        editor_->setStyleSheet(QStringLiteral(
            "QTextEdit#sceneEditor{background:#0b1016;color:#e7e0d6;border:0;padding:0;"
            "selection-background-color:#315b72;selection-color:#ffffff;}"));
    }

    QPointer<QMainWindow> window_;
    QPointer<WritingPage> page_;
    QPointer<SemanticTextEdit> editor_;
    QPointer<QTreeWidget> tree_;
    QTimer commitTimer_;
    QTimer marginTimer_;
};

} // namespace

void installWritingPerformanceController(QMainWindow* window) {
    if (!window || window->property("wbwWritingPerformanceV5").toBool()) return;
    window->setProperty("wbwWritingPerformanceV5", true);
    new ManuscriptGuard(window);
}

} // namespace wbw
