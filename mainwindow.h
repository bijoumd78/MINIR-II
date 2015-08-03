#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "gdcmImageReader.h"
#include <QImage>
#include <QImageWriter>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileSystemWatcher>
#include <QDebug>
#include "ReadRawData.h"
#include <QTcpSocket>
#include <utility>
#include <tuple>
#include <QFile>
#include "realtimeplot.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QTcpSocket;
class QNetworkSession;
QT_END_NAMESPACE



namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

signals:
    void updateSensorData(const vector<Vector_3D<double>>& sensorData);


private slots:
    void directoryChanged_dicom(const QString & path);
    void directoryChanged_raw(const QString & path);
    void ImageReconStateChanged(int state);
    void checkBox_1StateChanged(int state);
    // Network functions
    void requestNewSensorData();
    void readSensorData();
    void displayError(QAbstractSocket::SocketError socketError);
    void enableConnectButton();
    void sessionOpened();
    void isclicked();
    void sendRequest();
    // Dicom annotation
    void dicom_Annotation();
    // Measurement
    void on_pushButton_toggled(bool checked);
    //Show the plot form
    void on_plot_clicked();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    // Measurement
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *);


private:
    /* For internal use only */
    //Convert Dicom to Qt image format
    bool ConvertToFormat_RGB888(gdcm::Image const & gimage, char *buffer, QImage *&imageQt);
    void delay(int msec);

    // String parser
    void stringParser(std::string s, double *outputMatrix, size_t size);

    // Image orientation
    char* getOrientation_H(double orientation_Matrix[]);
    char* getOrientation_V(double orientation_Matrix[]);

    // Drop event
    void FindFile_dropped(QString filename);
    void autoLoader(QString filename);

    // Image annotation functions
    std::pair<double, double> pix_3d_ptsfrom3d( double newlinept1xyz[], double newlinenorm[],
                                                double TLC_XYZmm[], double BRC_XYZmm[], double TRC_XYZmm[],
                                                double Im_rowdircoldir[], double thisPixSizeLocalYX[], double *newlineintersect_XYZmm);

    std::tuple<double, double, double, double> precise_phys_virt_na_dist(double thisFOVmm_LocalYX[], double thisPixSizeLocalYX[], double ImageNcols, double ImageNrows ,
                                                                         double TLC_XYZmm[], double TRC_XYZmm[] , double BRC_XYZmm[], double  ImageNormal[] , double Im_rowdircoldir[],
                                                                         double one_XYZ_pLPH[], double one_IJK_pLPH[]);

    void dicom_Annotation_RT(QString& filename);


    // Variables
    Ui::MainWindow *ui;
    RealTimePlot plot_XYZ; // plot form
    QFileSystemWatcher * watcher;
    QFileSystemWatcher * watcher_raw;
    const int window_2D_width;

    // Network
    QTcpSocket *tcpSocket;
    QString currentSensorData;
    quint16 blockSize;
    bool isDisconnect_Clicked;

    QNetworkSession *networkSession;

    // Received tracking data
    float XYZ[3];
    float nvt[3];
    float tvt[3];

    //Measurement
    float m_nInitialX, m_nInitialY, m_nFinalX, m_nFinalY;
    bool m_nbMousePressed;
    bool do_measure;
    QPixmap *m_nPTargetPixmap;

    // Log file
    QFile *myFile;
    QTextStream myLog();
    QString Image_path, Log_path;

    QPolygonF arrowHead;
   // QLineF line;
};

#endif // MAINWINDOW_H
