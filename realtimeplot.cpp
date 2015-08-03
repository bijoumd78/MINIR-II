#include "realtimeplot.h"
#include "ui_realtimeplot.h"
#include <QtNetwork>
#include <array>

#define PI 3.141592653589793

RealTimePlot::RealTimePlot(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RealTimePlot),
    DataRate(20), // data rate
    dXPos(0.0),
    dYPos(0.0),
    dZPos(0.0),
    dXRot(0.0),
    dYRot(0.0)
{
    ui->setupUi(this);
    setWindowTitle(tr("Graph of sensor data"));

    // give the axes some labels:
    ui->widget->yAxis->setLabel("X_Position [mm]");
    ui->widget_2->yAxis->setLabel("Y_Position [mm]");
    ui->widget_3->yAxis->setLabel("Z_Position [mm]");
    ui->widget_4->yAxis->setLabel("X_Rotation [deg]");
    ui->widget_5->yAxis->setLabel("Y_Rotation [deg]");

    setupRealtimeData(ui->widget);
    ui->widget->replot();
    setupRealtimeData(ui->widget_2);
    ui->widget_2->replot();
    setupRealtimeData(ui->widget_3);
    ui->widget_3->replot();
    setupRealtimeData(ui->widget_4);
    ui->widget_4->replot();
    setupRealtimeData(ui->widget_5);
    ui->widget_5->replot();
}

RealTimePlot::~RealTimePlot()
{
    delete ui;
}

void RealTimePlot::setupRealtimeData(QCustomPlot *customPlot)
{
    // include this section to fully disable antialiasing for higher performance:
    customPlot->setNotAntialiasedElements(QCP::aeAll);
    QFont font;
    font.setStyleStrategy(QFont::NoAntialias);
    customPlot->xAxis->setTickLabelFont(font);
    customPlot->yAxis->setTickLabelFont(font);
    customPlot->legend->setFont(font);

    customPlot->addGraph(); // blue line
    customPlot->graph(0)->setPen(QPen(Qt::green, 2));
    //customPlot->graph(0)->setBrush(QBrush(QColor(240, 255, 200)));
    customPlot->graph(0)->setAntialiasedFill(false);

    customPlot->addGraph(); // blue dot
    customPlot->graph(1)->setPen(QPen(Qt::green));
    customPlot->graph(1)->setLineStyle(QCPGraph::lsNone);
    customPlot->graph(1)->setScatterStyle(QCPScatterStyle::ssDisc);

    customPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    customPlot->yAxis->setBasePen(QPen(Qt::white, 1));
    customPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    customPlot->yAxis->setTickPen(QPen(Qt::white, 1));
    customPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    customPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
    customPlot->xAxis->setTickLabelColor(Qt::white);
    customPlot->yAxis->setTickLabelColor(Qt::white);

    customPlot->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    customPlot->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    customPlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    customPlot->xAxis->setDateTimeFormat("hh:mm:ss");
    customPlot->xAxis->setAutoTickStep(false);
    customPlot->xAxis->setTickStep(2);
    customPlot->yAxis->setLabelColor(Qt::white);
    customPlot->setBackground(Qt::black);

    // Make left and bottom axes transfer their ranges to right and top axes:
    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), customPlot->yAxis2, SLOT(setRange(QCPRange)));

    // Setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot_X()));
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot_Y()));
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot_Z()));
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot_XROT()));
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot_YROT()));
    dataTimer.start(0); // Interval 0 means to refresh as fast as possible
}

void RealTimePlot::realtimeDataSlot_X()
{
    // calculate two new data points:
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;

    static double lastPointKey = 0;
    if (key-lastPointKey > 0.01) // at most add point every 10 ms
    {
        //dXPos = qSin(key);
        double value0 = dXPos;  /*qSin(key);*/

        // Alarm
        /*if (value0 > 5 || value0 < -5)
            Beep(523,500); // 523 hertz (C5) for 500 milliseconds*/

        // add data to lines:
        ui->widget->graph(0)->addData(key, value0);
        // set data of dots:
        ui->widget->graph(1)->clearData();
        ui->widget->graph(1)->addData(key, value0);
        // remove data of lines that's outside visible range:
        ui->widget->graph(0)->removeDataBefore(key - DataRate);
        // rescale value (vertical) axis to fit the current data:
        ui->widget->graph(0)->rescaleValueAxis();
        ui->widget->graph(1)->rescaleValueAxis(true);
        lastPointKey = key;
    }
    // make key axis range scroll with the data (at a constant range size of DataRate):
    ui->widget->xAxis->setRange(key+0.25, DataRate, Qt::AlignRight);
    ui->widget->replot();
}

void RealTimePlot::realtimeDataSlot_Y()
{
    // calculate two new data points:
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;

    static double lastPointKey = 0;
    if (key-lastPointKey > 0.01) // at most add point every 10 ms
    {
        //dYPos = qCos(key);
        double value0 = dYPos;

        // Alarm
        /* if (value0 > 5 || value0 < -5)
            Beep(523,500); // 523 hertz (C5) for 500 milliseconds*/

        // add data to lines:
        ui->widget_2->graph(0)->addData(key, value0);
        // set data of dots:
        ui->widget_2->graph(1)->clearData();
        ui->widget_2->graph(1)->addData(key, value0);
        // remove data of lines that's outside visible range:
        ui->widget_2->graph(0)->removeDataBefore(key - DataRate);
        // rescale value (vertical) axis to fit the current data:
        ui->widget_2->graph(0)->rescaleValueAxis();
        ui->widget_2->graph(1)->rescaleValueAxis(true);
        lastPointKey = key;
    }
    // make key axis range scroll with the data (at a constant range size of DataRate):
    ui->widget_2->xAxis->setRange(key+0.25, DataRate, Qt::AlignRight);
    ui->widget_2->replot();
}

void RealTimePlot::realtimeDataSlot_Z()
{
    // calculate two new data points:
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;

    static double lastPointKey = 0;
    if (key-lastPointKey > 0.01) // at most add point every 10 ms
    {
        // dZPos = qTan(key);
        double value0 = dZPos;

        // Alarm
        /* if (value0 > 5 || value0 < -5)
            Beep(523,500); // 523 hertz (C5) for 500 milliseconds*/

        // add data to lines:
        ui->widget_3->graph(0)->addData(key, value0);
        // set data of dots:
        ui->widget_3->graph(1)->clearData();
        ui->widget_3->graph(1)->addData(key, value0);
        // remove data of lines that's outside visible range:
        ui->widget_3->graph(0)->removeDataBefore(key - DataRate);
        // rescale value (vertical) axis to fit the current data:
        ui->widget_3->graph(0)->rescaleValueAxis();
        ui->widget_3->graph(1)->rescaleValueAxis(true);
        lastPointKey = key;
    }
    // make key axis range scroll with the data (at a constant range size of DataRate):
    ui->widget_3->xAxis->setRange(key+0.25, DataRate, Qt::AlignRight);
    ui->widget_3->replot();
}

void RealTimePlot::realtimeDataSlot_XROT()
{
    // calculate two new data points:
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;

    static double lastPointKey = 0;
    if (key-lastPointKey > 0.01) // at most add point every 10 ms
    {
        //dXRot = qSin(key)+ 5*qCos(key);
        double value0 = dXRot;

        /* if (value0 > 5 || value0 < -5)
            Beep(523,500); // 523 hertz (C5) for 500 milliseconds*/

        // add data to lines:
        ui->widget_4->graph(0)->addData(key, value0);
        // set data of dots:
        ui->widget_4->graph(1)->clearData();
        ui->widget_4->graph(1)->addData(key, value0);
        // remove data of lines that's outside visible range:
        ui->widget_4->graph(0)->removeDataBefore(key - DataRate);
        // rescale value (vertical) axis to fit the current data:
        ui->widget_4->graph(0)->rescaleValueAxis();
        ui->widget_4->graph(1)->rescaleValueAxis(true);
        lastPointKey = key;
    }
    // make key axis range scroll with the data (at a constant range size of DataRate):
    ui->widget_4->xAxis->setRange(key+0.25, DataRate, Qt::AlignRight);
    ui->widget_4->replot();
}

void RealTimePlot::realtimeDataSlot_YROT()
{
    // calculate two new data points:
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;

    static double lastPointKey = 0;
    if (key-lastPointKey > 0.01) // at most add point every 10 ms
    {
        //dYRot = qSin(key) - 5*qCos(key);
        double value0 = dYRot;

        // Alarm
        /* if (value0 > 5 || value0 < -5)
            Beep(523,500); // 523 hertz (C5) for 500 milliseconds*/

        // add data to lines:
        ui->widget_5->graph(0)->addData(key, value0);
        // set data of dots:
        ui->widget_5->graph(1)->clearData();
        ui->widget_5->graph(1)->addData(key, value0);
        // remove data of lines that's outside visible range:
        ui->widget_5->graph(0)->removeDataBefore(key - DataRate);
        // rescale value (vertical) axis to fit the current data:
        ui->widget_5->graph(0)->rescaleValueAxis();
        ui->widget_5->graph(1)->rescaleValueAxis(true);
        lastPointKey = key;
    }
    // make key axis range scroll with the data (at a constant range size of DataRate):
    ui->widget_5->xAxis->setRange(key+0.25, DataRate, Qt::AlignRight);
    ui->widget_5->replot();

}

void RealTimePlot::getSensorData(const vector<Vector_3D<double> > &sensorData)
{
    coll_x.push_back(sensorData[0].get_x());
    coll_y.push_back(sensorData[0].get_y());
    coll_z.push_back(sensorData[0].get_z());

    coll_nvt.push_back(Vector_3D<double>(sensorData[1].get_x(), sensorData[1].get_y(), sensorData[1].get_z()));
    coll_tvt.push_back(Vector_3D<double>(sensorData[2].get_x(), sensorData[2].get_y(), sensorData[2].get_z()));

    // Compute the relative position
    dXPos = coll_x.front() - coll_x.back();
    dYPos = coll_y.front() - coll_y.back();
    dZPos = coll_z.front() - coll_z.back();

    Vector_3D<double> a(0,0,0), b(0,0,0), c(0,0,0);
    a = coll_nvt.front().crossproduct( coll_nvt.back() );
    b = coll_tvt.front().crossproduct( coll_tvt.back() );
    c = coll_nvt.front().crossproduct( coll_tvt.front() );


    // Compute the rotation angles (FYI: acos method is unstable)
    double angle_X = atan2( sqrt( pow( a.get_x(), 2 ) + pow( a.get_y(), 2 ) + pow( a.get_z(), 2 ) ),  coll_nvt.front().dotproduct( coll_nvt.back() ) );
    c.dotproduct( a ) < 0    ?  dXRot = (-180/PI) * angle_X  :  dXRot =  (180/PI) * angle_X; // Compute the sign of the angle

    double angle_Y = atan2( sqrt( pow( b.get_x(), 2 ) + pow( b.get_y(), 2 ) + pow( b.get_z(), 2 ) ),  coll_tvt.front().dotproduct( coll_tvt.back() ) );
    coll_nvt.front().dotproduct( b ) < 0  ?  dYRot = (-180/PI) * angle_Y  :  dYRot =  (180/PI) * angle_Y ; // Compute the sign of the angle
}

void RealTimePlot::closeEvent(QCloseEvent *)
{
    coll_x.clear();
    coll_y.clear();
    coll_z.clear();
    coll_nvt.clear();
    coll_tvt.clear();
}
