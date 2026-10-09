#pragma once

#include <QDialog>
#include <QSize>

class QComboBox;
class QSpinBox;

namespace wbw {

class MapExportDialog final : public QDialog {
    Q_OBJECT
public:
    explicit MapExportDialog(const QSize& documentSize, QWidget* parent = nullptr);

    QString format() const;
    QSize outputSize() const;

private:
    QComboBox* format_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    double aspectRatio_ = 1.0;
    bool syncing_ = false;
};

} // namespace wbw
