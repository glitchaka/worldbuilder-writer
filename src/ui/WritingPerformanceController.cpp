#include "ui/WritingPerformanceController.h"

#include "ui/SemanticTextEdit.h"
#include "ui/WritingPage.h"

#include <QEvent>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
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

        if (QWidget* legacyHero = window_->findChild<QWidget*>(QStringLiteral("writingHero"))) {
            legacyHero->hide();
            legacyHero->setMinimumHeight(0);
            legacyHero->setMaximumHeight(0);
        }

        commitTimer_.setSingleShot(true);
        commitTimer_.setInterval(380);
        marginTimer_.setSingleShot(true);
        marginTimer_.setInterval(60);

        // Persist the manuscript after a short idle period instead of rebuilding the
        // project model, semantic index and scene board for every individual keystroke.
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
        if ((watched == window_ || watched == editor_ || watched == editor_->viewport()) &&
            (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            marginTimer_.start();
        }

        if (watched == window_ && event->type() == QEvent::Close && commitTimer_.isActive()) {
            flushPendingEdit();
        }

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

        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        const int preferredWidth = qBound(520, settings.value(QStringLiteral("editor/lineWidth"), 680).toInt(), 960);
        const int targetTextWidth = qBound(520, qMin(preferredWidth, viewportWidth - 72), 960);
        const int side = qMax(32, (viewportWidth - targetTextWidth) / 2);

        editor_->document()->setDocumentMargin(0.0);
        QTextFrameFormat format = editor_->document()->rootFrame()->frameFormat();
        format.setLeftMargin(side);
        format.setRightMargin(side);
        format.setTopMargin(34);
        format.setBottomMargin(56);
        editor_->document()->rootFrame()->setFrameFormat(format);
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
    if (!window || window->property("wbwWritingPerformanceV8").toBool()) return;
    window->setProperty("wbwWritingPerformanceV8", true);
    new ManuscriptGuard(window);
}

} // namespace wbw
