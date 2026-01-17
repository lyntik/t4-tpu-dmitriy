#include "FiltrationVolOpTool.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

#include "ui_ControlPanel.h"
#include "FiltrationWidget.h"

#include "NoWheelEventHandler.h"
#include "CloseWindowEventHandler.h"

FiltrationVolOpTool::FiltrationVolOpTool():
    controlPanel_(new Ui::ControlPanel),
    controlWidget_(new QWidget())
{
    controlPanel_->setupUi(controlWidget_);
    controlPanel_->filtrationButton->setProperty("toolptr", QVariant().fromValue<IVolumeOperationTool*>(this));
    controlPanel_->aiWidget->setVisible(false);
    controlPanel_->claheLWidget->setVisible(false);
}

FiltrationVolOpTool::~FiltrationVolOpTool()
{

}

void FiltrationVolOpTool::init(IApplicationData* appData)
{
    appData_ = appData;
    NoWheelEventHandler::instance()->install({ controlPanel_->algoCmb, controlPanel_->aiNetCmb, controlPanel_->cmbBinFormat, controlPanel_->nlmHDblSpin, controlPanel_->nlmTemplateWindowSizeSpn, controlPanel_->nlmSearchWindowSizeSpn, controlPanel_->claheLClipLimitDblSpn, controlPanel_->claheLGridSizeSpn });
}

void FiltrationVolOpTool::setData(SharedImageDataType img)
{
    widget_->setData_s(img);
}

QPushButton* FiltrationVolOpTool::activateButton()
{
    return controlPanel_->filtrationButton;
}

ILayoutWidget* FiltrationVolOpTool::createLayoutWidget()
{
    widget_ = new FiltrationWidget(controlPanel_, appData_);
    return widget_;
}


