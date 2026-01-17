#ifndef FiltrationVolOpTool_H
#define FiltrationVolOpTool_H

#include <QString>
#include <QScopedPointer>

#include "IVolumeOperationTool.h"
#include "ComponentInterfaces.h"

namespace Ui {
class ControlPanel;
}
class FiltrationWidget;

class FiltrationVolOpTool: public QObject,
        public IComponent,
        public IVolumeOperationTool
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "VolumeOperationTool.Image2DFiltration")
    Q_INTERFACES(IComponent)
public:
    FiltrationVolOpTool();
    ~FiltrationVolOpTool();

    void init(IApplicationData* appData);

    QString name() { return "Image 2D Filtration Tool"; }
    QString version() override { return "1.0"; }

    void setData(SharedImageDataType img) override;

    QPushButton* activateButton() override;
    QWidget* controlWidget() override { return controlWidget_; }
    ILayoutWidget* createLayoutWidget() override;

private:
    IApplicationData *appData_;
    Ui::ControlPanel* controlPanel_;
    QWidget* controlWidget_;
    QScopedPointer<ILayoutWidget> layoutWidget_;
    FiltrationWidget *widget_;


};

#endif // FiltrationVolOpTool_H


