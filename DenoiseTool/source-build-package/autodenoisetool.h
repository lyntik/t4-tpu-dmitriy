#ifndef AUTODENOISETOOL_H
#define AUTODENOISETOOL_H

#include <QString>

#include "IVolumeOperationTool.h"
#include "ComponentInterfaces.h"

namespace Ui {
class DenoiseControlPanel;
}

class DenoiseWidget;

class AutoDenoiseTool : public QObject, public IComponent, public IVolumeOperationTool
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "VolumeOperationTool.AutoDenoiseModule")
    Q_INTERFACES(IComponent)

public:
    AutoDenoiseTool();
    ~AutoDenoiseTool();

    void init(IApplicationData* appData);
    QString name() override { return QStringLiteral("AutoDenoise"); }
    QString version() override { return QStringLiteral("1.0"); }

    void setData(SharedImageDataType img) override;

    QPushButton* activateButton() override;
    QWidget* controlWidget() override { return controlWidget_; }
    ILayoutWidget* createLayoutWidget() override;

private:
    IApplicationData* appData_ = nullptr;
    Ui::DenoiseControlPanel* controlPanel_ = nullptr;
    QWidget* controlWidget_ = nullptr;
    DenoiseWidget* widget_ = nullptr;
};

#endif
