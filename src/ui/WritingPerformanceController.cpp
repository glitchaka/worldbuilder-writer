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

namespace wbw {
namespace {

class ManuscriptGuard final : public QObject {
public:
    explicit ManuscriptGuard(QMainWindow* window) : QObject(window), window_(window) {
        page_ = window ? window->findChild<WritingPage*>(QStringLiteral("writingPage")) : nullptr;
        editor_ = window ? window->findChild<SemanticTextEdit*>(QStringLiteral("sceneEditor")) : nullptr;
        if (!page_ || !editor_) return;

        commitTimer_.setSingleShot(true);
        commitTimer_.setInterval(260);

        // The legacy connection persisted the complete scene, rescanned semantic references and
        // rebuilt the entire scene board for every character typed. Replace it with one coalesced
        // commit after the user pauses briefly.
        QObject::disconnect(editor_, &QTextEdit::textChanged, page_, &WritingPage::applyScene);
        connect(editor_, &QTextEdit::textChanged, this, [this]() {
            if (!page_) return;
            commitTimer_.start();
        });
        connect(&commitTimer_, &QTimer::timeout, this, [this]() {
            if (page_) page_->applyScene();
        });

        editor_->installEventFilter(this);
        if (editor_->viewport()) editor_->viewport()->installEventFilter(this);
        applyMargins();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)
            QTimer::singleShot(0, this, [this]() { applyMargins(); });
        return QObject::eventFilter(watched, event);
    }

private:
    void applyMargins() {
        if (!editor_ || !editor_->document() || !editor_->document()->rootFrame()) return;

        // Keep the manuscript as a centered reading column. Percentage CSS padding was causing
        // the effective line width to drift as side panels changed size. Frame margins are part of
        // QTextDocument layout itself, so paragraphs now keep a stable readable measure.
        const int viewportWidth = editor_->viewport() ? editor_->viewport()->width() : editor_->width();
        const int targetTextWidth = qBound(620, viewportWidth - 96, 820);
        const int side = qMax(30, (viewportWidth - targetTextWidth) / 2);

        editor_->document()->setDocumentMargin(0.0);
        QTextFrameFormat format = editor_->document()->rootFrame()->frameFormat();
        format.setLeftMargin(side);
        format.setRightMargin(side);
        format.setTopMargin(28);
        format.setBottomMargin(40);
        editor_->document()->rootFrame()->setFrameFormat(format);

        editor_->setStyleSheet(QStringLiteral(
            "QTextEdit#sceneEditor{background:#0b1016;color:#e2ddd3;border:0;padding:0;"
            "selection-background-color:#3b6579;selection-color:#ffffff;}"));
    }

    QPointer<QMainWindow> window_;
    QPointer<WritingPage> page_;
    QPointer<SemanticTextEdit> editor_;
    QTimer commitTimer_;
};

} // namespace

void installWritingPerformanceController(QMainWindow* window) {
    if (!window || window->property("wbwWritingPerformanceV2").toBool()) return;
    window->setProperty("wbwWritingPerformanceV2", true);
    new ManuscriptGuard(window);
}

} // namespace wbw
