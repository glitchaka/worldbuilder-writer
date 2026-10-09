#include "ui/MapExportDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace wbw {

MapExportDialog::MapExportDialog(const QSize& documentSize, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Exportar mapa"));
    setModal(true);
    setMinimumWidth(360);

    const int documentWidth = qMax(1, documentSize.width());
    const int documentHeight = qMax(1, documentSize.height());
    aspectRatio_ = static_cast<double>(documentWidth) / static_cast<double>(documentHeight);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(14);

    auto* description = new QLabel(tr("La exportación usa las coordenadas lógicas completas del mapa, no el tamaño visible del editor."));
    description->setWordWrap(true);
    description->setStyleSheet(QStringLiteral("color:#667085;"));
    root->addWidget(description);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    format_ = new QComboBox;
    format_->addItem(tr("PNG — imagen raster"), QStringLiteral("png"));
    format_->addItem(tr("SVG — vector editable"), QStringLiteral("svg"));
    format_->addItem(tr("PDF — documento"), QStringLiteral("pdf"));

    width_ = new QSpinBox;
    height_ = new QSpinBox;
    width_->setRange(256, 20000);
    height_->setRange(256, 20000);
    width_->setSingleStep(256);
    height_->setSingleStep(144);
    width_->setSuffix(tr(" px"));
    height_->setSuffix(tr(" px"));

    const int initialWidth = qBound(256, documentWidth, 20000);
    const int initialHeight = qBound(256, documentHeight, 20000);
    width_->setValue(initialWidth);
    height_->setValue(initialHeight);

    form->addRow(tr("Formato"), format_);
    form->addRow(tr("Ancho"), width_);
    form->addRow(tr("Alto"), height_);
    root->addLayout(form);

    auto* ratioHint = new QLabel(tr("Al cambiar una dimensión se conserva la proporción del documento."));
    ratioHint->setStyleSheet(QStringLiteral("color:#667085;font-size:9pt;"));
    root->addWidget(ratioHint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Exportar"));
    root->addWidget(buttons);

    connect(width_, &QSpinBox::valueChanged, this, [this](int value) {
        if (syncing_) return;
        syncing_ = true;
        height_->setValue(qBound(256, qRound(value / aspectRatio_), 20000));
        syncing_ = false;
    });
    connect(height_, &QSpinBox::valueChanged, this, [this](int value) {
        if (syncing_) return;
        syncing_ = true;
        width_->setValue(qBound(256, qRound(value * aspectRatio_), 20000));
        syncing_ = false;
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString MapExportDialog::format() const {
    return format_->currentData().toString();
}

QSize MapExportDialog::outputSize() const {
    return QSize(width_->value(), height_->value());
}

} // namespace wbw
