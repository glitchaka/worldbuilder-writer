#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {

void PilinReyEditor::installStudioStatusBar() {
    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if (!root) return;

    auto* bar = findChild<QFrame*>(QStringLiteral("pilinStudioStatusBar"), Qt::FindDirectChildrenOnly);
    if (!bar) {
        bar = new QFrame(this);
        bar->setObjectName(QStringLiteral("pilinStudioStatusBar"));
        bar->setFixedHeight(36);
        auto* row = new QHBoxLayout(bar);
        row->setContentsMargins(12, 0, 10, 0);
        row->setSpacing(9);

        auto* canvasState = new QLabel(tr("Lienzo cartográfico"), bar);
        canvasState->setObjectName(QStringLiteral("pilinStudioStatusText"));
        row->addWidget(canvasState, 1);

        auto* zoomHint = new QLabel(tr("Rueda · zoom   ·   botón central / Mover · desplazar"), bar);
        zoomHint->setObjectName(QStringLiteral("pilinStudioStatusHint"));
        row->addWidget(zoomHint);

        auto* toolbar = findChild<QFrame*>(QStringLiteral("pilinStudioToolbar"));
        auto* toolbarLayout = toolbar ? qobject_cast<QHBoxLayout*>(toolbar->layout()) : nullptr;
        QToolButton* fit = nullptr;
        if (toolbar) {
            for (QToolButton* button : toolbar->findChildren<QToolButton*>()) {
                if (button && button->toolTip() == tr("Encajar mapa")) {
                    fit = button;
                    break;
                }
            }
        }
        if (fit) {
            if (toolbarLayout) toolbarLayout->removeWidget(fit);
            fit->setParent(bar);
            fit->setObjectName(QStringLiteral("pilinStudioStatusAction"));
            fit->setText(tr("Encajar"));
            row->addWidget(fit);
        }

        if (gridCheck_) {
            if (toolbarLayout) toolbarLayout->removeWidget(gridCheck_);
            gridCheck_->setParent(bar);
            gridCheck_->setObjectName(QStringLiteral("pilinStudioStatusCheck"));
            row->addWidget(gridCheck_);
        }
        if (snapCheck_) {
            if (toolbarLayout) toolbarLayout->removeWidget(snapCheck_);
            snapCheck_->setParent(bar);
            snapCheck_->setObjectName(QStringLiteral("pilinStudioStatusCheck"));
            row->addWidget(snapCheck_);
        }

        root->addWidget(bar);
    }

    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    bar->setStyleSheet(dark ? QStringLiteral(R"QSS(
#pilinStudioStatusBar{background:#0c1117;border:0;border-top:1px solid #26313b;}
#pilinStudioStatusText{background:transparent;color:#8d99a5;border:0;padding:0;font-size:8pt;}
#pilinStudioStatusHint{color:#63717d;font-size:8pt;}
#pilinStudioStatusAction{background:transparent;color:#9ba7b1;border:1px solid transparent;border-radius:6px;padding:4px 8px;}
#pilinStudioStatusAction:hover{background:#17222c;color:#fff;border-color:#30404d;}
#pilinStudioStatusCheck{color:#8f9ba6;spacing:6px;font-size:8pt;}
)QSS") : QStringLiteral(R"QSS(
#pilinStudioStatusBar{background:#f7f4ee;border:0;border-top:1px solid #d3cabd;}
#pilinStudioStatusText{background:transparent;color:#756d64;border:0;padding:0;font-size:8pt;}
#pilinStudioStatusHint{color:#91877b;font-size:8pt;}
#pilinStudioStatusAction{background:transparent;color:#625b52;border:1px solid transparent;border-radius:6px;padding:4px 8px;}
#pilinStudioStatusAction:hover{background:#e8e1d7;color:#29251f;border-color:#c9bdad;}
#pilinStudioStatusCheck{color:#6d655c;spacing:6px;font-size:8pt;}
)QSS"));
}

} // namespace wbw
