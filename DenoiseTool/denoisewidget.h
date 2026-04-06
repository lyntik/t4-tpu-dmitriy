#ifndef DENOISEWIDGET_H
#define DENOISEWIDGET_H

#include "ILayoutWidget.h"
#include "IApplicationData.h"
#include "IQVolumeSource.h"

namespace Ui { class DenoiseTool; }

class DenoiseWidget : public ILayoutWidget
{
    Q_OBJECT

public:
    explicit DenoiseWidget(Ui::DenoiseTool* ctrlPanel, IApplicationData* appData);
    ~DenoiseWidget();

protected:
    IApplicationData* appData_;
    void finalize() override;
    void fit() override;

private:
    Ui::DenoiseTool* Panel_;
    IVolumeSource* volSrc_;
    QTimer* timer_;
    QSlider* slider_;
    GraphicsView* view_;
    GraphicsView* viewDenoised_;

    void init();
    void setupFrame();

private Q_SLOTS:
    void newVolumeOpened();
    void volumeClosed();
    void sliderValueChanged(int);
    void updateSlice();
    void startDenoise();
    void testSlice();

    QString buildHydraOverrides(const QString& directory, 
                                const QString& dataset,
                                const QString& modelName,
                                int single_slice);
};

#endif // DENOISEWIDGET_H