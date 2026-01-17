#ifndef SegmentionWidget_H
#define SegmentionWidget_H

#include "ILayoutWidget.h"
#include "GeoTypes.h"
#include "SlicerTypes.h"
#include "IApplicationData.h"
#include "IQVolumeSource.h"
#include "Pair.h"

namespace Ui {
class FiltrationWidget;
class ControlPanel;
}

class QSlider;
class LevelWidget;
class GraphicsView;

class FiltrationWidget : public ILayoutWidget
{
    Q_OBJECT
public:
    explicit FiltrationWidget(Ui::ControlPanel* ctrlPanel, IApplicationData *appData);
    ~FiltrationWidget();

Q_SIGNALS:
    void setData_s(SharedImageDataType img);

protected:
    IApplicationData* appData_;

    void finalize() override;
    void fit() override;

private:
    Ui::FiltrationWidget *ui_;
    Ui::ControlPanel* ctrlPanel_;
    IVolumeSource* volSrc_;
    QTimer *timer_;
    DimensionType dim_;
    bool leaving_;

    QSlider* slider_;

    bool fullSreen_;
    GraphicsView* lastFullScreenView_;

    ShapeROIType roi_;

    void init(double minLevel = DBL_MAX, double maxLevel = DBL_MIN);
    int invY(int slice);
    QString sliceLabel(AxisType ax, int slice);
    void setupFrame(AxisType ax);
    IFilterComponent* prepareFilter(int ind);

    QString filterParamsStr();

private Q_SLOTS:
    void newVolumeOpened();
    void volumeClosed();

    void sliderValueChanged(int);
    void updateSlice();
    void wheelTriggered(bool);

    void algoChanged(int);
    //void filterApplied();
    void testSlice();
    void start();

    void setData(SharedImageDataType img);


};

#endif // SegmentionWidget_H
