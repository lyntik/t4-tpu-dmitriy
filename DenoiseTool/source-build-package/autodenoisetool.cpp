#include "autodenoisetool.h"

#include <QVariant>

#include "ui_DenoiseControlPanel.h"
#include "denoisewidget.h"

#include "NoWheelEventHandler.h"

AutoDenoiseTool::AutoDenoiseTool()
    : controlPanel_(new Ui::DenoiseControlPanel)
    , controlWidget_(new QWidget())
{
    controlPanel_->setupUi(controlWidget_);
    controlPanel_->denoiseButton->setProperty(
        "toolptr", QVariant::fromValue<IVolumeOperationTool*>(this));
}

AutoDenoiseTool::~AutoDenoiseTool()
{
    delete controlPanel_;
}

void AutoDenoiseTool::init(IApplicationData* appData)
{
    appData_ = appData;
    NoWheelEventHandler::instance()->install(
        { controlPanel_->modelCmb, controlPanel_->cmbBinFormat });
}

void AutoDenoiseTool::setData(SharedImageDataType img)
{
    if (widget_)
        widget_->setData_s(img);
}

QPushButton* AutoDenoiseTool::activateButton()
{
    return controlPanel_->denoiseButton;
}

ILayoutWidget* AutoDenoiseTool::createLayoutWidget()
{
    widget_ = new DenoiseWidget(controlPanel_, appData_);
    return widget_;
}
