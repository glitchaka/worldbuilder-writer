#include "ui/MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

class AppTitleBar final : public QWidget {
public:
    explicit AppTitleBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("appTitleBar"));
        setFixedHeight(34);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 0, 0, 0);
        layout->setSpacing(0);

        auto* mark = new QLabel(QStringLiteral("WW"));
        mark->setObjectName(QStringLiteral("titleMark"));
        mark->setFixedWidth(30);
        auto* title = new QLabel(QStringLiteral("Worldbuilder Writer"));
        title->setObjectName(QStringLiteral("titleText"));
        layout->addWidget(mark);
        layout->addWidget(title);
        layout->addStretch(1);

        auto* minimize = windowButton(QStringLiteral("—"), QStringLiteral("windowMinimize"));
        auto* maximize = windowButton(QStringLiteral("□"), QStringLiteral("windowMaximize"));
        auto* close = windowButton(QStringLiteral("×"), QStringLiteral("windowClose"));
        layout->addWidget(minimize);
        layout->addWidget(maximize);
        layout->addWidget(close);

        connect(minimize, &QPushButton::clicked, this, [this]() {
            if (window()) window()->showMinimized();
        });
        connect(maximize, &QPushButton::clicked, this, [this]() {
            if (!window()) return;
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
        });
        connect(close, &QPushButton::clicked, this, [this]() {
            if (window()) window()->close();
        });
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window() && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window()) {
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    QPushButton* windowButton(const QString& text, const QString& name) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(name);
        button->setFixedSize(46, 34);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    }
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));

    app.setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#edf1f5")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#1f2937")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f7f9fc")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#1f2937")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#1f2937")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#344054")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#b42318")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#dceafe")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#175cd3")));
    app.setPalette(palette);

    app.setStyleSheet(QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9.5pt;color:#1f2937;}"
        "QPushButton,QToolButton{background:#ffffff;color:#344054;border:1px solid #cfd6df;border-radius:2px;padding:6px 10px;min-height:18px;}"
        "QPushButton:hover,QToolButton:hover{background:#f7f9fc;border-color:#98a2b3;}"
        "QPushButton:pressed,QToolButton:pressed{background:#eef2f7;}"
        "QToolButton:checked{background:#1668d4;color:#ffffff;border-color:#1668d4;}"
        "QLineEdit,QTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget{background:#ffffff;color:#1f2937;border:1px solid #cfd6df;border-radius:2px;padding:6px 8px;}"
        "QComboBox{padding-right:10px;}"
        "QComboBox::drop-down{width:0;border:0;}"
        "QComboBox::down-arrow{image:none;width:0;height:0;}"
        "QSpinBox,QDoubleSpinBox{padding-right:8px;}"
        "QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;background:transparent;}"
        "QSlider::groove:horizontal{height:4px;background:#d8dee8;border-radius:2px;}"
        "QSlider::handle:horizontal{width:14px;margin:-5px 0;background:#1668d4;border:1px solid #0f5fc8;border-radius:7px;}"
        "QCheckBox{spacing:7px;}"
        "QCheckBox::indicator{width:14px;height:14px;border:1px solid #aab4c0;background:#ffffff;border-radius:2px;}"
        "QCheckBox::indicator:checked{background:#1668d4;border-color:#1668d4;}"
        "#appTitleBar{background:#ffffff;border-bottom:1px solid #d8dee8;}"
        "#titleMark{font-family:'Georgia';font-weight:700;color:#175cd3;}"
        "#titleText{font-weight:600;color:#344054;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;border-radius:0;background:transparent;padding:0;min-height:0;color:#475467;font-size:12pt;}"
        "#windowMinimize:hover,#windowMaximize:hover{background:#eef2f7;}"
        "#windowClose:hover{background:#c42b1c;color:#ffffff;}"
    ));

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);

    if (auto* root = qobject_cast<QVBoxLayout*>(window.centralWidget()->layout())) {
        root->insertWidget(0, new AppTitleBar(window.centralWidget()));
    }

    window.show();
    return app.exec();
}
