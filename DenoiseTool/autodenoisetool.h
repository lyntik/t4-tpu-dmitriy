#ifndef AUTODENOISETOOL_H
#define AUTODENOISETOOL_H

#include <QString>
#include <QScopedPointer>
#include "IVolumeOperationTool.h"
#include "ComponentInterfaces.h"

namespace Ui { class DenoiseTool; }
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
    QString name() { return "AutoDenoise"; }
    QString version() override { return "1.0"; }

    QPushButton* activateButton() override;
    QWidget* controlWidget() override { return controlWidget_; }
    ILayoutWidget* createLayoutWidget() override;

private:
    IApplicationData* appData_;
    Ui::DenoiseTool* Panel_;
    QWidget* controlWidget_;
    DenoiseWidget* widget_;
};

#endif // AUTODENOISETOOL_H