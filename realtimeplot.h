#ifndef REALTIMEPLOT_H
#define REALTIMEPLOT_H

#include <QDialog>
#include "qcustomplot.h"
#include <QTimer>
#include <vector>
#include "Vector_3D.h"
using namespace std;



namespace Ui {
class RealTimePlot;
}

class RealTimePlot : public QDialog
{
    Q_OBJECT

public:
    explicit RealTimePlot(QWidget *parent = 0);
    ~RealTimePlot();

    void setupRealtimeData(QCustomPlot *customPlot);
   // void connect_to_sensor();

private slots:
    void realtimeDataSlot_X();
    void realtimeDataSlot_Y();
    void realtimeDataSlot_Z();
    void realtimeDataSlot_XROT();
    void realtimeDataSlot_YROT();
    void getSensorData(const vector<Vector_3D<double> > &sensorData);

protected:
    void closeEvent(QCloseEvent*);



private:
    Ui::RealTimePlot *ui;
    QTimer dataTimer;
    int DataRate;

    // Received tracking data
    vector<double> coll_x, coll_y, coll_z;
    vector<Vector_3D<double>> coll_nvt, coll_tvt;
    double dXPos, dYPos, dZPos, dXRot, dYRot;
};

#endif // REALTIMEPLOT_H
