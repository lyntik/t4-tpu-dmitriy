#ifndef DENOISEWIDGET_H
#define DENOISEWIDGET_H

#include <cfloat>

#include "ILayoutWidget.h"
#include "IApplicationData.h"
#include "IQVolumeSource.h"

namespace Ui {
class DenoiseControlPanel;
class DenoiseWidget;
}

class DenoiseWidget : public ILayoutWidget
{
    Q_OBJECT

public:
    explicit DenoiseWidget(Ui::DenoiseControlPanel* ctrlPanel, IApplicationData* appData);
    ~DenoiseWidget() override;

    void setHostImage(SharedImageDataType img);

protected:
    IApplicationData* appData_ = nullptr;

    void finalize() override;
    void fit() override;

private:
    Ui::DenoiseWidget* ui_ = nullptr;
    Ui::DenoiseControlPanel* ctrlPanel_ = nullptr;
    IVolumeSource* volSrc_ = nullptr;
    QTimer* timer_ = nullptr;
    bool leaving_ = false;

    void init(double minLevel = DBL_MAX, double maxLevel = DBL_MIN);
    QString sliceLabel(int slice);
    void setupFrame();
    QString buildPythonArgs(
        int singleSlice,
        const QString& filtpathAbsolute,
        const QString& progressPathAbsolute = QString());
    QString pythonScriptPath() const;
    QString filtrationScriptPath() const;
    QString buildInterpreterCommand(const QString& args) const;

private Q_SLOTS:
    void newVolumeOpened();
    void volumeClosed();

    void sliderValueChanged(int);
    void updateSlice();

    void testSlice();
    void startFullVolume();
    void selectOutDir();

    void setData(SharedImageDataType img);
};

#endif
