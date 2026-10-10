#include "ui/MainWindow.h"
#include "ui/ProductReorganizer.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {

class AppTitleBar final : public QWidget {
public:
    explicit AppTitleBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("appTitleBar"));
        setFixedHeight(30);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
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
        button->setFixedSize(44, 30);
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
    wbw::ThemeManager::applySaved();

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);

    if (auto* root = qobject_cast<QVBoxLayout*>(window.centralWidget()->layout())) {
        root->insertWidget(0, new AppTitleBar(window.centralWidget()));
    }

    wbw::applyProductReorganization(&window);
    wbw::ThemeManager::applySaved();
    window.show();
    QTimer::singleShot(0, [&window]() {
        wbw::applyProductReorganization(&window);
        wbw::ThemeManager::applySaved();
    });
    return app.exec();
}
