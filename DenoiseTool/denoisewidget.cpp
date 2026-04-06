#include "denoisewidget.h"
#include "ui_DenoiseTool.h"
#include "t4.h"
#include "BGTaskProgressDialog.h"
#include <QTimer>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>

DenoiseWidget::DenoiseWidget(Ui::DenoiseTool* Panel, IApplicationData* appData)
    : Panel_(Panel)
    , appData_(appData)
{
    Panel_->gridWidget->setEnabled(true);

    volSrc_ = appData->volumeSource2();
    IQVolumeSource* qvolSrc = dynamic_cast<IQVolumeSource*>(volSrc_);

    connect(qvolSrc, &IQVolumeSource::newVolumeOpened, 
            this, &DenoiseWidget::newVolumeOpened);
    connect(qvolSrc, &IQVolumeSource::volumeClosed, 
            this, &DenoiseWidget::volumeClosed);

    connect(Panel_->StartBut, &QPushButton::clicked, 
            this, &DenoiseWidget::startDenoise);
    connect(Panel_->testSliceButton, &QPushButton::clicked, 
            this, &DenoiseWidget::testSlice);
    connect(Panel_->Slider, &QSlider::valueChanged, 
            this, &DenoiseWidget::sliderValueChanged);

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &DenoiseWidget::updateSlice);
    timer_->setInterval(100);

    init();
}

DenoiseWidget::~DenoiseWidget()
{
    Panel_->gridWidget->setEnabled(false);
}

void DenoiseWidget::init()
{
    if (!volSrc_->isLoaded()) return;

    timer_->setInterval(-1);
    ui->Slider->setMaximum(volSrc_->dim().y() - 1);
    ui->Slider->setValue(volSrc_->currentSliceY());
    timer_->setInterval(100);

    setupFrame();
}

void DenoiseWidget::setupFrame()
{
    SharedImageDataType frame = volSrc_->getYSlice(ui->Slider->value());
    ui->Frame->setFrame(frame);
    ui->FrameDenoised->clear();
    ui->Label->setText(QString::number(ui->Slider->value()));
}

void DenoiseWidget::sliderValueChanged(int)
{
    if (timer_->interval() > 0) timer_->start();
}

void DenoiseWidget::updateSlice()
{
    setupFrame();
    timer_->stop();
}

void DenoiseWidget::newVolumeOpened()
{
    init();
}

void DenoiseWidget::volumeClosed()
{
    ui->Frame->clear();
    ui->FrameDenoised->clear();
}

QString DenoiseWidget::buildHydraOverrides(const QString& directory,
                                           const QString& dataset,
                                           const QString& modelName,
                                           int single_slice)
{
    QStringList overrides;
    overrides << "dataset_data_dir='" + directory + "'";
    overrides << "dataset_dst='" + dataset + "'";
    overrides << "model_name='" + modelName + "'";
    overrides << "single_slice=" + QString::number(single_slice);

    return overrides.join(",");
}

void DenoiseWidget::startDenoise()
{
    QDir dirPath = QFileInfo(volSrc_->fileName()).dir();
    QString directoryName = dirPath.dirName();
    QString parentPath = QDir::cleanPath(dirPath.absoluteFilePath(".."));

    QString overrides = buildHydraOverrides(
        parentPath,
        directoryName,
        Panel_->SelectModel->currentText(),
        -1  // full volume
    );

    BGTaskProgressDialog dlg(this);
    dlg.setMaximumProgress(1000000);
    dlg.setFunction([&]() {
        dlg.updateProgress(999999);
        dlg.updateStatusText(tr("Денoйзинг данных"));

        QString pythonScript = QSettings().value("PTH/PythonSystemPath").toString() 
                             + "/analytics/autoden/denoise.py";

        bool error;
        appData_->pyThread()->executeInterpreter(pythonScript + "|" + overrides, true, &error);
    });
    dlg.start();
    dlg.exec();
}

void DenoiseWidget::testSlice()
{
    QDir dirPath = QFileInfo(volSrc_->fileName()).dir();
    QString directoryName = dirPath.dirName();
    QString parentPath = QDir::cleanPath(dirPath.absoluteFilePath(".."));

    QString overrides = buildHydraOverrides(
        parentPath,
        directoryName,
        Panel_->SelectModel->currentText(),
        ui->Slider->value()  // current slice
    );

    BGTaskProgressDialog dlg(this);
    dlg.setMaximumProgress(1000000);
    dlg.setFunction([&]() {
        dlg.updateProgress(999999);
        dlg.updateStatusText(tr("Тестирование среза"));

        QString pythonScript = QSettings().value("PTH/PythonSystemPath").toString() 
                             + "/analytics/autoden/denoise.py";

        bool error;
        appData_->pyThread()->executeInterpreter(pythonScript + "|" + overrides, true, &error);
    });
    dlg.start();
    dlg.exec();
}

void DenoiseWidget::finalize()
{
    // Cleanup
}

void DenoiseWidget::fit()
{
    ui->Frame->fit();
    ui->FrameDenoised->fit();
}