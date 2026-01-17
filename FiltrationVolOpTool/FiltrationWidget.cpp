#include "FiltrationWidget.h"
#include "t4.h"
#include "ui_FiltrationWidget.h"
#include "ui_ControlPanel.h"
#include "BGTaskProgressDialog.h"
#include "StringConstants.h"

#include "geo/LevelWidget.h"
#include "MathFuncs.h"
#include "VolumeUtils.h"

#include <QTimer>
#include <QPalette>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>

FiltrationWidget::FiltrationWidget(Ui::ControlPanel* ctrlPanel, IApplicationData *appData):
    ui_(new Ui::FiltrationWidget),
    ctrlPanel_(ctrlPanel),
    appData_(appData),
    fullSreen_(false),
    lastFullScreenView_(nullptr),
    leaving_(false)
{
    ui_->setupUi(this);
    ui_->YFrame->setAppData(appData_);
    ui_->YFrameFiltered->setAppData(appData_);

    volSrc_ = appData->volumeSource2();

    QPalette palette = ui_->YFrame->palette();
    QBrush brush1(QColor(0, 0, 0, 255));
    brush1.setStyle(Qt::SolidPattern);
    palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
    palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
    ui_->YFrame->setPalette(palette);
    ui_->YFrameFiltered->setPalette(palette);

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &FiltrationWidget::updateSlice);
    timer_->setInterval(100);

    IQVolumeSource *qvolSrc = dynamic_cast<IQVolumeSource*> (volSrc_);
    connect(qvolSrc, &IQVolumeSource::newVolumeOpened, this, &FiltrationWidget::newVolumeOpened);
    connect(qvolSrc, &IQVolumeSource::volumeClosed, this, &FiltrationWidget::volumeClosed);

    connect(ctrlPanel_->testSliceButton, &QPushButton::clicked, this, &FiltrationWidget::testSlice);
    connect(ctrlPanel_->startButton, &QPushButton::clicked, this, &FiltrationWidget::start);
    connect(ctrlPanel_->algoCmb, SIGNAL(currentIndexChanged(int)), this, SLOT(algoChanged(int)));

    connect(ui_->YSlider, &QSlider::valueChanged, this, &FiltrationWidget::sliderValueChanged);

    connect(this, &FiltrationWidget::setData_s, this, &FiltrationWidget::setData);

    ctrlPanel_->aiNetCmb->clear();
    QDir dir("ai-models/denoise");
    QStringList files = dir.entryList(QStringList() << "*.pt", QDir::Files, QDir::NoSort);
    Q_FOREACH(const QString& model, files)
        ctrlPanel_->aiNetCmb->addItem(model);

    // currently hardcoded
    // TODO: dynamic plug in, get params widget from filter plugin
    ctrlPanel_->algoCmb->clear();
    if (appData_->pluginByIID("Filters.NonLocalMeans")) ctrlPanel_->algoCmb->addItem(tr("Нелокальное среднее"), 0);
    ctrlPanel_->algoCmb->addItem(tr("Шумоподавление ИИ"), 1);
    if (appData_->pluginByIID("Filters.CLAHE-L")) ctrlPanel_->algoCmb->addItem(tr("Усиление контраста CLAHE-L"), 2);
    if (appData_->pluginByIID("Filters.Sharpen")) ctrlPanel_->algoCmb->addItem(tr("Повышение резкости"), 3);

    ctrlPanel_->filtrationControl->setEnabled(true);
    if (ctrlPanel_->algoCmb->count())
        algoChanged(ctrlPanel_->algoCmb->currentIndex());

    init();
}

FiltrationWidget::~FiltrationWidget()
{
    delete ui_;
    ctrlPanel_->filtrationControl->setEnabled(false);
}

void FiltrationWidget::init(double minLevel, double maxLevel)
{
    ctrlPanel_->filtrationControl->setEnabled(volSrc_->isLoaded());
    if (!volSrc_->isLoaded() || leaving_) return;
    dim_ = volSrc_->dim();

    QVector<double> data;
    SharedImageDataType frame = volSrc_->getYSlice(dim_.y() / 2, true);

    double min, max;
    frame->minMaxValues(&min, &max);
    if (minLevel == DBL_MAX)
    {
        minLevel = min;
        maxLevel = max;
    }
    ui_->YFrame->setLevel(min, max);
    LevelWidget* levelWidget = ui_->YFrame->levelWidget();
    bool visible = levelWidget->isVisible();
    levelWidget->clear();
    levelWidget->setFrame(frame);
    levelWidget->updateLevel(minLevel, maxLevel);
    if (visible) levelWidget->show();

    timer_->setInterval(-1);
    ui_->YSlider->setMaximum(dim_.y() - 1);
    ui_->YSlider->setValue(volSrc_->currentSliceY());
    timer_->setInterval(100);

    setupFrame(AllAxes);
}

void FiltrationWidget::finalize()
{
    leaving_ = true;

    volSrc_->setFiltering(-1);
}

int FiltrationWidget::invY(int slice)
{
    return (volSrc_->yAxisInverse() ? dim_.y()-1-slice : slice);
}

QString FiltrationWidget::sliceLabel(AxisType ax, int slice)
{
    int labelType = QSettings().value(GeoGroupLine + GeoSliceLabelType).toInt();
    if (labelType == 0) return QString::number(slice);

    int offset = 0;
    if (ax == XAxis) offset = -dim_.x()/2;
    else if (ax == ZAxis) offset = -dim_.z()/2;
    QString mm = QString::number((slice+offset)*volSrc_->spacing0(), 'f', 1) + tr(" мм");
    return QString::number((slice+offset)*volSrc_->spacing0(), 'f', 1) + tr(" мм") + (labelType == 2 ? (" [" + QString::number(slice) + "]") : "");
}

void FiltrationWidget::setupFrame(AxisType ax)
{
    //if (ax == YAxis)
    {
        SharedImageDataType frame = volSrc_->getYSlice(ui_->YSlider->value());
        ui_->YFrame->setFrame(frame);
        ui_->YLabel->setText(sliceLabel(YAxis, ui_->YSlider->value()));

        ui_->YFrameFiltered->clear();
    }
}

void FiltrationWidget::sliderValueChanged(int )
{
    slider_ = dynamic_cast<QSlider*> (sender());
    if (timer_->interval() > 0)
        timer_->start();
}

void FiltrationWidget::updateSlice()
{
    setupFrame(YAxis);
    timer_->stop();
}

void FiltrationWidget::fit()
{
    ui_->YFrame->fit(); ui_->YFrameFiltered->fit();
    ui_->YFrame->fit(); ui_->YFrameFiltered->fit();
}

void FiltrationWidget::newVolumeOpened()
{
    init();
}

void FiltrationWidget::volumeClosed()
{
    ui_->YFrame->clear();
    ui_->YFrameFiltered->clear();

}

void FiltrationWidget::wheelTriggered(bool up)
{
    if (QObject::sender() == ui_->YFrame)
        ui_->YSlider->setValue(ui_->YSlider->value() + (up ? 1 : -1));
}

void FiltrationWidget::algoChanged(int ind)
{
    int i = 0;
    Q_FOREACH(QObject* o, ctrlPanel_->groupBox->children())
    {
        QWidget *widget = dynamic_cast<QWidget*> (o);
        if (widget)
            widget->setVisible(ind == i++);
    }
}

QString FiltrationWidget::filterParamsStr()
{
    QString str;
    switch (ctrlPanel_->algoCmb->itemData(ctrlPanel_->algoCmb->currentIndex()).toInt())
    {
        case 0: str = "nlm:" + QString::number(ctrlPanel_->nlmHDblSpin->value(), 'f', 1) + ":" + QString::number(ctrlPanel_->nlmTemplateWindowSizeSpn->value()) + ":" + QString::number(ctrlPanel_->nlmSearchWindowSizeSpn->value()); break;
        case 1: str = "ai:" + ctrlPanel_->aiNetCmb->currentText(); break;
    }
    return str;
}

IFilterComponent* FiltrationWidget::prepareFilter(int ind)
{
    switch (ind)
    {
        case 0:
        {
            IComponent *c = appData_->pluginByIID("Filters.NonLocalMeans");
            if (!c) return nullptr;
            IFilterComponent * filter = dynamic_cast<IFilterComponent * > (c);
            if (!filter) return nullptr;
            filter->setParam("H", ctrlPanel_->nlmHDblSpin->value());
            filter->setParam("TemplateWindowSize", ctrlPanel_->nlmTemplateWindowSizeSpn->value());
            filter->setParam("SearchWindowSize", ctrlPanel_->nlmSearchWindowSizeSpn->value());
            return filter;
        } break;
        case 2:
        {
            IComponent *c = appData_->pluginByIID("Filters.CLAHE-L");
            if (!c) return nullptr;
            IFilterComponent * filter = dynamic_cast<IFilterComponent * > (c);
            if (!filter) return nullptr;
            filter->setParam("ClipLimit", ctrlPanel_->claheLClipLimitDblSpn->value());
            filter->setParam("GridSize", ctrlPanel_->claheLGridSizeSpn->value());
            return filter;
        } break;
        case 3:
        {
            IComponent *c = appData_->pluginByIID("Filters.Sharpen");
            if (!c) return nullptr;
            IFilterComponent * filter = dynamic_cast<IFilterComponent * > (c);
            if (!filter) return nullptr;
            return filter;
        } break;
    }

    return nullptr;
}

void FiltrationWidget::testSlice()
{
    if (ctrlPanel_->algoCmb->currentIndex() == -1) return;
    int ind = ctrlPanel_->algoCmb->itemData(ctrlPanel_->algoCmb->currentIndex()).toInt();
    BGTaskProgressDialog dlg(this);
    dlg.setMaximumProgress(1000000);
    dlg.setFunction(
        [&]
        ()
        {
            dlg.updateProgress(999999);
            dlg.updateStatusText(tr("Фильтрация...") );

            if (ind == 1)
                appData_->pyThread()->executeInterpreter(QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString() + "/analytics/filtration.py|sliceNumber=" + QString::number(ui_->YSlider->value()) +
                                                     ",filterParams='" + filterParamsStr() + "'", true);
            else if (ind == 0 || ind == 2 || ind == 3)
            {
                SharedImageDataType frame = volSrc_->getYSlice(ui_->YSlider->value())->deepCopy();
                IFilterComponent* filter = prepareFilter(ind);
                if (filter)
                {
                    filter->update(frame);
                    setData_s(frame);
                }
            }
        }
    );
    dlg.start();
    dlg.exec();
}

void FiltrationWidget::start()
{
    if (ctrlPanel_->algoCmb->currentIndex() == -1) return;
    int ind = ctrlPanel_->algoCmb->itemData(ctrlPanel_->algoCmb->currentIndex()).toInt();
    QString dir = ctrlPanel_->segDirEdt->text();
    if (QFileInfo(dir).isRelative())
        dir = QFileInfo(volSrc_->fileName()).absolutePath() + "/" + dir;

    if (QDir(dir).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).count() != 0)
    {
        if (QMessageBox::question(this, tr("Подтверждение"), tr("Директория для сохранения результатов содержит данные. Для запуска процесса её необходимо очистить. Продолжить ?")) != QMessageBox::Yes)
            return;
        QDir(dir).removeRecursively();
    }
    QDir().mkpath(dir);

    BGTaskProgressDialog dlg(this);
    dlg.setMaximumProgress(1000000);
    dlg.setFunction(
        [&]
        ()
        {
            dlg.updateProgress(999999);
            dlg.updateStatusText(tr("Фильтрация... (объём)") );

            if (ind == 1)
                appData_->pyThread()->executeInterpreter(QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString() + "/analytics/filtration.py|sliceNumber=-1,filterParams='" + filterParamsStr() + "',filtpath='" + dir + "'", true);
            else if (ind == 0 || ind == 2 || ind == 3)
                t4::Utils::save2DFilteredVolume(volSrc_, prepareFilter(ind), -1, dir);
        }
    );
    dlg.start();
    dlg.exec();
}

void FiltrationWidget::setData(SharedImageDataType img)
{
    img->initMinMax();
    ui_->YFrameFiltered->setFrame(img, false);
}


