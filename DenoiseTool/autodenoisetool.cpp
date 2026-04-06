#include "autodenoisetool.h"
#include "ui_DenoiseTool.h"
#include "denoisewidget.h"

AutoDenoiseTool::AutoDenoiseTool()
    : Panel_(new Ui::DenoiseTool)
    , controlWidget_(new QWidget())
{
    Panel_->setupUi(controlWidget_);
    Panel_->DenoiseBut->setProperty("toolptr", 
        QVariant().fromValue<IVolumeOperationTool*>(this));
}

AutoDenoiseTool::~AutoDenoiseTool()
{
    delete Panel_;
}

void AutoDenoiseTool::init(IApplicationData* appData)
{
    appData_ = appData;
}

QPushButton* AutoDenoiseTool::activateButton()
{
    return Panel_->DenoiseBut;
}

ILayoutWidget* AutoDenoiseTool::createLayoutWidget()
{
    widget_ = new DenoiseWidget(Panel_, appData_);
    return widget_;
}