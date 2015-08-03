#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gdcmTag.h"
#include "gdcmStringFilter.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <memory>
#include <string>
#include "ReadRawData.h"
#include <QtNetwork>




MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    window_2D_width(420),
    networkSession(0),
    isDisconnect_Clicked(false),
    m_nInitialX(0.0f),
    m_nInitialY(0.0f),
    m_nFinalX(0.0f),
    m_nFinalY(0.0f),
    m_nbMousePressed(false),
    do_measure(false),
    Image_path( QDir::currentPath()),
    Log_path( QDir::currentPath())
{
    ui->setupUi(this);

    setWindowFlags( (windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowMaximizeButtonHint);

    ui->label_1->setScaledContents(true);
    ui->label_2->setScaledContents(true);
    ui->label_3->setScaledContents(true);

    setUpdatesEnabled(true);
    setAcceptDrops(true);

    // Automatically upload Dicom files
    watcher = new QFileSystemWatcher(this);
    watcher->addPath("Y:/20150417.UMB_COLLPark.0013");
    connect(watcher, SIGNAL(directoryChanged(const QString &)), this, SLOT(directoryChanged_dicom(const QString &)));

    // Automatically upload and recon MRI Raw data file
    watcher_raw = new QFileSystemWatcher(this);
    watcher_raw->addPath(QDir::currentPath() + QString("/Dicom_Dir"));
    connect(watcher_raw, SIGNAL(directoryChanged(const QString &)), this, SLOT(directoryChanged_raw(const QString &)));

    // Make sure that the two checkboxes (R/T Dicom and Image Recon) are NOT checked at the same time
    connect(ui->checkBox_1, SIGNAL(stateChanged(int)), this, SLOT(ImageReconStateChanged(int)));
    connect(ui->checkBox_ImageRecon, SIGNAL(stateChanged(int)), this, SLOT(checkBox_1StateChanged(int)));
    connect(ui->Annotation, SIGNAL(clicked()), this, SLOT(dicom_Annotation()));

    // Network =Connect to EndoScout server
    ui->hostCombo->setEditable(true);
    // find out name of this machine
    QString name = QHostInfo::localHostName();
    if (!name.isEmpty()) {
        ui->hostCombo->addItem(name);
        QString domain = QHostInfo::localDomainName();
        if (!domain.isEmpty())
            ui->hostCombo->addItem(name + QChar('.') + domain);
    }
    if (name != QString("localhost"))
        ui->hostCombo->addItem(QString("localhost"));
    // Find out IP addresses of this machine
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    // add non-localhost addresses
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (!ipAddressesList.at(i).isLoopback())
            ui->hostCombo->addItem(ipAddressesList.at(i).toString());
    }
    // add localhost addresses
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i).isLoopback())
            ui->hostCombo->addItem(ipAddressesList.at(i).toString());
    }

    ui->ServerPort->setValidator(new QIntValidator(1, 65535, this));
    ui->Connect->setEnabled(false);
    ui->checkBox_1->setEnabled(false);
    ui->Annotation->setEnabled(false);

    // TCP/IP socket
    tcpSocket = new QTcpSocket(this);

    connect(ui->hostCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(enableConnectButton()));
    connect(ui->ServerPort, SIGNAL(textChanged(QString)),
            this, SLOT(enableConnectButton()));
    connect(ui->Connect, SIGNAL(clicked()), this, SLOT(requestNewSensorData()));

    connect(ui->Disconnect, SIGNAL(clicked()), this, SLOT(isclicked()));

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(sendRequest()));

    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readSensorData()));

    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError(QAbstractSocket::SocketError)));

    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        // Get saved network configuration
        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();

        // If the saved network configuration is not currently discovered use the system default
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) !=
                QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }

        networkSession = new QNetworkSession(config, this);
        connect(networkSession, SIGNAL(opened()), this, SLOT(sessionOpened()));

        ui->Connect->setEnabled(false);
        networkSession->open();
    }

    // Measurement
    ui->measure_Button->setCheckable(true);
   // m_nPTargetPixmap = new QPixmap( "image.png" );
    connect(ui->measure_Button, SIGNAL(toggled(bool)), this, SLOT(on_pushButton_toggled(bool)));

    // Logger
    auto myString =  QTime::currentTime().toString();
    QString hours   = myString.mid(0, 2);
    QString minutes = myString.mid(3, 2);
    QString seconds = myString.mid(6, 2);
    const QString LogFileName = Log_path + QString("/Logger/Log_") + QDate::currentDate().toString("dd.MM.yyyy") + QString("_") +
            hours + QString(".") + minutes + QString(".") + seconds + QString(".txt");

    myFile = new QFile(LogFileName);
    myFile->open(QIODevice::WriteOnly|QIODevice::Text);

    QTextStream(myFile) << QDate::currentDate().toString("ddd MMMM d yyyy") +
                           QString(" ") +  QTime::currentTime().toString() + QString("  INFO - ") + QString("Program started ...") << endl;

    // Send sensor data for real time plotting
    connect(this, SIGNAL(updateSensorData(const vector<Vector_3D<double> >&)), &plot_XYZ, SLOT(getSensorData(const vector<Vector_3D<double> >&)));

}

MainWindow::~MainWindow()
{
    QTextStream(myFile) << QDate::currentDate().toString("ddd MMMM d yyyy") +
                           QString(" ") +  QTime::currentTime().toString() + QString("  INFO - ") + QString("Program ended ...") << endl;

    // close the connection
    tcpSocket->close();
    //Close Log file
    myFile->close();

    // Be tidy
    delete myFile;
    delete ui;
}

void MainWindow::directoryChanged_dicom(const QString &path)
{
    if(ui->checkBox_1->isChecked())
    {
        QDir dir(path);
        QFileInfoList list = dir.entryInfoList(QDir::Files);

        if (!list.isEmpty())
            dicom_Annotation_RT(list.last().absoluteFilePath());
    }
}

void MainWindow::directoryChanged_raw(const QString &path)
{
    if(ui->checkBox_ImageRecon->isChecked())
    {
        QDir dir(path);
        QFileInfoList list = dir.entryInfoList(QDir::Files);

        if (!list.isEmpty()){
            ReadRawData ks;
            QImage imageQt_Scaled = ks.reconImage(list.last().absoluteFilePath());

            if(!imageQt_Scaled.isNull()) {
                // Display the image
                ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            }
            else{
                std::cerr << "Error in reconImage function, abort...\n";
                exit(-1);
            }
        }
    }
}

void MainWindow::ImageReconStateChanged(int state)
{
    if (state)
        ui->checkBox_ImageRecon->setChecked(false);
}

void MainWindow::checkBox_1StateChanged(int state)
{
    if (state)
        ui->checkBox_1->setChecked(false);
}

void MainWindow::requestNewSensorData()
{
    ui->Connect->setEnabled(false);
    blockSize = 0;
    tcpSocket->abort();
    tcpSocket->connectToHost(ui->hostCombo->currentText(),
                             ui->ServerPort->text().toInt());
}

void MainWindow::readSensorData()
{

    // Received data parameters
    QString channel;
    QString StatusCode;
    vector<Vector_3D<double>> sensorData;
    sensorData.reserve(3);

    forever{

        if (isDisconnect_Clicked)
            break;

        if(tcpSocket->bytesAvailable()){
            //Get the data
            QString s = QString::fromLatin1((tcpSocket->readAll()).constData());

            // Parse the received string
            QTextStream(&s) >> channel >> XYZ[0] >> XYZ[1] >> XYZ[2] >> nvt[0] >>nvt[1]
                            >> nvt[2] >> tvt[0] >> tvt[1] >> tvt[2] >> StatusCode;


            // Check for good tracking status
            if(StatusCode == QString("1000")){

                // Store the sensor data to be sent for real time plotting
                Vector_3D<double> a(XYZ[0],XYZ[1],XYZ[2]);
                Vector_3D<double> b(nvt[0],nvt[1],nvt[2]);
                Vector_3D<double> c(tvt[0],tvt[1],tvt[2]);
                sensorData.push_back(a);
                sensorData.push_back(b);
                sensorData.push_back(c);

                emit updateSensorData(sensorData);

                // Formatting of the string soon to be displayed
                QString position_x(  QString::number(XYZ[0], 'f', 6) );
                QString position_y(  QString::number(XYZ[1], 'f', 6) );
                QString position_z(  QString::number(XYZ[2], 'f', 6) );

                // Show the received data
                ui->Sensor_x->setText(position_x);
                ui->Sensor_y->setText(position_y);
                ui->Sensor_z->setText(position_z);
                update();

                // Enable the annotation button and the real time check box
                // after recieving the first data
                if(!ui->Annotation->isEnabled())
                    ui->Annotation->setEnabled(true);

                if(!ui->checkBox_1->isEnabled())
                    ui->checkBox_1->setEnabled(true);
            }
        }
        // Wait for one second before sending another request
        // This function doesn't freeze the GUI
        delay(100); // unit sec.
        sensorData.clear();
        emit sendRequest();
    }

    // close the connection
    tcpSocket->close();

    isDisconnect_Clicked = false;
    ui->Connect->setEnabled(true);

}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        ui->Connect->setEnabled(true);
        break;
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, tr("MINIR II"),
                                 tr("The host was not found. Please check the "
                                    "host name and port settings."));
        ui->Connect->setEnabled(true);
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, tr("MINIR II"),
                                 tr("The connection was refused by the peer. "
                                    "Make sure the EndoScout server is running, "
                                    "and check that the host name and port "
                                    "settings are correct."));
        ui->Connect->setEnabled(true);
        break;
    default:
        QMessageBox::information(this, tr("MINIR II"),
                                 tr("The following error occurred: %1.")
                                 .arg(tcpSocket->errorString()));
        ui->Connect->setEnabled(true);
    }
}

void MainWindow::enableConnectButton()
{
    ui->Connect->setEnabled((!networkSession || networkSession->isOpen()) &&
                            !ui->hostCombo->currentText().isEmpty() &&
                            !ui->ServerPort->text().isEmpty());
}

void MainWindow::sessionOpened()
{
    // Save the used configuration
    QNetworkConfiguration config = networkSession->configuration();
    QString id;
    if (config.type() == QNetworkConfiguration::UserChoice)
        id = networkSession->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
    else
        id = config.identifier();

    QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
    settings.beginGroup(QLatin1String("QtNetwork"));
    settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
    settings.endGroup();

    enableConnectButton();
}

void MainWindow::isclicked()
{
    isDisconnect_Clicked = true;
}

void MainWindow::sendRequest()
{
    // Request string
    const char request[]  = "DATA 1";
    tcpSocket->write(request, strlen(request));
    tcpSocket->waitForBytesWritten(1000);

}

void MainWindow::dicom_Annotation()
{
    // Read all files
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select one or more files to open"), QDir::currentPath());

    int _count = 1;
    // Read all files
    for(size_t ii = 0; ii < files.size(); ++ii)
    {
        // Start logging
        QStringList parts = files.at(ii).split("/");
        QString lastBit = parts.at(parts.size()-1);
        QTextStream(myFile) << QDate::currentDate().toString("ddd MMMM d yyyy") +
                               QString(" ") +  QTime::currentTime().toString() + QString("  INFO - ") + QString("Dicom loaded: ") + lastBit << endl;

        // Read input dicom file
        gdcm::ImageReader ir;
        ir.SetFileName( files.at(ii).toLocal8Bit().constData() );
        if(!ir.Read())
        {
            std::cerr << "One or more file(s) are not supported, abort...\n";
            return;
        }

        // Let's get some info from the header file
        gdcm::File &file = ir.GetFile();
        gdcm::StringFilter  sf = gdcm::StringFilter();
        sf.SetFile(file);

        /*
         *  The names of the variables are based on Barry matlab script for simplicity
         *  Gathering the input data
         * */
        std::string PatOrient(sf.ToStringPair(gdcm::Tag(0x0018,0x5100)).second);

        // Dicom location
        double TLC_XYZmm[3];
        stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0032)).second, TLC_XYZmm, 3);

        double Im_rowdircoldir[6];
        stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, Im_rowdircoldir, 6);

        double Slice_Thick = std::stod(sf.ToStringPair(gdcm::Tag(0x0018,0x0050)).second);

        double PixSizeLocalYX[2]; // Pixel spacing
        stringParser(sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second, PixSizeLocalYX, 2);

        // This tag (0051,1012) does not always exit, therefore I am going to assume:
        //  - TablePositionString = "TP 0" ;
        // Hence:
        //  - const char TablePosHorF = 'H';
        //  - const size_t TablePosVal  = 0;
        const char TablePosHorF  = 'H';
        const size_t TablePosVal = 0;


        double ImageNrows = std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second ) ;
        double ImageNcols = std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second ) ;

        // Get the tracking sensor coordinates
        // These coord. need to be queried in real-time from the endoscout machine, but for now this will
        // do it.

        /*double orig_one_RISC_XYZ [] = { 26.6057, 36.4035, 85.0759 };
        double orig_one_RISC_IJK[] = { -0.16185589904513695, -0.79944343693693765, -0.57852645495495447 };
        double orig_one_RISC_TnormIJK[] = { 1, 0, 0 };*/

        double orig_one_RISC_XYZ []     = { XYZ[0], XYZ[1], XYZ[2] };
        //double orig_one_RISC_IJK[]      = { tvt[0], tvt[1], tvt[2] };
        //double orig_one_RISC_TnormIJK[] = { nvt[0], nvt[1], nvt[2] };

        // For the monopole sensor =Mahamadou=
        //double orig_one_RISC_IJK[]      = { tvt[0], tvt[1], tvt[2] };
        //double orig_one_RISC_TnormIJK[] = { nvt[0], nvt[1], nvt[2] };

        // For the catheder 036 sensor
        double orig_one_RISC_IJK[]      = { nvt[0], nvt[1], nvt[2] };
        double orig_one_RISC_TnormIJK[] = { tvt[0], tvt[1], tvt[2] };

        // Start logging Sensor data
        QTextStream(myFile) << QDate::currentDate().toString("ddd MMMM d yyyy") +
                               QString(" ") +  QTime::currentTime().toString() + QString("  INFO - ") + QString("Sensor data: ") <<
                               QString::number(XYZ[0]) + QString(" ") + QString::number(XYZ[1]) + QString(" ") +QString::number(XYZ[2])<<
                               QString("  ") + QString::number(nvt[0]) + QString(" ") + QString::number(nvt[1]) + QString(" ") +QString::number(nvt[2]) <<
                               QString("  ") + QString::number(tvt[0]) + QString(" ") + QString::number(tvt[1]) + QString(" ") +QString::number(tvt[2])<< endl;


        /*
         * Start computing some relevents matrices
         */

        // Compute the Field of View
        double FOVmm_LocalYX[] = { PixSizeLocalYX[0]*ImageNrows, PixSizeLocalYX[1]*ImageNcols}  ; // no need to :  round()

        // Determine the normal to the dicom image
        // Using the cross product of Im_rowdirColdir
        double ImageNormal[] = { Im_rowdircoldir[1]*Im_rowdircoldir[5] - Im_rowdircoldir[2]*Im_rowdircoldir[4],
                                 Im_rowdircoldir[2]*Im_rowdircoldir[3] - Im_rowdircoldir[0]*Im_rowdircoldir[5],
                                 Im_rowdircoldir[0]*Im_rowdircoldir[4] - Im_rowdircoldir[3]*Im_rowdircoldir[1] };

        // Bottom Right Corner, the center of that pixel :
        double BRC_XYZmm[3];         //  Move to Top Right (row movement). Move to Bottom Right (col movement)
        BRC_XYZmm[0] = TLC_XYZmm[0]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[0] +  (FOVmm_LocalYX[0]-1*PixSizeLocalYX[0])*Im_rowdircoldir[3];
        BRC_XYZmm[1] = TLC_XYZmm[1]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[1] +  (FOVmm_LocalYX[0]-1*PixSizeLocalYX[0])*Im_rowdircoldir[4];
        BRC_XYZmm[2] = TLC_XYZmm[2]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[2] +  (FOVmm_LocalYX[0]-1*PixSizeLocalYX[0])*Im_rowdircoldir[5];

        // Top Right Corner, the center of that pixel :
        double TRC_XYZmm[3];               // Move to Top Right (row movement)
        TRC_XYZmm[0] = TLC_XYZmm[0]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[0];
        TRC_XYZmm[1] = TLC_XYZmm[1]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[1];
        TRC_XYZmm[2] = TLC_XYZmm[2]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[2];

        // Set 2 = optional, _if_ need to work with the surfaces of the MRI image as a volume/slice/slab :
        double PS_TLC_XYZmm[3]; // "PLUS" Surface, point 1 (Top Left).
        PS_TLC_XYZmm[0] = TLC_XYZmm[0] + 0.5*Slice_Thick*ImageNormal[0] ; PS_TLC_XYZmm[1] = TLC_XYZmm[1] + 0.5*Slice_Thick*ImageNormal[1] ;  PS_TLC_XYZmm[2] = TLC_XYZmm[2] + 0.5*Slice_Thick*ImageNormal[2] ;
        double  MS_TLC_XYZmm[3]; // "MINUS" Surface, point 1 (Top Left).
        MS_TLC_XYZmm[0] = TLC_XYZmm[0] - 0.5*Slice_Thick*ImageNormal[0] ; MS_TLC_XYZmm[1] = TLC_XYZmm[1] - 0.5*Slice_Thick*ImageNormal[1] ;  MS_TLC_XYZmm[2] = TLC_XYZmm[2] - 0.5*Slice_Thick*ImageNormal[2] ;
        double PS_BRC_XYZmm[3]; // "PLUS" Surface, point 2 (Bottom Right).
        PS_BRC_XYZmm[0] = BRC_XYZmm[0] + 0.5*Slice_Thick*ImageNormal[0] ; PS_BRC_XYZmm[1] = BRC_XYZmm[1] + 0.5*Slice_Thick*ImageNormal[1] ;  PS_BRC_XYZmm[2] = BRC_XYZmm[2] + 0.5*Slice_Thick*ImageNormal[2] ;
        double MS_BRC_XYZmm[3]; // "MINUS" Surface, point 2 (Bottom Right).
        MS_BRC_XYZmm[0] = BRC_XYZmm[0] - 0.5*Slice_Thick*ImageNormal[0] ; MS_BRC_XYZmm[1] = BRC_XYZmm[1] - 0.5*Slice_Thick*ImageNormal[1] ;  MS_BRC_XYZmm[2] = BRC_XYZmm[2] - 0.5*Slice_Thick*ImageNormal[2] ;
        double PS_TRC_XYZmm[3]; // "PLUS" Surface, point 3 (Top Right).
        PS_TRC_XYZmm[0] = TRC_XYZmm[0] + 0.5*Slice_Thick*ImageNormal[0] ; PS_TRC_XYZmm[1] = TRC_XYZmm[1] + 0.5*Slice_Thick*ImageNormal[1] ;  PS_TRC_XYZmm[2] = TRC_XYZmm[2] + 0.5*Slice_Thick*ImageNormal[2] ;
        double MS_TRC_XYZmm[3]; // "MINUS" Surface, point 3 (Top Right).
        MS_TRC_XYZmm[0] = TRC_XYZmm[0] - 0.5*Slice_Thick*ImageNormal[0] ; MS_TRC_XYZmm[1] = TRC_XYZmm[1] - 0.5*Slice_Thick*ImageNormal[1] ;  MS_TRC_XYZmm[2] = TRC_XYZmm[2] - 0.5*Slice_Thick*ImageNormal[2] ;

        /*
         * Step 2 - perform RISC (Robin Invariant Scanner Coordinate) to +LPH Coordinate System Conversion, on the tracking data
        */
        // Declare some variables
        int  InnerFactor = 0;
        int  OuterFactor = 0;

        if (TablePosHorF == 'H')
            InnerFactor = -1;
        else if (TablePosHorF == 'F')
            InnerFactor = +1;
        else
            std::cerr << "Invalid value for TablePosHorF, must quit program";

        // Make a temporary copy of orig_one_RISC_XYZ
        double Temp[3], Temp1[3], Temp2[3];
        std::copy(orig_one_RISC_XYZ, orig_one_RISC_XYZ+3, Temp);
        std::copy(orig_one_RISC_IJK, orig_one_RISC_IJK+3, Temp1);
        std::copy(orig_one_RISC_TnormIJK, orig_one_RISC_TnormIJK+3, Temp2);


        if(PatOrient.compare(std::string("HFS ")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME= as before. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }
        else if(PatOrient.compare(std::string("HFP ")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME= as before. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }
        else if(PatOrient.compare(std::string("FFS ")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME= as before. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];


        }

        else if(PatOrient.compare(std::string("FFP ")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME=. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("HFDR")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes to [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];


            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("HFDL")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes to [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];


            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("FFDR")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes to [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];


            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("FFDL")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];

            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];

        }
        else{
            std::cerr << "Invalid PatOrient value, abort...";
            exit(-1);
        }

        /*
        * Draw the annotation :
        * Please note : this is only one method of performing the annotation.
        * this annotation mimics the method of GE Signa SP / iDrive, circa 1995.
        * this method can be changed and still be valid.
        * some choices that are made, that can be changed/simplified in another annotation method, are :
        *           - drawing the annotation line in 20mm chunks
        *           - in different colors
        *           - dealing with : within plane, proximal to plane, distal to plane
        *                   - the need to calc the "PLUS" Surface and the "MINUS" Surface
        *           - 60 degrees value
        *           - the bifurcated method of line or dots
        *           - adding text of distance, when method is dots
        */

        // Compute the angle of the needle
        long double angle_needle_off_slice = abs( 90 - acos( ImageNormal[0]*orig_one_RISC_IJK[0] + ImageNormal[1]*orig_one_RISC_IJK[1] + ImageNormal[2]*orig_one_RISC_IJK[2]) * 180/M_PI  ) ;

        // Start drawing
        if (angle_needle_off_slice <= 60)   // draw a Needle Annotation as Linear Line
        {

            // Parse the image orientation string
            double orientation_matrix[6];
            stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix, 6);

            // Get image orientation
            char *Letter_H = getOrientation_H(orientation_matrix);
            char *Letter_V = getOrientation_V(orientation_matrix);


            // Get the largest image pixel value
            //uint LargestImagePixelValue = /*std::stoi(sf.ToStringPair(gdcm::Tag(0x0028,0x0107)).second)*/ 1482;

            const gdcm::Image &gimage = ir.GetImage();
            std::vector<char> vbuffer;
            vbuffer.resize( gimage.GetBufferLength() );
            char *buffer = &vbuffer[0];

            // Convert dicom to RGB format
            QImage *imageQt = NULL;
            if( !ConvertToFormat_RGB888( gimage, buffer, imageQt ) )
            {
                std::cerr << "Error during file conversion: ConvertToFormat_RGB888\n";
            }


            // FOV calculation
            // FOVx = Raw * Pixel_spacing[0];
            // FOVy = Col * Pixel_spacing[1];
            std::string token = "\\";
            std::string s = sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second;

            long double FOVx = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second) *
                                          std::stod(s.substr(0, s.find(token))) + 0.5);

            s.erase(0, s.find(token) + token.length());

            long double FOVy = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second) *
                                          std::stod(s) + 0.5);


            // Keep aspect ratio
            QImage imageQt_Scaled = imageQt->scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

            // Write some Dicom Info on the display
            std::unique_ptr<QPainter> painter(new QPainter(&imageQt_Scaled));
            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));

            //Draw the text on the image
            painter->drawText( QPoint(2,14),    QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0010,0x0010)).second));
            painter->drawText( QPoint(2,28),    QString::fromStdString("FOV: " + std::to_string(FOVx) + "*" + std::to_string(FOVy)));

            painter->setPen(Qt::white);
            painter->setFont(QFont("Segoe UI",16));
            painter->drawText( QPoint(4,230),    QString::fromStdString(Letter_H));
            painter->drawText( QPoint(218,20),   QString::fromStdString(Letter_V));

            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));
            painter->drawText( QPoint(267,14),  QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).second));
            painter->drawText( QPoint(267,28),  QString::fromStdString("Date: " + sf.ToStringPair(gdcm::Tag(0x0008,0x0023)).second));
            painter->drawText( QPoint(2,390),   QString::fromStdString("TR: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0080)).second + " ms"));
            painter->drawText( QPoint(2,404),   QString::fromStdString("TE: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0081)).second + " ms"));
            painter->drawText( QPoint(2,418),   QString::fromStdString("FA: " + sf.ToStringPair(gdcm::Tag(0x0018,0x1314)).second + " deg"));


            /*
             *  See "GE_FPmanual_NeedleTracking19-11-02.pdf", pages 1-33 to 1-35, for a discussiom on Needle Annotation.
             *
             * 3 - learn where the Plus Surface and the Minus Surface are intersected by the 3-D needle annotation.
             * The result of this function is the plot locations on the MR image - "local-x" and "local-y", in whole/integer pixel units.
             * Result will = -999 if image plane and needle are parallel.
             * Also, this is the "general" solution, result may be <1, >ImageNrows, read "pixptsfrom3d" for more information.
             */
            std::pair<double, double> psi_centralpix, msi_centralpix;
            double newlineintersect_XYZmm[3];
            double newlineintersect_XYZmm_2[3];

            psi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, PS_TLC_XYZmm, PS_BRC_XYZmm, PS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm ) ;
            msi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, MS_TLC_XYZmm, MS_BRC_XYZmm, MS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm_2 ) ;

            // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
            // http://mathworld.wolfram.com/Point-PlaneDistance.html

            double dist_tip_PSinter = 0.0;
            double dist_tip_MSinter = 0.0;
            double temp[] = {newlineintersect_XYZmm[0] - orig_one_RISC_XYZ[0], newlineintersect_XYZmm[1] - orig_one_RISC_XYZ[1], newlineintersect_XYZmm[2] - orig_one_RISC_XYZ[2]};
            if (std::abs(newlineintersect_XYZmm[0]) != +999)
                dist_tip_PSinter = orig_one_RISC_IJK[0]*temp[0] + orig_one_RISC_IJK[1]*temp[1] + orig_one_RISC_IJK[2]*temp[2] ;
            else
                dist_tip_PSinter = -999 ;

            double temp_1[] = {newlineintersect_XYZmm_2[0]-orig_one_RISC_XYZ[0], newlineintersect_XYZmm_2[1]-orig_one_RISC_XYZ[1], newlineintersect_XYZmm_2[2]-orig_one_RISC_XYZ[2]};
            if (std::abs(newlineintersect_XYZmm_2[0]) != +999)
                dist_tip_MSinter = orig_one_RISC_IJK[0]*temp_1[0] + orig_one_RISC_IJK[1]*temp_1[1] + orig_one_RISC_IJK[2]*temp_1[2] ;
            else
                dist_tip_MSinter = -999;

            /*
            * 4 - location of the needle tip on the (local) MR image.
            * See pencil notes page 23 for explaination why "ImageNormal" substituted for "locposPNT(4:6)".
            */
            double unused_var[3];
            std::pair<double, double> tip_plot_pix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, unused_var);

            /*
            * 5 - learn exactly what pixels need to be created for the NA (dashes = solid and blanks)
            * and what color each should be (prox, distal, or in plane)

            * and 6 - PLOT
            * (Original plan was to separate out calculations and plotting, but both are done in the sub-functions below ("plot_NApixels, etc) )


            * PROGRAMMING NOTE : The problem has been structured to determine
            * how much of the "physcial needle" must be plotted (16mm solid, 4mm blank)
            * and how much of the "virtual needle" must be plotted (6mm solid, 14mm blank).

            * My initial solution was to determine precisely the exact (plus some overshoot) distance of each.
            * This requires some detailed calculations to determine where the NA "enters" the MR image and where it "exists" - see "precise_phys_virt_na_dist".
            * Another solution is to pick 2 very large distances, the max possible that can occur,
            * and plot them blindly onto the MR image.

            * on 4/23/04, a speed test was conducted in Matlab. (See commented code for "tic", "toc", "temp_del_jc","accum_tictocs", etc.)
            * the "precise" method took M+-SD 0.06 +- 0.02 sec to calc and plot each case
            * distances=2000 (the diameter of the OpenSpeed magnets) took M+-SD 0.26 +- 0.02 sec to plot each case, 4 times longer.

            * You may want to perform the same type of speed test in the real time C program, to help decide which method is better.
            */
            double physical_max_distance, phys_dist_begins, virtual_max_distance , virt_dist_begins;
            std::tie(physical_max_distance, phys_dist_begins, virtual_max_distance , virt_dist_begins) =
                    precise_phys_virt_na_dist( FOVmm_LocalYX , PixSizeLocalYX , ImageNcols , ImageNrows , TLC_XYZmm , TRC_XYZmm , BRC_XYZmm , ImageNormal ,
                                               Im_rowdircoldir , orig_one_RISC_XYZ, orig_one_RISC_IJK);

            // Assign Constants :
            const int DBDB_mm = 20; // = "Dist_Bet_Dash_Begins" , set by GE, read "GE_FPmanual_NeedleTracking19-11-02.pdf" for more info

            if (physical_max_distance > 0) // Mahamadou
            {// create physical needle pattern
                const int len_solid_part_mm = 16; // in physical pattern, 16 out of 20 mm are solid, determined by BF by counting on IP images.

                //target_distances_beg_end =   0:DBDB_mm:physical_max_distance+DBDB_mm ;  % overshoot how long you must go, allow auto-clipping
                std::vector<double> target_distances_beg_end, target_distances_beg_end_temp;
                int ii = 0;
                while (ii <= physical_max_distance+DBDB_mm){
                    // Keep only values less or equal phys_dist_begins
                    if (ii >= phys_dist_begins)
                        target_distances_beg_end.push_back(ii);
                    ii += DBDB_mm;
                }

                // Target_distances_beg_end = [  target_distances_beg_end    target_distances_beg_end+len_solid_part_mm ] ;
                target_distances_beg_end_temp.resize(target_distances_beg_end.size());
                for (int ii = 0; ii < target_distances_beg_end_temp.size(); ++ii)
                    target_distances_beg_end_temp[ii] = target_distances_beg_end[ii] + len_solid_part_mm;

                // Append the two vec.
                target_distances_beg_end.insert(target_distances_beg_end.end(), target_distances_beg_end_temp.begin(),target_distances_beg_end_temp.end());

                // Will travel retrograde, from the needle tip to off the MR image
                double dir2travel[3];
                dir2travel[0]= -1 * orig_one_RISC_IJK[0];
                dir2travel[1]= -1 * orig_one_RISC_IJK[1];
                dir2travel[2]= -1 * orig_one_RISC_IJK[2];

                // I can introduce the analysis/comparison to dist_tip_PSinter, dist_tip_MSinter
                // Here or inside plot_NA_dashes - do it inside.
                std::vector<double> additionalbreak;
                additionalbreak.reserve(2);

                int sign_of_dist_tip_PSinter = (dist_tip_PSinter > 0) ? 1 :((dist_tip_PSinter < 0) ? -1 : 0);

                if (sign_of_dist_tip_PSinter == -1 && std::abs(dist_tip_PSinter)!= 999 && std::abs(dist_tip_PSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_PSinter));
                // Do abs() - I checked that direction is correct already, so submit it as a positive distance (in dir2travel direction)

                int sign_of_dist_tip_MSinter = (dist_tip_MSinter > 0) ? 1 :((dist_tip_MSinter < 0) ? -1 : 0);
                if (sign_of_dist_tip_MSinter == -1 && std::abs(dist_tip_MSinter)!=999 && std::abs(dist_tip_MSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_MSinter));
                // Sort the elements of target_distances_beg_end
                std::sort(target_distances_beg_end.begin(), target_distances_beg_end.end());
                // Remove all the duplicated elements
                auto last = std::unique(target_distances_beg_end.begin(), target_distances_beg_end.end());
                target_distances_beg_end.erase(last,target_distances_beg_end.end());

                /* Start plotting
                 * Part 0 - add dist_tip_PSinter, dist_tip_MSinter to the mix of begin - end pairs  if necc.
                 * If I need to break a dash into 2 pieces
                */
                if (additionalbreak.size() != 0){
                    double newbreak_val1val2[2];
                    for (size_t ii = 0; ii < target_distances_beg_end.size()/2 ; ++ii){
                        for (size_t jj = 0; jj < additionalbreak.size(); ++jj){
                            //double thisAB = additionalbreak[perAB] ;
                            if (additionalbreak[jj] > target_distances_beg_end[2*ii] && additionalbreak[jj] < target_distances_beg_end[2*ii +1]){
                                newbreak_val1val2[0] = additionalbreak[jj] - 0.05;
                                newbreak_val1val2[1] = additionalbreak[jj] + 0.05;
                            }
                        }
                    }
                    if (sizeof(newbreak_val1val2)/sizeof(newbreak_val1val2[0]) != 0)
                        target_distances_beg_end.insert(target_distances_beg_end.end(), newbreak_val1val2, newbreak_val1val2+2);
                }

                // Part 1 - determine the true 3-D locations of 3-D jumps, up to plotreal3Ddistance, and then the pixel locations to plot

                size_t Npts = target_distances_beg_end.size();
                // Pixel_locs_X_Y = zeros(Npts,2);
                std::vector< std::pair<double, double>> pixel_locs_X_Y(Npts);
                double thisloc_XYZmm[3];
                double unused_var2[3];

                for(size_t ii = 0; ii < Npts; ++ii){
                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + target_distances_beg_end[ii]*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + target_distances_beg_end[ii]*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + target_distances_beg_end[ii]*dir2travel[2];

                    // get the pixel location of this 3-D point
                    std::pair<double, double> pix = pix_3d_ptsfrom3d( thisloc_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, unused_var2 );
                    pixel_locs_X_Y[ii].first = pix.first;
                    pixel_locs_X_Y[ii].second = pix.second;
                }

                // part 2 -
                // learn if these 3-D points, "thisloc_XYZmm", are distal, proximal, or in-slice
                // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
                // http://mathworld.wolfram.com/Point-PlaneDistance.html
                // analyze the central point of each dash. The edges may be coincident with the PS or MS,...

                size_t Npairs = Npts/2;
                std::vector<double> dist4color_perpair(Npairs);

                for (size_t ii = 0;  ii < Npairs; ++ii){

                    double thisdist = (target_distances_beg_end[2*ii] + target_distances_beg_end[2*ii +1])/2;
                    double thisloc_XYZmm[3];

                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + thisdist*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + thisdist*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + thisdist*dir2travel[2];

                    // from MR image to needle line. distance = final - initial. From Mr image to 3-D needle line.
                    double temp[] = {thisloc_XYZmm[0] - TLC_XYZmm[0], thisloc_XYZmm[1] - TLC_XYZmm[1], thisloc_XYZmm[2] - TLC_XYZmm[2]};
                    double D = ImageNormal[0]*temp[0] + ImageNormal[1]*temp[1]+ ImageNormal[2]*temp[2];

                    dist4color_perpair[ii] = D;
                }

                // part 3 - fill in the solid pattern part, from beginning to end
                // pre-make with all blue. Find the places where the color is different than this.
                //colorperpair = abs(inplanecolor) * ones(1,Npairs);
                std::vector<int> colorperpair(Npairs, 98); // abs('b')=98 in Matlab

                // Change color to yellow
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] < -1*Slice_Thick/2)
                        // Change to yellow
                        colorperpair[ii] = 121;                    // abs('y') = 121 in Matlab
                }

                // Change to red
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] > +1*Slice_Thick/2)
                        // Change to red
                        colorperpair[ii] = 114;                    // abs('r') = 114 in Matlab
                }

                // Start plotting
                QVector<QPoint> pointPairs;
                for(size_t ii = 0; ii < Npairs; ++ii){
                    if (2*ii+1 < Npts)
                        pointPairs << QPoint(std::ceil(pixel_locs_X_Y[2*ii].first * window_2D_width/ImageNrows), std::ceil(pixel_locs_X_Y[2*ii+1].first * window_2D_width/ImageNrows)) // scaling factor
                                << QPoint(std::ceil(pixel_locs_X_Y[2*ii].second * window_2D_width/ImageNcols), std::ceil(pixel_locs_X_Y[2*ii+1].second * window_2D_width/ImageNcols));
                }

                // PROGRAMMING NOTE : Decide, if it is better in C, to first test if the points are in the image, or to always plot and take advantage of auto-clip.
                // plot( [ pt1(1)  pt2(1) ] , [ pt1(2)  pt2(2) ] , '-' , 'color', colorperpair(perpair), 'LineWidth' , NALineWidth );

                for (int ii = 0; ii < Npairs; ++ii){
                    if (colorperpair[ii] == 121){
                        QPen pen1(Qt::yellow, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                    else if (colorperpair[ii] == 114){
                        QPen pen1(Qt::red, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                    else if (colorperpair[ii] == 98){
                        QPen pen1(Qt::blue, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                }

                // Load the dicom file
                if (_count == 1){
                    ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    //Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 2){
                    ui->label_2->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 3){
                    ui->label_3->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    //Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }

            }


            if (virtual_max_distance > 0){//% create virtual needle pattern

                const int len_solid_part_mm = 6; // in virtual pattern, 6 out of 20 mm are solid, determined by BF by counting on IP images.

                // next 1, 4th lines  are different than for physical :
                // target_distances_beg_end =   DBDB_mm:DBDB_mm:virtual_max_distance+DBDB_mm;
                // i = find(target_distances_beg_end<virt_dist_begins);
                std::vector<double> target_distances_beg_end, target_distances_beg_end_temp;
                int ii = DBDB_mm;
                while (ii <= virtual_max_distance+DBDB_mm){
                    // Keep only values less or equal phys_dist_begins
                    //if (ii <= virt_dist_begins)
                    target_distances_beg_end.push_back(ii);
                    ii += DBDB_mm;
                }

                // Target_distances_beg_end = [  target_distances_beg_end    target_distances_beg_end-len_solid_part_mm ];
                target_distances_beg_end_temp.resize(target_distances_beg_end.size());
                for (int ii = 0; ii < target_distances_beg_end_temp.size(); ++ii)
                    target_distances_beg_end_temp[ii] = target_distances_beg_end[ii] - len_solid_part_mm;
                // Append the two vec.
                target_distances_beg_end.insert(target_distances_beg_end.end(), target_distances_beg_end_temp.begin(),target_distances_beg_end_temp.end());

                // Will travel travel the same direction the needle is pointing
                double dir2travel[3];
                dir2travel[0]= +1 * orig_one_RISC_IJK[0];
                dir2travel[1]= +1 * orig_one_RISC_IJK[1];
                dir2travel[2]= +1 * orig_one_RISC_IJK[2];

                // I can introduce the analysis/comparison to dist_tip_PSinter, dist_tip_MSinter
                // Here or inside plot_NA_dashes - do it inside.
                std::vector<double> additionalbreak;
                additionalbreak.reserve(2);

                // Next 2 lines are different than for physical :
                int sign_of_dist_tip_PSinter = (dist_tip_PSinter > 0) ? 1 :((dist_tip_PSinter < 0) ? -1 : 0);

                if (sign_of_dist_tip_PSinter == +1 && std::abs(dist_tip_PSinter)!= 999 && std::abs(dist_tip_PSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_PSinter));
                // Do abs() - I checked that direction is correct already, so submit it as a positive distance (in dir2travel direction)

                int sign_of_dist_tip_MSinter = (dist_tip_MSinter > 0) ? 1 :((dist_tip_MSinter < 0) ? -1 : 0);
                if (sign_of_dist_tip_MSinter == +1 && std::abs(dist_tip_MSinter)!=999 && std::abs(dist_tip_MSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_MSinter));
                // Sort the elements of target_distances_beg_end
                std::sort(target_distances_beg_end.begin(), target_distances_beg_end.end());
                // Remove all the duplicated elements
                auto last = std::unique(target_distances_beg_end.begin(), target_distances_beg_end.end());
                target_distances_beg_end.erase(last, target_distances_beg_end.end());


                /* Start plotting
                 * Part 0 - add dist_tip_PSinter, dist_tip_MSinter to the mix of begin - end pairs  if necc.
                 * If I need to break a dash into 2 pieces
                */
                if (additionalbreak.size() != 0){
                    double newbreak_val1val2[2];
                    for (size_t ii = 0; ii < target_distances_beg_end.size()/2 ; ++ii){
                        for (size_t jj = 0; jj < additionalbreak.size(); ++jj){
                            //double thisAB = additionalbreak[perAB] ;
                            if (additionalbreak[jj] > target_distances_beg_end[2*ii] && additionalbreak[jj] < target_distances_beg_end[2*ii +1]){
                                newbreak_val1val2[0] = additionalbreak[jj] - 0.05;
                                newbreak_val1val2[1] = additionalbreak[jj] + 0.05;
                            }
                        }
                    }
                    if (sizeof(newbreak_val1val2)/sizeof(newbreak_val1val2[0]) != 0)
                        target_distances_beg_end.insert(target_distances_beg_end.end(), newbreak_val1val2, newbreak_val1val2+2);
                }

                // Part 1 - determine the true 3-D locations of 3-D jumps, up to plotreal3Ddistance, and then the pixel locations to plot

                size_t Npts = target_distances_beg_end.size();
                // Pixel_locs_X_Y = zeros(Npts,2);
                std::vector< std::pair<double, double>> pixel_locs_X_Y(Npts);
                double thisloc_XYZmm[3];
                double unused_var2[3];

                for(size_t ii = 0; ii < Npts; ++ii){
                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + target_distances_beg_end[ii]*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + target_distances_beg_end[ii]*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + target_distances_beg_end[ii]*dir2travel[2];

                    // get the pixel location of this 3-D point
                    std::pair<double, double> pix = pix_3d_ptsfrom3d( thisloc_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, unused_var2 );
                    pixel_locs_X_Y[ii].first = pix.first;
                    pixel_locs_X_Y[ii].second = pix.second;
                }

                // part 2 -
                // learn if these 3-D points, "thisloc_XYZmm", are distal, proximal, or in-slice
                // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
                // http://mathworld.wolfram.com/Point-PlaneDistance.html
                // analyze the central point of each dash. The edges may be coincident with the PS or MS,...

                size_t Npairs = Npts/2;
                std::vector<double> dist4color_perpair(Npairs);

                for (size_t ii = 0;  ii < Npairs; ++ii){

                    double thisdist = (target_distances_beg_end[2*ii] + target_distances_beg_end[2*ii +1])/2;
                    double thisloc_XYZmm[3];

                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + thisdist*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + thisdist*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + thisdist*dir2travel[2];

                    // from MR image to needle line. distance = final - initial. From Mr image to 3-D needle line.
                    double temp[] = {thisloc_XYZmm[0] - TLC_XYZmm[0], thisloc_XYZmm[1] - TLC_XYZmm[1], thisloc_XYZmm[2] - TLC_XYZmm[2]};
                    double D = ImageNormal[0]*temp[0] + ImageNormal[1]*temp[1]+ ImageNormal[2]*temp[2];

                    dist4color_perpair[ii] = D;
                }

                // part 3 - fill in the solid pattern part, from beginning to end
                // pre-make with all blue. Find the places where the color is different than this.
                //colorperpair = abs(inplanecolor) * ones(1,Npairs);
                std::vector<int> colorperpair(Npairs, 98); // abs('b')=98 in Matlab

                // Change color to yellow
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] < -1*Slice_Thick/2)
                        // Change to yellow
                        colorperpair[ii] = 121;                    // abs('y') = 121 in Matlab
                }

                // Change to red
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] > +1*Slice_Thick/2)
                        // Change to red
                        colorperpair[ii] = 114;                    // abs('r') = 114 in Matlab
                }

                // Start plotting
                QVector<QPoint> pointPairs;
                for(size_t ii = 0; ii < Npairs; ++ii){
                    if (2*ii+1 < Npts)
                        pointPairs << QPoint(std::ceil(pixel_locs_X_Y[2*ii].first * window_2D_width/ImageNrows), std::ceil(pixel_locs_X_Y[2*ii+1].first * window_2D_width/ImageNrows)) // scaling factor
                                << QPoint(std::ceil(pixel_locs_X_Y[2*ii].second * window_2D_width/ImageNcols), std::ceil(pixel_locs_X_Y[2*ii+1].second * window_2D_width/ImageNcols));
                }

                // PROGRAMMING NOTE : Decide, if it is better in C, to first test if the points are in the image, or to always plot and take advantage of auto-clip.
                // plot( [ pt1(1)  pt2(1) ] , [ pt1(2)  pt2(2) ] , '-' , 'color', colorperpair(perpair), 'LineWidth' , NALineWidth );

                for (int ii = 0; ii < Npairs; ++ii){
                    if (colorperpair[ii] == 121){
                        QPen pen1(Qt::yellow, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());

                        // Get last dashed line coordinates (=Mahamadou=)
                        //line.setP1(QPointF(pointPairs[0].x(),  pointPairs[0].y()));
                        //line.setP2(QPointF(pointPairs[2*ii+1].x(),  pointPairs[2*ii+1].y()));
                    }
                    else if (colorperpair[ii] == 114){
                        QPen pen1(Qt::red, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                    else if (colorperpair[ii] == 98){
                        QPen pen1(Qt::blue, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                }

                /* Also plot - mark where the Needle line intersects the Plus Surface and the Minus surface
                *  (i) Plus Surface : 3-component distance from PS_TLC to PS intersect
                */
                if( (psi_centralpix.first >= -1) && (psi_centralpix.first <= ImageNcols-2) && (psi_centralpix.second >= -1)
                        && (psi_centralpix.second <= ImageNrows-2) && (std::abs(psi_centralpix.first) != +999))
                {
                    int target_x =  std::ceil(psi_centralpix.first * window_2D_width/ImageNrows); // scaling factor
                    int target_y =  std::ceil(psi_centralpix.second * window_2D_width/ImageNcols);
                    int target_r = 5;

                    QPen pen(Qt::blue, 2);
                    pen.setWidth(4);
                    painter->setPen(pen);

                    QVector<QPoint> pointPairs;
                    pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y + 2*target_r)
                               << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                    painter->drawLines(pointPairs);
                }

                if( (msi_centralpix.first >= -1) && (msi_centralpix.first <= ImageNcols-2) && (msi_centralpix.second >= -1)
                        && (msi_centralpix.second <= ImageNrows-2) && (std::abs(msi_centralpix.first)!= +999))
                {
                    int target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // scaling factor
                    int target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                    int target_r = 5;

                    QPen pen(Qt::blue, 2);
                    pen.setWidth(4);
                    painter->setPen(pen);

                    QVector<QPoint> pointPairs;
                    pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y + 2*target_r)
                               << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                    painter->drawLines(pointPairs);
                }

                // Show tip of the Needle
                bool ShowNeedleTip= true;
                if (ShowNeedleTip){
                    int target_x =  std::ceil(tip_plot_pix.first * window_2D_width/ImageNrows); // scaling factor
                    int target_y =  std::ceil(tip_plot_pix.second * window_2D_width/ImageNcols);
                    //int target_r = 5;

                    QPen pen(Qt::green, 2);
                    pen.setCapStyle(Qt::RoundCap);
                    pen.setWidth(15);
                    painter->setRenderHint(QPainter::Antialiasing,true);
                    painter->setPen(pen);
                    painter->drawPoint(QPoint(target_x, target_y));

                    //painter->drawLine(line);

                    /********************************************************************************************
                     * Draw arrow
                     * ******************************************************************************************/
                   /* QPen myPen(Qt::green);
                    qreal arrowSize = 20;
                    painter->setPen(myPen);
                    painter->setBrush(Qt::green);

                    double angle = ::acos(line.dx() / line.length());
                    if (line.dy() >= 0)
                        angle = (M_PI * 2) - angle;

                    line.setP1(QPointF(0.0, 0.0));
                    line.setP2(QPointF(0.0, 0.0));

                    QPointF arrowP1 = QPointF(target_x, target_y) + QPointF(sin(angle + M_PI / 3) * arrowSize,
                                                                            cos(angle + M_PI / 3) * arrowSize);
                    QPointF arrowP2 = QPointF(target_x, target_y) + QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize,
                                                                            cos(angle + M_PI - M_PI / 3) * arrowSize);

                    arrowHead.clear();
                    arrowHead << QPoint(target_x, target_y) << arrowP1 << arrowP2;
                    // Draw the arrow
                    painter->drawPolygon(arrowHead);*/

                }

                // Load the dicom file
                if (_count == 1){
                    ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    //Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 2){
                    ui->label_2->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 3){
                    ui->label_3->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
            }

        }

        else{ // Draw a needle annotation as target point

            // Parse the image orientation string
            double orientation_matrix[6];
            stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix, 6);

            // Get image orientation
            char *Letter_H = getOrientation_H(orientation_matrix);
            char *Letter_V = getOrientation_V(orientation_matrix);


            // Get the largest image pixel value
            //uint LargestImagePixelValue = /*std::stoi(sf.ToStringPair(gdcm::Tag(0x0028,0x0107)).second)*/ 1482;

            const gdcm::Image &gimage = ir.GetImage();
            std::vector<char> vbuffer;
            vbuffer.resize( gimage.GetBufferLength() );
            char *buffer = &vbuffer[0];

            // Convert dicom to RGB format
            QImage *imageQt = NULL;
            if( !ConvertToFormat_RGB888( gimage, buffer, imageQt ) )
            {
                std::cerr << "Error during file conversion: ConvertToFormat_RGB888\n";
            }


            // FOV calculation
            // FOVx = Raw * Pixel_spacing[0];
            // FOVy = Col * Pixel_spacing[1];
            std::string token = "\\";
            std::string s = sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second;

            long double FOVx = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second) *
                                          std::stod(s.substr(0, s.find(token))) + 0.5);

            s.erase(0, s.find(token) + token.length());

            long double FOVy = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second) *
                                          std::stod(s) + 0.5);


            // Keep aspect ratio
            QImage imageQt_Scaled = imageQt->scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

            // Write some Dicom Info on the display
            std::unique_ptr<QPainter> painter(new QPainter(&imageQt_Scaled));
            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));

            //Draw the text on the image
            painter->drawText( QPoint(2,14),    QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0010,0x0010)).second));
            painter->drawText( QPoint(2,28),    QString::fromStdString("FOV: " + std::to_string(FOVx) + "*" + std::to_string(FOVy)));

            painter->setPen(Qt::white);
            painter->setFont(QFont("Segoe UI",16));
            painter->drawText( QPoint(4,230),    QString::fromStdString(Letter_H));
            painter->drawText( QPoint(218,20),   QString::fromStdString(Letter_V));

            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));
            painter->drawText( QPoint(267,14),  QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).second));
            painter->drawText( QPoint(267,28),  QString::fromStdString("Date: " + sf.ToStringPair(gdcm::Tag(0x0008,0x0023)).second));
            painter->drawText( QPoint(2,390),   QString::fromStdString("TR: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0080)).second + " ms"));
            painter->drawText( QPoint(2,404),   QString::fromStdString("TE: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0081)).second + " ms"));
            painter->drawText( QPoint(2,418),   QString::fromStdString("FA: " + sf.ToStringPair(gdcm::Tag(0x0018,0x1314)).second + " deg"));


            /********** Draw target on image ***************************/

            /* 2 - learn where the Plus Surface and the Minus Surface are intersected by the 3-D needle annotation.
            * The result of this function is the plot locations on the MR image - "x" and "y", in whole/integer pixel units.
            * Result will = -999 if image plane and needle are parallel.
            * Also, this is the "general" solution, result may be <1, >ImageNrows, read "pixptsfrom3d" for more information.
            */
            std::pair<double, double> psi_centralpix, msi_centralpix;
            double newlineintersect_XYZmm[3]; // Unused variable, but needed for the first part.
            double newlineintersect_XYZmm_2[3]; // Unused variable

            psi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, PS_TLC_XYZmm, PS_BRC_XYZmm, PS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm ) ;
            msi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, MS_TLC_XYZmm, MS_BRC_XYZmm, MS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm_2) ;


            /* 3 - Where is the needle tip in relation to slab - distal to all, proximal to all, or inside the slab?

            * Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
            * http://mathworld.wolfram.com/Point-PlaneDistance.html

            * PROGRAMMING NOTE : in plot_NA_pixels.m, I measured this distance as " from MR image to needle line"
            * and the equation looked like : distance = final - initial, D = dot( ImageNormal , ( thisloc_XYZmm - TLC_XYZmm ) ) ;
            * However, here, the sign of the MRslice - tip distance matches the "Show Distance" reference
            * Octane annotation if the distance is done "from needle tip to MR slice"

            * PLEASE NOTE that D will be compared to HALF the SLICE THICKNESS -
            * "central" MR slice is half way between the PLUS Surface and the MINUS Surface
            */

            //D = dot( ImageNormal , ( TLC_XYZmm - locposPNT(1:3)) ) ;
            double temp[] = {TLC_XYZmm[0] - orig_one_RISC_XYZ[0], TLC_XYZmm[1] - orig_one_RISC_XYZ[1], TLC_XYZmm[2] - orig_one_RISC_XYZ[2]};
            long double D = ImageNormal[0]*temp[0] + ImageNormal[1]*temp[1] + ImageNormal[2]*temp[2];
            D = D /*- 69.5971*/;
            int target_x = -999; //Dummy values
            int target_y = -999;

            /* 3 possibilities :*/
            if (D > Slice_Thick/2){
                // 1) needle tip is proximal to the MR "slab" - physical needle has not entered nor exited the MR slab
                target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                const size_t target_r = 10;

                QPen pen(Qt::blue, 2);
                pen.setWidth(4);
                painter->setPen(pen);


                QVector<QPoint> pointPairs;
                pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y - target_r)
                           << QPoint(target_x, target_y + target_r) << QPoint(target_x, target_y + 2*target_r)
                           << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x - target_r, target_y)
                           << QPoint(target_x + target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                painter->drawEllipse(QPoint(target_x, target_y), target_r, target_r);
                painter->drawLines(pointPairs);

                // Show distance
                painter->setFont(QFont("Time New Roman",10,QFont::Bold));
                painter->drawText( QPoint(target_x+25,target_y+20),   QString::fromStdString(std::to_string(std::ceil(D)) + " mm"));

                // Load the dicom file
                if (_count == 1){
                    ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 2){
                    ui->label_2->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 3){
                    ui->label_3->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
            }
            else if(D>=-1*Slice_Thick/2 && D<=Slice_Thick/2){
                // 2) needle tip is IN the MR "slab" - physical needle has entered but not exitted the MR slab
                target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                const size_t target_r = 10;

                QPen pen(Qt::green, 2);
                pen.setWidth(4);
                painter->setPen(pen);


                QVector<QPoint> pointPairs;
                pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y - target_r)
                           << QPoint(target_x, target_y + target_r) << QPoint(target_x, target_y + 2*target_r)
                           << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x - target_r, target_y)
                           << QPoint(target_x + target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                painter->drawEllipse(QPoint(target_x, target_y), target_r, target_r);
                painter->drawLines(pointPairs);

                // Show distance
                painter->setFont(QFont("Time New Roman",10,QFont::Bold));
                painter->drawText( QPoint(target_x+25,target_y+20),   QString::fromStdString(std::to_string(std::ceil(D)) + " mm"));

                // Load the dicom file
                if (_count == 1){
                    ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 2){
                    ui->label_2->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 3){
                    ui->label_3->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }

            }
            else // or make it explicit : elseif D<-1*Slice_Thick/2,
            {
                // 3) needle tip is distal to the MR slab
                target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // scaling factor
                target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                const size_t target_r = 10;

                QPen pen(Qt::red, 2);
                pen.setWidth(4);
                painter->setPen(pen);


                QVector<QPoint> pointPairs;
                pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y - target_r)
                           << QPoint(target_x, target_y + target_r) << QPoint(target_x, target_y + 2*target_r)
                           << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x - target_r, target_y)
                           << QPoint(target_x + target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                painter->drawEllipse(QPoint(target_x, target_y), target_r, target_r);
                painter->drawLines(pointPairs);

                // Show distance
                painter->setFont(QFont("Time New Roman",10,QFont::Bold));
                painter->drawText( QPoint(target_x+25,target_y+20),   QString::fromStdString(std::to_string(std::ceil(D)) + " mm"));

                // Load the dicom file
                if (_count == 1){
                    ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 2){
                    ui->label_2->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    // Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
                else if(_count == 3){
                    ui->label_3->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                    ui->label_3->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    //update();

                    //Save image
                    auto myString =  QTime::currentTime().toString();
                    QString hours   = myString.mid(0, 2);
                    QString minutes = myString.mid(3, 2);
                    QString seconds = myString.mid(6, 2);
                    Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                                  QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
                }
            }

        }

        ++_count;
    }
}

void MainWindow::on_pushButton_toggled(bool checked)
{
    checked ? do_measure = true : do_measure = false;
}


void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if(urls.isEmpty())
        return;

    QString filename = urls.first().toLocalFile();

    if(filename.isEmpty())
        return;

    // Read file
    FindFile_dropped(filename);
    event->acceptProposedAction();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    m_nbMousePressed = true;
    m_nInitialX = event->x();
    m_nInitialY = event->y();
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_nbMousePressed = false;
    m_nFinalX = event->x();
    m_nFinalY = event->y();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->type() == QEvent::MouseMove )
    {
        m_nFinalX = event->x();
        m_nFinalY = event->y();
    }
   // m_nPTargetPixmap = new QPixmap("image.png");
    update(); // update your view
}

void MainWindow::paintEvent(QPaintEvent *)
{
    if ( m_nbMousePressed && do_measure)
    {
        QPainter painter(m_nPTargetPixmap);
        painter.setPen(QPen(Qt::red,1));
        painter.setFont(QFont("Segoe UI", 10));
        double Distance = std::sqrt( std::pow((m_nFinalX - m_nInitialX), 2) + std::pow((m_nFinalY - m_nInitialY), 2));

        painter.drawLine(m_nInitialX, m_nInitialY, m_nFinalX, m_nFinalY);

        painter.setPen(QPen(Qt::white, 4));
        painter.drawText(QPointF(m_nInitialX + (m_nFinalX - m_nInitialX)/2 + 5 , m_nInitialY + (m_nFinalY - m_nInitialY)/2 + 5),
                         QString::number(Distance)+ QString(" pix"));
        painter.drawPoint(QPointF(m_nInitialX + (m_nFinalX - m_nInitialX)/2 , m_nInitialY + (m_nFinalY - m_nInitialY)/2));
    }
    //ui->label_1->setPixmap(*m_nPTargetPixmap);
}

bool MainWindow::ConvertToFormat_RGB888(const gdcm::Image &gimage, char *buffer, QImage *&imageQt)
{
    const uint* dimension = gimage.GetDimensions();

    uint dimX = dimension[0];
    uint dimY = dimension[1];

    gimage.GetBuffer(buffer);

    // This section is for Siemens Dicom ONLY (Pixel representation is UNSIGNED i.e 0)
    // Some modifications are necessary to load dicom from other vendors.
    if( gimage.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::MONOCHROME2 &&
            gimage.GetPixelFormat() == gdcm::PixelFormat::UINT16)
    {
        // We need to copy each individual 16 bits into R / G and B (truncate value)
        short *buffer16 = (short*)buffer;
        uchar *ubuffer = new uchar[dimX*dimY*3];
        uchar *pubuffer = ubuffer;

        // Compute the largest image pixel value
        short LargestImagePixelValue = 0;
        for(uint ii = 0; ii < dimX*dimY; ++ii)
            LargestImagePixelValue = std::max(LargestImagePixelValue, *(buffer16 + ii));


        for(uint i = 0; i < dimX*dimY; i++)
        {
            if(LargestImagePixelValue != 0){
                // Linear scaling of the dicom image
                *pubuffer++ = (uchar) 255 * *buffer16 / LargestImagePixelValue;
                *pubuffer++ = (uchar) 255 * *buffer16 / LargestImagePixelValue;
                *pubuffer++ = (uchar) 255 * *buffer16 / LargestImagePixelValue;
                buffer16++;
            }else{std::cerr << "LargestImagePixelValue = 0.\nThis should never happen, abort...\n";}
        }

        imageQt = new QImage(ubuffer, dimX, dimY, QImage::Format_RGB888);
    }

    else
    {
        std::cerr << "Unhandled PhotometricInterpretation: " << gimage.GetPhotometricInterpretation() << std::endl;
        std::cerr << "Pixel Format is: " << gimage.GetPixelFormat() << std::endl;
        return false;
    }

    return true;
}

void MainWindow::delay(int msec)
{
    //QTime dieTime= QTime::currentTime().addSecs(second);
    QTime dieTime= QTime::currentTime().addMSecs(msec);
    while( QTime::currentTime() < dieTime )
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void MainWindow::stringParser(string s, double *outputMatrix, size_t size)
{
    std::string token = "\\";
    for (size_t ii = 0; ii < size; ++ii){
        outputMatrix[ii] = std::stod(s.substr(0, s.find(token)));
        s.erase(0, s.find(token) + token.length());
    }
}

char *MainWindow::getOrientation_H(double orientation_Matrix[])
{
    char *orientation = new char[4];
    char *optr = orientation;
    *optr='\0';

    char orientationX = orientation_Matrix[0] > 0.0 ? 'R' : 'L';
    char orientationY = orientation_Matrix[1] > 0.0 ? 'A' : 'P';
    char orientationZ = orientation_Matrix[2] > 0.0 ? 'F' : 'H';

    double absX = fabs(orientation_Matrix[0]);
    double absY = fabs(orientation_Matrix[1]);
    double absZ = fabs(orientation_Matrix[2]);

    for (int i=0; i<3; ++i) {
        if (absX>.0001 && absX>absY && absX>absZ) {
            *optr++=orientationX;
            absX=0;
        }
        else if (absY>.0001 && absY>absX && absY>absZ) {
            *optr++=orientationY;
            absY=0;
        }
        else if (absZ>.0001 && absZ>absX && absZ>absY) {
            *optr++=orientationZ;
            absZ=0;
        }
        else break;
        *optr='\0';
    }

    return orientation;
}

char *MainWindow::getOrientation_V(double orientation_Matrix[])
{
    char *orientation = new char[4];
    char *optr = orientation;
    *optr='\0';

    char orientationX = orientation_Matrix[3] > 0.0 ? 'R' : 'L';
    char orientationY = orientation_Matrix[4] > 0.0 ? 'A' : 'P';
    char orientationZ = orientation_Matrix[5] > 0.0 ? 'F' : 'H';

    double absX = fabs(orientation_Matrix[3]);
    double absY = fabs(orientation_Matrix[4]);
    double absZ = fabs(orientation_Matrix[5]);

    for (int i=0; i<3; ++i) {
        if (absX>.0001 && absX>absY && absX>absZ) {
            *optr++=orientationX;
            absX=0;
        }
        else if (absY>.0001 && absY>absX && absY>absZ) {
            *optr++=orientationY;
            absY=0;
        }
        else if (absZ>.0001 && absZ>absX && absZ>absY) {
            *optr++=orientationZ;
            absZ=0;
        }
        else break;
        *optr='\0';
    }

    return orientation;
}

void MainWindow::FindFile_dropped(QString filename)
{
    //QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::currentPath());

    if (!filename.isNull())
    {
        // Convert Qstring to string literal
        QByteArray byteArray = filename.toUtf8();
        const char *cString = byteArray.constData();

        // Read input dicom file
        gdcm::ImageReader ir;
        ir.SetFileName( cString );
        if(!ir.Read())
        {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("The following file is not a supported DICOM file %1.").arg(QDir::toNativeSeparators(cString)));

            return;
        }

        // Let's get some info from the header file
        gdcm::File &file = ir.GetFile();
        gdcm::StringFilter  sf = gdcm::StringFilter();
        sf.SetFile(file);

        // Parse the image orientation string
        double orientation_matrix[6];
        stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix, 6);

        // Get image orientation
        char *Letter_H = getOrientation_H(orientation_matrix);
        char *Letter_V = getOrientation_V(orientation_matrix);

        const gdcm::Image &gimage = ir.GetImage();
        std::vector<char> vbuffer;
        vbuffer.resize( gimage.GetBufferLength() );
        char *buffer = &vbuffer[0];

        // Convert dicom to RGB format
        QImage *imageQt = NULL;
        if( !ConvertToFormat_RGB888( gimage, buffer, imageQt ) )
        {
            std::cerr << "Error during file conversion: ConvertToFormat_RGB888\n";
        }

        // FOV calculation
        // FOVx = Raw * Pixel_spacing[0];
        // FOVy = Col * Pixel_spacing[1];
        std::string token = "\\";
        std::string s = sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second;

        long double FOVx = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second) *
                                      std::stod(s.substr(0, s.find(token))) + 0.5);

        s.erase(0, s.find(token) + token.length());

        long double FOVy = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second) *
                                      std::stod(s) + 0.5);

        // Keep aspect ratio
        QImage imageQt_Scaled = imageQt->scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

        // Write some Dicom Info on the display
        std::unique_ptr<QPainter> painter(new QPainter(&imageQt_Scaled));
        painter->setPen(Qt::green);
        painter->setFont(QFont("Segoe UI", 11));

        //Draw the text on the image
        painter->drawText( QPoint(2,14),    QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0010,0x0010)).second));
        painter->drawText( QPoint(2,28),    QString::fromStdString("FOV: " + std::to_string(FOVx) + "*" + std::to_string(FOVy)));

        painter->setPen(Qt::white);
        painter->setFont(QFont("Segoe UI",16));
        painter->drawText( QPoint(4,230),    QString::fromStdString(Letter_H));
        painter->drawText( QPoint(218,20),   QString::fromStdString(Letter_V));

        painter->setPen(Qt::green);
        painter->setFont(QFont("Segoe UI", 11));
        painter->drawText( QPoint(267,14),  QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).second));
        painter->drawText( QPoint(267,28),  QString::fromStdString("Date: " + sf.ToStringPair(gdcm::Tag(0x0008,0x0023)).second));
        painter->drawText( QPoint(2,390),   QString::fromStdString("TR: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0080)).second + " ms"));
        painter->drawText( QPoint(2,404),   QString::fromStdString("TE: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0081)).second + " ms"));
        painter->drawText( QPoint(2,418),   QString::fromStdString("FA: " + sf.ToStringPair(gdcm::Tag(0x0018,0x1314)).second + " deg"));


        // Determine widget name
        QLabel* label= static_cast<QLabel*>(QWidget::childAt(QCursor::pos()));
        // Load the image
        label->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
        label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        update();
    }
}

void MainWindow::autoLoader(QString filename)
{
    if (!filename.isNull())
    {
        // Convert Qstring to string literal
        QByteArray byteArray = filename.toUtf8();
        const char *cString = byteArray.constData();

        // Read input dicom file
        gdcm::ImageReader ir;
        ir.SetFileName( cString );
        if(!ir.Read())
        {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("The following file is not a supported DICOM file %1.").arg(QDir::toNativeSeparators(cString)));

            return;
        }

        // Let's get some info from the header file
        gdcm::File &file = ir.GetFile();
        gdcm::StringFilter  sf = gdcm::StringFilter();
        sf.SetFile(file);

        // Parse the image orientation string
        double orientation_matrix[6];
        stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix, 6);

        // Get image orientation
        char *Letter_H = getOrientation_H(orientation_matrix);
        char *Letter_V = getOrientation_V(orientation_matrix);

        const gdcm::Image &gimage = ir.GetImage();
        std::vector<char> vbuffer;
        vbuffer.resize( gimage.GetBufferLength() );
        char *buffer = &vbuffer[0];

        // Convert dicom to RGB format
        QImage *imageQt = NULL;
        if( !ConvertToFormat_RGB888( gimage, buffer, imageQt ) )
        {
            std::cerr << "Error during file conversion: ConvertToFormat_RGB888\n";
        }

        // FOV calculation
        // FOVx = Raw * Pixel_spacing[0];
        // FOVy = Col * Pixel_spacing[1];
        std::string token = "\\";
        std::string s = sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second;

        long double FOVx = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second) *
                                      std::stod(s.substr(0, s.find(token))) + 0.5);

        s.erase(0, s.find(token) + token.length());

        long double FOVy = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second) *
                                      std::stod(s) + 0.5);

        // Keep aspect ratio
        QImage imageQt_Scaled = imageQt->scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

        // Write some Dicom Info on the display
        std::unique_ptr<QPainter> painter(new QPainter(&imageQt_Scaled));
        painter->setPen(Qt::green);
        painter->setFont(QFont("Segoe UI", 11));

        //Draw the text on the image
        painter->drawText( QPoint(2,14),    QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0010,0x0010)).second));
        painter->drawText( QPoint(2,28),    QString::fromStdString("FOV: " + std::to_string(FOVx) + "*" + std::to_string(FOVy)));

        painter->setPen(Qt::white);
        painter->setFont(QFont("Segoe UI",16));
        painter->drawText( QPoint(4,230),    QString::fromStdString(Letter_H));
        painter->drawText( QPoint(218,20),   QString::fromStdString(Letter_V));

        painter->setPen(Qt::green);
        painter->setFont(QFont("Segoe UI", 11));
        painter->drawText( QPoint(267,14),  QString::fromStdString(/*sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).first + ": "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  +*/sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).second));
        painter->drawText( QPoint(267,28),  QString::fromStdString("Date: " + sf.ToStringPair(gdcm::Tag(0x0008,0x0023)).second));
        painter->drawText( QPoint(2,390),   QString::fromStdString("TR: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0080)).second + " ms"));
        painter->drawText( QPoint(2,404),   QString::fromStdString("TE: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0081)).second + " ms"));
        painter->drawText( QPoint(2,418),   QString::fromStdString("FA: " + sf.ToStringPair(gdcm::Tag(0x0018,0x1314)).second + " deg"));


        // Determine widget name
        //QLabel* label= static_cast<QLabel*>(QWidget::childAt(QCursor::pos()));
        // Load the image
        ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
        ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        update();
    }
}

std::pair<double, double> MainWindow::pix_3d_ptsfrom3d(double newlinept1xyz[], double newlinenorm[], double TLC_XYZmm[], double BRC_XYZmm[], double TRC_XYZmm[], double Im_rowdircoldir[], double thisPixSizeLocalYX[], double *newlineintersect_XYZmm)
{
    // Declare some variables
    double plot_local_x = 0.0;
    double plot_local_y = 0.0;
    std::pair<double, double> target_coord;

    /* Obtain the "local-X", "local-Y" pixels to plot on the current MR image.
     * This is the "projection" of a 3-D point in space (which is not in the MR slice) onto the MR slice/plane.

     * part i - create a new line with point1=input 3-D point, direction Unit Vector = input 3-D direction
     * this may describe the needle line, or some imaginary/constructed line (i.e a line coming off the image plane, with dir. = image normal) - whatever is required by the calling function.

     * OK :  newlinept1xyz , newlinenorm .

     *  part ii - obtain the 2-points form of this line
    */

    double arbdist = 1000.00 ; // arbitrary distance, large.
    double newlinept2xyz[3];
    newlinept2xyz[0] = newlinept1xyz[0] + arbdist*newlinenorm[0];
    newlinept2xyz[1] = newlinept1xyz[1] + arbdist*newlinenorm[1];
    newlinept2xyz[2] = newlinept1xyz[2] + arbdist*newlinenorm[2];

    /* part iii - obtain intersection

     * A - intersection with "central/usual" image plane.
     * From : Eric W. Weisstein. "Line-Plane Intersection." From MathWorld--A Wolfram Web Resource.
     * http://mathworld.wolfram.com/Line-PlaneIntersection.html
    */

    double  NUMERiMATRIX[4][4] = {    1,              1,               1,                  1,
                                      TLC_XYZmm[0],     BRC_XYZmm[0],    TRC_XYZmm[0],     newlinept1xyz[0],
                                      TLC_XYZmm[1],     BRC_XYZmm[1],    TRC_XYZmm[1],     newlinept1xyz[1],
                                      TLC_XYZmm[2],     BRC_XYZmm[2],    TRC_XYZmm[2],     newlinept1xyz[2]} ;

    double DENOMiMATRIX[4][4] = {    1,              1,               1,                  0,
                                     TLC_XYZmm[0],     BRC_XYZmm[0],    TRC_XYZmm[0],    newlinept2xyz[0]-newlinept1xyz[0],
                                     TLC_XYZmm[1],     BRC_XYZmm[1],    TRC_XYZmm[1],    newlinept2xyz[1]-newlinept1xyz[1],
                                     TLC_XYZmm[2],     BRC_XYZmm[2],    TRC_XYZmm[2],    newlinept2xyz[2]-newlinept1xyz[2]};

    // Compute the determinant of the two 4x4 matrices
    double a =  NUMERiMATRIX[0][3] * NUMERiMATRIX[1][2] * NUMERiMATRIX[2][1] * NUMERiMATRIX[3][0] - NUMERiMATRIX[0][2] * NUMERiMATRIX[1][3] * NUMERiMATRIX[2][1] * NUMERiMATRIX[3][0] - NUMERiMATRIX[0][3] * NUMERiMATRIX[1][1] * NUMERiMATRIX[2][2] * NUMERiMATRIX[3][0] + NUMERiMATRIX[0][1] * NUMERiMATRIX[1][3] * NUMERiMATRIX[2][2] * NUMERiMATRIX[3][0]+
            NUMERiMATRIX[0][2] * NUMERiMATRIX[1][1] * NUMERiMATRIX[2][3] * NUMERiMATRIX[3][0] - NUMERiMATRIX[0][1] * NUMERiMATRIX[1][2] * NUMERiMATRIX[2][3] * NUMERiMATRIX[3][0] - NUMERiMATRIX[0][3] * NUMERiMATRIX[1][2] * NUMERiMATRIX[2][0] * NUMERiMATRIX[3][1] + NUMERiMATRIX[0][2] * NUMERiMATRIX[1][3] * NUMERiMATRIX[2][0] * NUMERiMATRIX[3][1]+
            NUMERiMATRIX[0][3] * NUMERiMATRIX[1][0] * NUMERiMATRIX[2][2] * NUMERiMATRIX[3][1] - NUMERiMATRIX[0][0] * NUMERiMATRIX[1][3] * NUMERiMATRIX[2][2] * NUMERiMATRIX[3][1] - NUMERiMATRIX[0][2] * NUMERiMATRIX[1][0] * NUMERiMATRIX[2][3] * NUMERiMATRIX[3][1] + NUMERiMATRIX[0][0] * NUMERiMATRIX[1][2] * NUMERiMATRIX[2][3] * NUMERiMATRIX[3][1]+
            NUMERiMATRIX[0][3] * NUMERiMATRIX[1][1] * NUMERiMATRIX[2][0] * NUMERiMATRIX[3][2] - NUMERiMATRIX[0][1] * NUMERiMATRIX[1][3] * NUMERiMATRIX[2][0] * NUMERiMATRIX[3][2] - NUMERiMATRIX[0][3] * NUMERiMATRIX[1][0] * NUMERiMATRIX[2][1] * NUMERiMATRIX[3][2] + NUMERiMATRIX[0][0] * NUMERiMATRIX[1][3] * NUMERiMATRIX[2][1] * NUMERiMATRIX[3][2]+
            NUMERiMATRIX[0][1] * NUMERiMATRIX[1][0] * NUMERiMATRIX[2][3] * NUMERiMATRIX[3][2] - NUMERiMATRIX[0][0] * NUMERiMATRIX[1][1] * NUMERiMATRIX[2][3] * NUMERiMATRIX[3][2] - NUMERiMATRIX[0][2] * NUMERiMATRIX[1][1] * NUMERiMATRIX[2][0] * NUMERiMATRIX[3][3] + NUMERiMATRIX[0][1] * NUMERiMATRIX[1][2] * NUMERiMATRIX[2][0] * NUMERiMATRIX[3][3]+
            NUMERiMATRIX[0][2] * NUMERiMATRIX[1][0] * NUMERiMATRIX[2][1] * NUMERiMATRIX[3][3] - NUMERiMATRIX[0][0] * NUMERiMATRIX[1][2] * NUMERiMATRIX[2][1] * NUMERiMATRIX[3][3] - NUMERiMATRIX[0][1] * NUMERiMATRIX[1][0] * NUMERiMATRIX[2][2] * NUMERiMATRIX[3][3] + NUMERiMATRIX[0][0] * NUMERiMATRIX[1][1] * NUMERiMATRIX[2][2] * NUMERiMATRIX[3][3];

    double b =  DENOMiMATRIX[0][3] * DENOMiMATRIX[1][2] * DENOMiMATRIX[2][1] * DENOMiMATRIX[3][0] - DENOMiMATRIX[0][2] * DENOMiMATRIX[1][3] * DENOMiMATRIX[2][1] * DENOMiMATRIX[3][0] - DENOMiMATRIX[0][3] * DENOMiMATRIX[1][1] * DENOMiMATRIX[2][2] * DENOMiMATRIX[3][0] + DENOMiMATRIX[0][1] * DENOMiMATRIX[1][3] * DENOMiMATRIX[2][2] * DENOMiMATRIX[3][0]+
            DENOMiMATRIX[0][2] * DENOMiMATRIX[1][1] * DENOMiMATRIX[2][3] * DENOMiMATRIX[3][0] - DENOMiMATRIX[0][1] * DENOMiMATRIX[1][2] * DENOMiMATRIX[2][3] * DENOMiMATRIX[3][0] - DENOMiMATRIX[0][3] * DENOMiMATRIX[1][2] * DENOMiMATRIX[2][0] * DENOMiMATRIX[3][1] + DENOMiMATRIX[0][2] * DENOMiMATRIX[1][3] * DENOMiMATRIX[2][0] * DENOMiMATRIX[3][1]+
            DENOMiMATRIX[0][3] * DENOMiMATRIX[1][0] * DENOMiMATRIX[2][2] * DENOMiMATRIX[3][1] - DENOMiMATRIX[0][0] * DENOMiMATRIX[1][3] * DENOMiMATRIX[2][2] * DENOMiMATRIX[3][1] - DENOMiMATRIX[0][2] * DENOMiMATRIX[1][0] * DENOMiMATRIX[2][3] * DENOMiMATRIX[3][1] + DENOMiMATRIX[0][0] * DENOMiMATRIX[1][2] * DENOMiMATRIX[2][3] * DENOMiMATRIX[3][1]+
            DENOMiMATRIX[0][3] * DENOMiMATRIX[1][1] * DENOMiMATRIX[2][0] * DENOMiMATRIX[3][2] - DENOMiMATRIX[0][1] * DENOMiMATRIX[1][3] * DENOMiMATRIX[2][0] * DENOMiMATRIX[3][2] - DENOMiMATRIX[0][3] * DENOMiMATRIX[1][0] * DENOMiMATRIX[2][1] * DENOMiMATRIX[3][2] + DENOMiMATRIX[0][0] * DENOMiMATRIX[1][3] * DENOMiMATRIX[2][1] * DENOMiMATRIX[3][2]+
            DENOMiMATRIX[0][1] * DENOMiMATRIX[1][0] * DENOMiMATRIX[2][3] * DENOMiMATRIX[3][2] - DENOMiMATRIX[0][0] * DENOMiMATRIX[1][1] * DENOMiMATRIX[2][3] * DENOMiMATRIX[3][2] - DENOMiMATRIX[0][2] * DENOMiMATRIX[1][1] * DENOMiMATRIX[2][0] * DENOMiMATRIX[3][3] + DENOMiMATRIX[0][1] * DENOMiMATRIX[1][2] * DENOMiMATRIX[2][0] * DENOMiMATRIX[3][3]+
            DENOMiMATRIX[0][2] * DENOMiMATRIX[1][0] * DENOMiMATRIX[2][1] * DENOMiMATRIX[3][3] - DENOMiMATRIX[0][0] * DENOMiMATRIX[1][2] * DENOMiMATRIX[2][1] * DENOMiMATRIX[3][3] - DENOMiMATRIX[0][1] * DENOMiMATRIX[1][0] * DENOMiMATRIX[2][2] * DENOMiMATRIX[3][3] + DENOMiMATRIX[0][0] * DENOMiMATRIX[1][1] * DENOMiMATRIX[2][2] * DENOMiMATRIX[3][3];

    //double newlineintersect_XYZmm[3];

    if (b!=0){

        double t = a/b ;


        newlineintersect_XYZmm[0] = newlinept1xyz[0] + t * (  newlinept1xyz[0] - newlinept2xyz[0]  ) ;
        newlineintersect_XYZmm[1] = newlinept1xyz[1] + t * (  newlinept1xyz[1] - newlinept2xyz[1]  ) ;
        newlineintersect_XYZmm[2] = newlinept1xyz[2] + t * (  newlinept1xyz[2] - newlinept2xyz[2]  ) ;
    }
    else{

        std::cout << "Determinant of the denominator of the matrix is zero, this plane and this line are parallel, no intersection, giving dummy value of -999 -999 for pixel plot values.\n";
        plot_local_x = -999.0;
        plot_local_y = -999.0;
        newlineintersect_XYZmm[0] = +999;
        newlineintersect_XYZmm[1] = +999;
        newlineintersect_XYZmm[2] = +999;
    }

    // part iv - travel from TLC (Top Left Corner) to this point, along RowDir and ColDir, distance = Final - Initial.
    double W[3];
    if (newlineintersect_XYZmm[0]!= 999 && newlineintersect_XYZmm[1] != 999 && newlineintersect_XYZmm[2] != 999){
        W[0] = newlineintersect_XYZmm[0] - TLC_XYZmm[0];
        W[1] = newlineintersect_XYZmm[1] - TLC_XYZmm[1];
        W[2] = newlineintersect_XYZmm[2] - TLC_XYZmm[2];
    }

    // project it onto the rowdir and coldir
    // rowdirdist_mm = dot(  Im_rowdircoldir(1:3) , W );
    double rowdirdist_mm = Im_rowdircoldir[0]*W[0] + Im_rowdircoldir[1]*W[1] + Im_rowdircoldir[2]*W[2];

    //double coldirdist_mm = dot(  Im_rowdircoldir(4:6) , W );
    double coldirdist_mm = Im_rowdircoldir[3]*W[0] + Im_rowdircoldir[4]*W[1] + Im_rowdircoldir[5]*W[2];

    // part v - then translate it into pixel distances and locations.
    // PROGRAMMING NOTE : Adding +1 because in Matlab, the first pixel value is placed at 1, not zero.
    // Always round pixel locations if you want to mimic the MR scanner's method of needle annotations as whole-pixel entities.
    // 7/25/06 - remove the rounding of the division below; not really needed.

    plot_local_x = rowdirdist_mm  /  thisPixSizeLocalYX[1]  ;  //  / PixelSize  ;  to go from mm to Pixels . what gets plotted is integer "x" and "y"
    plot_local_y = coldirdist_mm  /  thisPixSizeLocalYX[0]  ;  //  / PixelSize  ;  to go from mm to Pixels . what gets plotted is integer "x" and "y"

    target_coord.first = plot_local_x;
    target_coord.second = plot_local_y;

    return target_coord;
}

std::tuple<double, double, double, double> MainWindow::precise_phys_virt_na_dist(double thisFOVmm_LocalYX[], double thisPixSizeLocalYX[], double ImageNcols, double ImageNrows, double TLC_XYZmm[], double TRC_XYZmm[], double BRC_XYZmm[], double ImageNormal[], double Im_rowdircoldir[], double one_XYZ_pLPH[], double one_IJK_pLPH[])
{
    // Initialize the return values
    double physical_max_distance = 0.0;
    double phys_dist_begins      = 0.0;
    double virtual_max_distance  = 0.0;
    double virt_dist_begins      = 0.0;

    /* 11/9/09 - IMPORTANT FIX -
    * projecting in direction "ImageNormal" can be an arbitrary distance,
    * but projecting in either of "Im_rowdircoldir" must be that size in FOV.
    */
    double arbdist4INDonly = std::max(thisFOVmm_LocalYX[0], thisFOVmm_LocalYX[1]);

    // see pencil notes, pages 26-29.
    // "PLane 1" = Shares an edge along the top of the MR image, :
    // PL1pt1 =  TLC_XYZmm  ;
    double PL1pt2[] = { TLC_XYZmm[0]+arbdist4INDonly*ImageNormal[0],  TLC_XYZmm[1]+arbdist4INDonly*ImageNormal[1],  TLC_XYZmm[2]+arbdist4INDonly*ImageNormal[2]};
    // PL1pt3 =  TRC_XYZmm  ;

    // "PLane 2" = Shares an edge along the right side of the MR image, :
    // PL2pt1 =  TRC_XYZmm  ;
    // PL2pt2 =  BRC_XYZmm  ;
    double PL2pt3[] = { TRC_XYZmm[0]+arbdist4INDonly*ImageNormal[0],  TRC_XYZmm[1]+arbdist4INDonly*ImageNormal[1],  TRC_XYZmm[2]+arbdist4INDonly*ImageNormal[2]};

    // "PLane 3" = Shares an edge along the bottom of the MR image, :
    // do like for TRC, BRC calcs - distance is _NOT_ FOVmm, it is 1 pixel less, in mm
    //PL3pt1 =  BRC_XYZmm  ;
    double PL3pt2[3];
    PL3pt2[0] = TLC_XYZmm[0]+(thisFOVmm_LocalYX[0]-thisPixSizeLocalYX[0])*Im_rowdircoldir[3] ;
    PL3pt2[1] = TLC_XYZmm[1]+(thisFOVmm_LocalYX[0]-thisPixSizeLocalYX[0])*Im_rowdircoldir[4] ;
    PL3pt2[2] = TLC_XYZmm[2]+(thisFOVmm_LocalYX[0]-thisPixSizeLocalYX[0])*Im_rowdircoldir[5] ;
    double PL3pt3[] = { BRC_XYZmm[0]+arbdist4INDonly*ImageNormal[0],  BRC_XYZmm[1]+arbdist4INDonly*ImageNormal[1],  BRC_XYZmm[2]+arbdist4INDonly*ImageNormal[2]};

    // "PLane 4" = Shares an edge along the left side of the MR image, :
    //PL4pt1 =  TLC_XYZmm ;
    //PL4pt2 =  PL3pt2  ; % so don't change the order above !
    //PL4pt3 =  PL1pt2  ; % so don't change the order above !

    // Where does the 3-D needle intersect these 3-D planes ?
    double PL1_XYZmm[3];
    double PL2_XYZmm[3];
    double PL3_XYZmm[3];
    double PL4_XYZmm[3];
    std::pair<double, double> unused_1 = pix_3d_ptsfrom3d( one_XYZ_pLPH , one_IJK_pLPH , TLC_XYZmm, PL1pt2,  TRC_XYZmm, Im_rowdircoldir, thisPixSizeLocalYX, PL1_XYZmm );
    std::pair<double, double> unused_2 = pix_3d_ptsfrom3d( one_XYZ_pLPH , one_IJK_pLPH , TRC_XYZmm, BRC_XYZmm, PL2pt3, Im_rowdircoldir, thisPixSizeLocalYX, PL2_XYZmm );
    std::pair<double, double> unused_3 = pix_3d_ptsfrom3d( one_XYZ_pLPH , one_IJK_pLPH , BRC_XYZmm, PL3pt2, PL3pt3, Im_rowdircoldir, thisPixSizeLocalYX, PL3_XYZmm );
    std::pair<double, double> unused_4 = pix_3d_ptsfrom3d( one_XYZ_pLPH , one_IJK_pLPH , TLC_XYZmm, PL3pt2, PL1pt2, Im_rowdircoldir, thisPixSizeLocalYX, PL4_XYZmm );


    /*NOTE : "PL[x]_XYZmm" is a point on the 3-D needle line. It is _not_ a point on the MR image.
     * Where do these 3-D points lie on the MR image, if anywhere?
     * nn2sd = 20 ;
     * do the +-0.001 because round off error occurs, will get a result of 1.0000002503551049 instead of 1.0
     * 4 put the disp's in the if statement, before the fixing-equation, to learn all the times this occurs. will occur almost every time.
    */
    std::pair<double, double> PL1_pix, PL2_pix, PL3_pix, PL4_pix;
    double unused_5[3];
    double unused_6[3];
    double unused_7[3];
    double unused_8[3];


    PL1_pix = pix_3d_ptsfrom3d( PL1_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, thisPixSizeLocalYX, unused_5 );
    if (PL1_pix.first+1 > 1-0.001  &&  PL1_pix.first+1 < 1+0.001)
        PL1_pix.first = 1;
    if (PL1_pix.second+1 > 1-0.001 && PL1_pix.second+1 < 1+0.001)
        PL1_pix.second = 1;
    if (PL1_pix.first+1 > ImageNcols-0.001 && PL1_pix.first+1 <= ImageNcols+0.001)
        PL1_pix.first = ImageNcols;
    if (PL1_pix.second+1 > ImageNrows-0.001 && PL1_pix.second+1 <= ImageNrows+0.001)
        PL1_pix.second = ImageNrows;

    PL2_pix = pix_3d_ptsfrom3d( PL2_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, thisPixSizeLocalYX, unused_6);
    if (PL2_pix.first+1 > 1-0.001 && PL2_pix.first+1 < 1+0.001)
        PL2_pix.first = 1;
    if (PL2_pix.second+1 > 1-0.001 && PL2_pix.second+1 < 1+0.001)
        PL2_pix.second = 1;
    if (PL2_pix.first+1 > ImageNcols-0.001 && PL2_pix.first+1 <= ImageNcols+0.001)
        PL2_pix.first = ImageNcols;
    if (PL2_pix.second+1 > ImageNrows-0.001 && PL2_pix.second+1 <= ImageNrows+0.001)
        PL2_pix.second = ImageNrows;

    PL3_pix = pix_3d_ptsfrom3d( PL3_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, thisPixSizeLocalYX, unused_7 );
    if (PL3_pix.first+1 > 1-0.001  &&  PL3_pix.first+1 < 1+0.001)
        PL3_pix.first = 1;
    if (PL3_pix.second+1 > 1-0.001  &&  PL3_pix.second+1 < 1+0.001)
        PL3_pix.second = 1;
    if (PL3_pix.first+1 > ImageNcols-0.001 && PL3_pix.first+1 <= ImageNcols+0.001)
        PL3_pix.first = ImageNcols;
    if (PL3_pix.second+1 > ImageNrows-0.001 && PL3_pix.second+1 <= ImageNrows+0.001)
        PL3_pix.second = ImageNrows;

    PL4_pix = pix_3d_ptsfrom3d( PL4_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, thisPixSizeLocalYX, unused_8 );
    if (PL4_pix.first+1 > 1-0.001   && PL4_pix.first+1 < 1+0.001)
        PL4_pix.first = 1;
    if (PL4_pix.second+1 > 1-0.001  && PL4_pix.second+1 < 1+0.001)
        PL4_pix.second = 1;
    if (PL4_pix.first+1 > ImageNcols-0.001 && PL4_pix.first+1 <= ImageNcols+0.001)
        PL4_pix.first = ImageNcols;
    if (PL4_pix.second+1 > ImageNrows-0.001 && PL4_pix.second+1 <= ImageNrows+0.001)
        PL4_pix.second = ImageNrows;


    // Part 1, sub-part b - which of these points are "valid" ? those that == 1 or == ImageNcols, not those that are negative.
    std::vector<int> valid_planes_inters;
    valid_planes_inters.reserve(6);// for better performance
    if (( PL1_pix.first >= 1 && PL1_pix.first <= ImageNcols ) && ( PL1_pix.second >= 1 && PL1_pix.second <= ImageNrows ) )
        valid_planes_inters.push_back(1);
    if (( PL2_pix.first >= 1 && PL2_pix.first <= ImageNcols ) && ( PL2_pix.second >= 1 && PL2_pix.second <= ImageNrows ) )
        valid_planes_inters.push_back(2);
    if (( PL3_pix.first >= 1 && PL3_pix.first <= ImageNcols ) && ( PL3_pix.second >= 1 && PL3_pix.second <= ImageNrows ))
        valid_planes_inters.push_back(3);
    if (( PL4_pix.first >= 1 && PL4_pix.first <= ImageNcols ) && ( PL4_pix.second >= 1 && PL4_pix.second <= ImageNrows ))
        valid_planes_inters.push_back(4);


    if (valid_planes_inters.size()>2){ // there probably is a more elegant way to do this, but to explictly test each of the 4 corners :

        if (PL1_pix.first == PL2_pix.first && PL1_pix.second == PL2_pix.second){ // hits the Top Right Corner exactly
            std::vector<int>::iterator i = std::find(valid_planes_inters.begin(), valid_planes_inters.end(), 2 );
            if (i != valid_planes_inters.end())
                valid_planes_inters.erase(i); // toss Plane 2's match from the list
        }

        if (PL2_pix.first == PL3_pix.first && PL2_pix.second == PL3_pix.second){ // hits the Bottom Right Corner exactly
            std::vector<int>::iterator i = std::find(valid_planes_inters.begin(), valid_planes_inters.end(), 3 );
            if (i != valid_planes_inters.end())
                valid_planes_inters.erase(i); // toss Plane 3's match from the list
        }

        if (PL3_pix.first == PL4_pix.first && PL3_pix.second == PL4_pix.second){ // hits the Bottom Left Corner exactly
            std::vector<int>::iterator i = std::find(valid_planes_inters.begin(), valid_planes_inters.end(), 4 );
            if (i != valid_planes_inters.end())
                valid_planes_inters.erase(i); // toss Plane 4's match from the list
        }

        if (PL1_pix.first == PL4_pix.first && PL1_pix.second == PL4_pix.second){ // hits the Top Left Corner exactly
            std::vector<int>::iterator i = std::find(valid_planes_inters.begin(), valid_planes_inters.end(), 4 );
            if (i != valid_planes_inters.end())
                valid_planes_inters.erase(i); // toss Plane 4's match from the list
        }
    }

    if (valid_planes_inters.size() == 0 || valid_planes_inters.size()==2)
        ;// is OK, do nothing...
    else if (valid_planes_inters.size()== 1)
        std::cout << "WARNING - program says only 1 plane is being intercepted, assume a corner is hit/grazed exactly, not producing an NA right now, returning.\n";
    else
        std::cout << "WARNING - program says " << valid_planes_inters.size() << " plane(s) is being intercepted, assume a corner is hit/grazed exactly, not producing an NA right now, returning.\n";

    // Case where there is no Needle Annotation on the MR image:
    if (valid_planes_inters.size() != 2)
        return (std::make_tuple(physical_max_distance, phys_dist_begins, virtual_max_distance , virt_dist_begins));

    // Part 2 - make the dashes ;
    // Test 1 - distance from the needle tip to each entry/exit point. Obv., along the direction of the needle.
    double tip_edge_distances[2];
    // pt1xyz = locposPNT(1:3); % Needle Tip
    // eval(['pt2xyzA = PL'    num2str(valid_planes_inters(1))    '_XYZmm; ']);
    double pt2xyzA[3];
    if (valid_planes_inters[0] == 1)
        std::copy(PL1_XYZmm, PL1_XYZmm+3, pt2xyzA);
    else if(valid_planes_inters[0] == 2)
        std::copy(PL2_XYZmm, PL2_XYZmm+3, pt2xyzA);
    else if(valid_planes_inters[0] == 3)
        std::copy(PL3_XYZmm, PL3_XYZmm+3, pt2xyzA);
    else if(valid_planes_inters[0] == 4)
        std::copy(PL4_XYZmm, PL4_XYZmm+3, pt2xyzA);
    else
        std::cerr << "This should never happen, abort...\n";

    // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
    // http://mathworld.wolfram.com/Point-PlaneDistance.html
    double temp_3[] = {pt2xyzA[0]-one_XYZ_pLPH[0], pt2xyzA[1]-one_XYZ_pLPH[1], pt2xyzA[2]-one_XYZ_pLPH[2]};
    tip_edge_distances[0] = one_IJK_pLPH[0]*temp_3[0] + one_IJK_pLPH[1]*temp_3[1] +one_IJK_pLPH[2]*temp_3[2];


    // same : pt1xyz = locposPNT(1:3); % Needle Tip
    // eval(['pt2xyzB = PL'    num2str(valid_planes_inters(2))    '_XYZmm; ']);
    double pt2xyzB[3];
    if (valid_planes_inters[1] == 1)
        std::copy(PL1_XYZmm, PL1_XYZmm+3, pt2xyzB);
    else if(valid_planes_inters[1] == 2)
        std::copy(PL2_XYZmm, PL2_XYZmm+3, pt2xyzB);
    else if(valid_planes_inters[1] == 3)
        std::copy(PL3_XYZmm, PL3_XYZmm+3, pt2xyzB);
    else if(valid_planes_inters[1] == 4)
        std::copy(PL4_XYZmm, PL4_XYZmm+3, pt2xyzB);
    else
        std::cerr << "This should never happen, abort...\n";

    // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
    // http://mathworld.wolfram.com/Point-PlaneDistance.html
    double temp_4[] = {pt2xyzB[0]-one_XYZ_pLPH[0], pt2xyzB[1]-one_XYZ_pLPH[1], pt2xyzB[2]-one_XYZ_pLPH[2]};
    tip_edge_distances[1] = one_IJK_pLPH[0]*temp_4[0] + one_IJK_pLPH[1]*temp_4[1] + one_IJK_pLPH[2]*temp_4[2];


    // ABStip_edge_distances = abs(tip_edge_distances) ; % will be used a few times.

    if (tip_edge_distances[0] == std::abs(tip_edge_distances[0]) && tip_edge_distances[1] == std::abs(tip_edge_distances[1])){

        physical_max_distance = 0.0;
        phys_dist_begins = 0.0;

        size_t MAXind = std::abs(tip_edge_distances[0]) > std::abs(tip_edge_distances[1]) ? 0 : 1; // take the one with largest absolute value.
        size_t MINind;

        double temp_5[3];

        if (MAXind == 0){
            std::copy(pt2xyzA, pt2xyzA+3, temp_5);
            MINind = 1;
        }
        else{
            std::copy(pt2xyzB, pt2xyzB+3, temp_5);
            MINind = 0;
        }


        double dummy[2][3]; // = [ locposPNT(1:3)  ;  bothpointtwos(MAXind,:)  ] ;
        for (size_t ii = 0; ii < 2; ++ii)
            for (size_t jj = 0; jj < 3; ++jj)
                if(ii == 0)
                    dummy[ii][jj] = one_XYZ_pLPH[jj];
                else
                    dummy[ii][jj] = temp_5[jj];

        virtual_max_distance = std::sqrt( std::pow(( dummy[0][0] - dummy[1][0]), 2)    +   std::pow((dummy[0][1]  -  dummy[1][1]), 2)   +   std::pow((dummy[0][2] -  dummy[1][2]),2) );
        // ^ even though this may be a huge distance, not all of it in the MR image, I need a definite start point for the 20mm jumps (the needle tip), it cannot be any old 20mm jumps.
        // ^ also here, dummy = ACROSS is x,y,z ; DOWN is pt #1 then pt #2.
        //clear val MAXind ind dummy ;

        // Take the one with smallest absolute value.
        //dummy = [ locposPNT(1:3)  ;  bothpointtwos(MINind,:)  ] ;
        if (MINind == 0){
            std::copy(pt2xyzA, pt2xyzA+3, temp_5);
        }
        else{
            std::copy(pt2xyzB, pt2xyzB+3, temp_5);
        }
        // Put in the new ARRAY
        for (size_t jj = 0; jj < 3; ++jj)
            dummy[1][jj] = temp_5[jj];

        virt_dist_begins     = std::sqrt( std::pow((dummy[0][0] - dummy[1][0]), 2)    +   std::pow((dummy[0][1]  -  dummy[1][1]), 2)   +   std::pow((dummy[0][2] -  dummy[1][2]), 2));
    }

    else if (tip_edge_distances[0] == -1*std::abs(tip_edge_distances[0]) && tip_edge_distances[1] == -1*std::abs(tip_edge_distances[1])){

        size_t MAXind =std::abs(tip_edge_distances[0]) > std::abs(tip_edge_distances[1]) ? 0 : 1; // take the one with largest absolute value.
        //dummy = [ locposPNT(1:3)  ;  bothpointtwos(MAXind,:)  ] ;
        size_t MINind;

        double temp_5[3];

        if (MAXind == 0){
            std::copy(pt2xyzA, pt2xyzA+3, temp_5);
            MINind = 1;
        }
        else{
            std::copy(pt2xyzB, pt2xyzB+3, temp_5);
            MINind = 0;
        }


        double dummy[2][3]; // = [ locposPNT(1:3)  ;  bothpointtwos(MAXind,:)  ] ;
        for (size_t ii = 0; ii < 2; ++ii)
            for (size_t jj = 0; jj < 3; ++jj)
                if(ii == 0)
                    dummy[ii][jj] = one_XYZ_pLPH[jj];
                else
                    dummy[ii][jj] = temp_5[jj];

        physical_max_distance = std::sqrt(   std::pow((dummy[0][0] - dummy[1][0]), 2)    +   std::pow((dummy[0][1]  -  dummy[1][1]), 2)   +   std::pow((dummy[0][2] -  dummy[1][2]), 2));


        //[~,MINind]=min(ABStip_edge_distances); % take the one with smallest absolute value.
        //dummy = [ locposPNT(1:3)  ;  bothpointtwos(MINind,:)  ] ;
        if (MINind == 0){
            std::copy(pt2xyzA, pt2xyzA+3, temp_5);
        }
        else{
            std::copy(pt2xyzB, pt2xyzB+3, temp_5);
        }
        // Put in the new ARRAY
        for (size_t jj = 0; jj < 3; ++jj)
            dummy[1][jj] = temp_5[jj];

        phys_dist_begins = std::sqrt(  std::pow(( dummy[0][0] - dummy[1][0]), 2)    +   std::pow(( dummy[0][1]  -  dummy[1][1] ), 2)   +   std::pow(( dummy[0][2] -  dummy[1][2]), 2));

        virtual_max_distance = 0.0 ;
        virt_dist_begins = 0.0;

    }
    else { // make the appropriate part of the NA in the "virtual" pattern and the rest in the "physical" pattern, and vice-versa.

        //[~,NEGind]=min(tip_edge_distances); // NOT ABS, learn which one is NEGative
        size_t NEGind = tip_edge_distances[0] > tip_edge_distances[1] ? 1 : 0;
        size_t POSind;

        // dummy = [ locposPNT(1:3)  ;  bothpointtwos(NEGind,:)  ] ;
        double temp_6[3];
        if (NEGind == 0){
            std::copy(pt2xyzA, pt2xyzA+3, temp_6);
            POSind = 1;
        }
        else{
            std::copy(pt2xyzB, pt2xyzB+3, temp_6);
            POSind = 0;
        }

        double dummy[2][3]; // = [ locposPNT(1:3)  ;  bothpointtwos(NEind,:)  ] ;
        for (size_t ii = 0; ii < 2; ++ii)
            for (size_t jj = 0; jj < 3; ++jj)
                if(ii == 0)
                    dummy[ii][jj] = one_XYZ_pLPH[jj];
                else
                    dummy[ii][jj] = temp_6[jj];


        physical_max_distance = std::sqrt(   std::pow((dummy[0][0] - dummy[1][0]), 2)    +   std::pow((dummy[0][1]  -  dummy[1][1]), 2)   +   std::pow((dummy[0][2] -  dummy[1][2]), 2)    );

        //[~,POSind]=max(tip_edge_distances); // NOT ABS, learn which one is POSitive
        if (POSind == 0){
            std::copy(pt2xyzA, pt2xyzA+3, temp_6);
        }
        else{
            std::copy(pt2xyzB, pt2xyzB+3, temp_6);
        }
        // dummy = [ locposPNT(1:3)  ;  bothpointtwos(POSind,:)  ] ; // not needed : virtual_pt1pt2 = [ locposPNT(1:3)  ;  bothpointtwos(POSind,:)  ] ; dummy = virtual_pt1pt2;
        // Put in the new ARRAY
        for (size_t jj = 0; jj < 3; ++jj)
            dummy[1][jj] = temp_6[jj];

        virtual_max_distance = std::sqrt(   std::pow(( dummy[0][0] - dummy[1][0]), 2)    +   std::pow(( dummy[0][1]  -  dummy[1][1] ), 2)   +   std::pow(( dummy[0][2] -  dummy[1][2]), 2)    );

        phys_dist_begins = 0;
        virt_dist_begins = 0;
    }

    return (std::make_tuple(physical_max_distance, phys_dist_begins, virtual_max_distance , virt_dist_begins));
}

void MainWindow::dicom_Annotation_RT(QString &filename)
{
    if (!filename.isNull())
    {
        // Start logging
        QStringList parts = filename.split("/");
        QString lastBit = parts.at(parts.size()-1);
        QTextStream(myFile) << QDate::currentDate().toString("ddd MMMM d yyyy") +
                               QString(" ") +  QTime::currentTime().toString() + QString("  INFO - ") + QString("Dicom loaded: ") + lastBit << endl;

        // Convert Qstring to string literal
        QByteArray byteArray = filename.toUtf8();
        const char *cString = byteArray.constData();

        // Read input dicom file
        gdcm::ImageReader ir;
        ir.SetFileName( cString );
        if(!ir.Read())
        {
            std::cerr << "One or more file(s) are not supported, abort...\n";
            return;
        }

        // Let's get some info from the header file
        gdcm::File &file = ir.GetFile();
        gdcm::StringFilter  sf = gdcm::StringFilter();
        sf.SetFile(file);

        /*
         *  The names of the variables are based on Barry matlab script for simplicity
         *  Gathering the input data
         * */
        std::string PatOrient(sf.ToStringPair(gdcm::Tag(0x0018,0x5100)).second);

        // Dicom location
        double TLC_XYZmm[3];
        stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0032)).second, TLC_XYZmm, 3);

        double Im_rowdircoldir[6];
        stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, Im_rowdircoldir, 6);

        double Slice_Thick = std::stod(sf.ToStringPair(gdcm::Tag(0x0018,0x0050)).second);

        double PixSizeLocalYX[2]; // Pixel spacing
        stringParser(sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second, PixSizeLocalYX, 2);

        // This tag (0051,1012) does not always exit, therefore I am going to assume:
        //  - TablePositionString = "TP 0" ;
        // Hence:
        //  - const char TablePosHorF = 'H';
        //  - const size_t TablePosVal  = 0;
        const char TablePosHorF  = 'H';
        const size_t TablePosVal = 0;


        double ImageNrows = std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second ) ;
        double ImageNcols = std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second ) ;

        // Get the tracking sensor coordinates
        // These coord. need to be queried in real-time from the endoscout machine, but for now this will
        // do it.

        double orig_one_RISC_XYZ []     = { XYZ[0], XYZ[1], XYZ[2] };

        // For the monopole sensor =Mahamadou=
        //double orig_one_RISC_IJK[]      = { tvt[0], tvt[1], tvt[2] };
        //double orig_one_RISC_TnormIJK[] = { nvt[0], nvt[1], nvt[2] };

        // For the catheder 036 sensor
        double orig_one_RISC_IJK[]      = { nvt[0], nvt[1], nvt[2] };
        double orig_one_RISC_TnormIJK[] = { tvt[0], tvt[1], tvt[2] };

        // Start logging Sensor data
        QTextStream(myFile) << QDate::currentDate().toString("ddd MMMM d yyyy") +
                               QString(" ") +  QTime::currentTime().toString() + QString("  INFO - ") + QString("Sensor data: ") <<
                               QString::number(XYZ[0]) + QString(" ") + QString::number(XYZ[1]) + QString(" ") +QString::number(XYZ[2])<<
                                                                                                                                          QString("  ") + QString::number(nvt[0]) + QString(" ") + QString::number(nvt[1]) + QString(" ") +QString::number(nvt[2]) <<
                                                                                                                                                                                                                                                                      QString("  ") + QString::number(tvt[0]) + QString(" ") + QString::number(tvt[1]) + QString(" ") +QString::number(tvt[2])<< endl;

        /*
         * Start computing some relevents matrices
         */

        // Compute the Field of View
        double FOVmm_LocalYX[] = { PixSizeLocalYX[0]*ImageNrows, PixSizeLocalYX[1]*ImageNcols}  ; // no need to :  round()

        // Determine the normal to the dicom image
        // Using the cross product of Im_rowdirColdir
        double ImageNormal[] = { Im_rowdircoldir[1]*Im_rowdircoldir[5] - Im_rowdircoldir[2]*Im_rowdircoldir[4],
                                 Im_rowdircoldir[2]*Im_rowdircoldir[3] - Im_rowdircoldir[0]*Im_rowdircoldir[5],
                                 Im_rowdircoldir[0]*Im_rowdircoldir[4] - Im_rowdircoldir[3]*Im_rowdircoldir[1] };

        // Bottom Right Corner, the center of that pixel :
        double BRC_XYZmm[3];         //  Move to Top Right (row movement). Move to Bottom Right (col movement)
        BRC_XYZmm[0] = TLC_XYZmm[0]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[0] +  (FOVmm_LocalYX[0]-1*PixSizeLocalYX[0])*Im_rowdircoldir[3];
        BRC_XYZmm[1] = TLC_XYZmm[1]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[1] +  (FOVmm_LocalYX[0]-1*PixSizeLocalYX[0])*Im_rowdircoldir[4];
        BRC_XYZmm[2] = TLC_XYZmm[2]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[2] +  (FOVmm_LocalYX[0]-1*PixSizeLocalYX[0])*Im_rowdircoldir[5];

        // Top Right Corner, the center of that pixel :
        double TRC_XYZmm[3];               // Move to Top Right (row movement)
        TRC_XYZmm[0] = TLC_XYZmm[0]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[0];
        TRC_XYZmm[1] = TLC_XYZmm[1]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[1];
        TRC_XYZmm[2] = TLC_XYZmm[2]  +  (FOVmm_LocalYX[1]-1*PixSizeLocalYX[1])*Im_rowdircoldir[2];

        // Set 2 = optional, _if_ need to work with the surfaces of the MRI image as a volume/slice/slab :
        double PS_TLC_XYZmm[3]; // "PLUS" Surface, point 1 (Top Left).
        PS_TLC_XYZmm[0] = TLC_XYZmm[0] + 0.5*Slice_Thick*ImageNormal[0] ; PS_TLC_XYZmm[1] = TLC_XYZmm[1] + 0.5*Slice_Thick*ImageNormal[1] ;  PS_TLC_XYZmm[2] = TLC_XYZmm[2] + 0.5*Slice_Thick*ImageNormal[2] ;
        double  MS_TLC_XYZmm[3]; // "MINUS" Surface, point 1 (Top Left).
        MS_TLC_XYZmm[0] = TLC_XYZmm[0] - 0.5*Slice_Thick*ImageNormal[0] ; MS_TLC_XYZmm[1] = TLC_XYZmm[1] - 0.5*Slice_Thick*ImageNormal[1] ;  MS_TLC_XYZmm[2] = TLC_XYZmm[2] - 0.5*Slice_Thick*ImageNormal[2] ;
        double PS_BRC_XYZmm[3]; // "PLUS" Surface, point 2 (Bottom Right).
        PS_BRC_XYZmm[0] = BRC_XYZmm[0] + 0.5*Slice_Thick*ImageNormal[0] ; PS_BRC_XYZmm[1] = BRC_XYZmm[1] + 0.5*Slice_Thick*ImageNormal[1] ;  PS_BRC_XYZmm[2] = BRC_XYZmm[2] + 0.5*Slice_Thick*ImageNormal[2] ;
        double MS_BRC_XYZmm[3]; // "MINUS" Surface, point 2 (Bottom Right).
        MS_BRC_XYZmm[0] = BRC_XYZmm[0] - 0.5*Slice_Thick*ImageNormal[0] ; MS_BRC_XYZmm[1] = BRC_XYZmm[1] - 0.5*Slice_Thick*ImageNormal[1] ;  MS_BRC_XYZmm[2] = BRC_XYZmm[2] - 0.5*Slice_Thick*ImageNormal[2] ;
        double PS_TRC_XYZmm[3]; // "PLUS" Surface, point 3 (Top Right).
        PS_TRC_XYZmm[0] = TRC_XYZmm[0] + 0.5*Slice_Thick*ImageNormal[0] ; PS_TRC_XYZmm[1] = TRC_XYZmm[1] + 0.5*Slice_Thick*ImageNormal[1] ;  PS_TRC_XYZmm[2] = TRC_XYZmm[2] + 0.5*Slice_Thick*ImageNormal[2] ;
        double MS_TRC_XYZmm[3]; // "MINUS" Surface, point 3 (Top Right).
        MS_TRC_XYZmm[0] = TRC_XYZmm[0] - 0.5*Slice_Thick*ImageNormal[0] ; MS_TRC_XYZmm[1] = TRC_XYZmm[1] - 0.5*Slice_Thick*ImageNormal[1] ;  MS_TRC_XYZmm[2] = TRC_XYZmm[2] - 0.5*Slice_Thick*ImageNormal[2] ;

        /*
         * Step 2 - perform RISC (Robin Invariant Scanner Coordinate) to +LPH Coordinate System Conversion, on the tracking data
        */
        // Declare some variables
        int  InnerFactor = 0;
        int  OuterFactor = 0;

        if (TablePosHorF == 'H')
            InnerFactor = -1;
        else if (TablePosHorF == 'F')
            InnerFactor = +1;
        else
            std::cerr << "Invalid value for TablePosHorF, must quit program";

        // Make a temporary copy of orig_one_RISC_XYZ
        double Temp[3], Temp1[3], Temp2[3];
        std::copy(orig_one_RISC_XYZ, orig_one_RISC_XYZ+3, Temp);
        std::copy(orig_one_RISC_IJK, orig_one_RISC_IJK+3, Temp1);
        std::copy(orig_one_RISC_TnormIJK, orig_one_RISC_TnormIJK+3, Temp2);


        if(PatOrient.compare(std::string("HFS ")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME= as before. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }
        else if(PatOrient.compare(std::string("HFP ")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME= as before. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }
        else if(PatOrient.compare(std::string("FFS ")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME= as before. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];


        }

        else if(PatOrient.compare(std::string("FFP ")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ is the =SAME=. Nothing to do.

            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("HFDR")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes to [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];


            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("HFDL")) == 0){

            OuterFactor = -1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes to [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];


            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = +1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = +1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = +1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("FFDR")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes to [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];


            // Alter the polarities
            orig_one_RISC_XYZ[0] = +1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = +1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = +1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = +1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = +1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = +1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];

        }

        else if(PatOrient.compare(std::string("FFDL")) == 0){

            OuterFactor = +1 ;

            // A separate calculation for the table Position, which only affects Z location.
            // Perform it while the tracking data is still in RISC
            orig_one_RISC_XYZ[2] = orig_one_RISC_XYZ[2] + OuterFactor * InnerFactor * TablePosVal ;

            // Do the actual conversion, of 1 location and 2 directions
            // In this case the indexing of orig_one_RISC_XYZ changes [2 1 3]
            orig_one_RISC_XYZ[0] = Temp[1];
            orig_one_RISC_XYZ[1] = Temp[0];
            orig_one_RISC_XYZ[2] = Temp[2];

            orig_one_RISC_IJK[0] = Temp1[1];
            orig_one_RISC_IJK[1] = Temp1[0];
            orig_one_RISC_IJK[2] = Temp1[2];

            orig_one_RISC_TnormIJK[0] = Temp2[1];
            orig_one_RISC_TnormIJK[1] = Temp2[0];
            orig_one_RISC_TnormIJK[2] = Temp2[2];

            // Alter the polarities
            orig_one_RISC_XYZ[0] = -1 * orig_one_RISC_XYZ[0];
            orig_one_RISC_XYZ[1] = -1 * orig_one_RISC_XYZ[1];
            orig_one_RISC_XYZ[2] = -1 * orig_one_RISC_XYZ[2];

            orig_one_RISC_IJK[0] = -1 * orig_one_RISC_IJK[0];
            orig_one_RISC_IJK[1] = -1 * orig_one_RISC_IJK[1];
            orig_one_RISC_IJK[2] = -1 * orig_one_RISC_IJK[2];

            orig_one_RISC_TnormIJK[0] = -1 * orig_one_RISC_TnormIJK[0];
            orig_one_RISC_TnormIJK[1] = -1 * orig_one_RISC_TnormIJK[1];
            orig_one_RISC_TnormIJK[2] = -1 * orig_one_RISC_TnormIJK[2];

        }
        else{
            std::cerr << "Invalid PatOrient value, abort...";
            exit(-1);
        }

        /*
        * Draw the annotation :
        * Please note : this is only one method of performing the annotation.
        * this annotation mimics the method of GE Signa SP / iDrive, circa 1995.
        * this method can be changed and still be valid.
        * some choices that are made, that can be changed/simplified in another annotation method, are :
        *           - drawing the annotation line in 20mm chunks
        *           - in different colors
        *           - dealing with : within plane, proximal to plane, distal to plane
        *                   - the need to calc the "PLUS" Surface and the "MINUS" Surface
        *           - 60 degrees value
        *           - the bifurcated method of line or dots
        *           - adding text of distance, when method is dots
        */

        // Compute the angle of the needle
        long double angle_needle_off_slice = abs( 90 - acos( ImageNormal[0]*orig_one_RISC_IJK[0] + ImageNormal[1]*orig_one_RISC_IJK[1] + ImageNormal[2]*orig_one_RISC_IJK[2]) * 180/M_PI  ) ;

        // Start drawing
        if (angle_needle_off_slice <= 60)   // draw a Needle Annotation as Linear Line
        {

            // Parse the image orientation string
            double orientation_matrix[6];
            stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix, 6);

            // Get image orientation
            char *Letter_H = getOrientation_H(orientation_matrix);
            char *Letter_V = getOrientation_V(orientation_matrix);


            // Get the largest image pixel value
            //uint LargestImagePixelValue = /*std::stoi(sf.ToStringPair(gdcm::Tag(0x0028,0x0107)).second)*/ 1482;

            const gdcm::Image &gimage = ir.GetImage();
            std::vector<char> vbuffer;
            vbuffer.resize( gimage.GetBufferLength() );
            char *buffer = &vbuffer[0];

            // Convert dicom to RGB format
            QImage *imageQt = NULL;
            if( !ConvertToFormat_RGB888( gimage, buffer, imageQt ) )
            {
                std::cerr << "Error during file conversion: ConvertToFormat_RGB888\n";
            }


            // FOV calculation
            // FOVx = Raw * Pixel_spacing[0];
            // FOVy = Col * Pixel_spacing[1];
            std::string token = "\\";
            std::string s = sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second;

            long double FOVx = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second) *
                                          std::stod(s.substr(0, s.find(token))) + 0.5);

            s.erase(0, s.find(token) + token.length());

            long double FOVy = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second) *
                                          std::stod(s) + 0.5);


            // Keep aspect ratio
            QImage imageQt_Scaled = imageQt->scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

            // Write some Dicom Info on the display
            std::unique_ptr<QPainter> painter(new QPainter(&imageQt_Scaled));
            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));

            //Draw the text on the image
            painter->drawText( QPoint(2,14),    QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0010,0x0010)).second));
            painter->drawText( QPoint(2,28),    QString::fromStdString("FOV: " + std::to_string(FOVx) + "*" + std::to_string(FOVy)));

            painter->setPen(Qt::white);
            painter->setFont(QFont("Segoe UI",16));
            painter->drawText( QPoint(4,230),    QString::fromStdString(Letter_H));
            painter->drawText( QPoint(218,20),   QString::fromStdString(Letter_V));

            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));
            painter->drawText( QPoint(267,14),  QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).second));
            painter->drawText( QPoint(267,28),  QString::fromStdString("Date: " + sf.ToStringPair(gdcm::Tag(0x0008,0x0023)).second));
            painter->drawText( QPoint(2,390),   QString::fromStdString("TR: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0080)).second + " ms"));
            painter->drawText( QPoint(2,404),   QString::fromStdString("TE: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0081)).second + " ms"));
            painter->drawText( QPoint(2,418),   QString::fromStdString("FA: " + sf.ToStringPair(gdcm::Tag(0x0018,0x1314)).second + " deg"));


            /*
             *  See "GE_FPmanual_NeedleTracking19-11-02.pdf", pages 1-33 to 1-35, for a discussiom on Needle Annotation.
             *
             * 3 - learn where the Plus Surface and the Minus Surface are intersected by the 3-D needle annotation.
             * The result of this function is the plot locations on the MR image - "local-x" and "local-y", in whole/integer pixel units.
             * Result will = -999 if image plane and needle are parallel.
             * Also, this is the "general" solution, result may be <1, >ImageNrows, read "pixptsfrom3d" for more information.
             */
            std::pair<double, double> psi_centralpix, msi_centralpix;
            double newlineintersect_XYZmm[3];
            double newlineintersect_XYZmm_2[3];

            psi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, PS_TLC_XYZmm, PS_BRC_XYZmm, PS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm ) ;
            msi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, MS_TLC_XYZmm, MS_BRC_XYZmm, MS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm_2 ) ;

            // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
            // http://mathworld.wolfram.com/Point-PlaneDistance.html

            double dist_tip_PSinter = 0.0;
            double dist_tip_MSinter = 0.0;
            double temp[] = {newlineintersect_XYZmm[0] - orig_one_RISC_XYZ[0], newlineintersect_XYZmm[1] - orig_one_RISC_XYZ[1], newlineintersect_XYZmm[2] - orig_one_RISC_XYZ[2]};
            if (std::abs(newlineintersect_XYZmm[0]) != +999)
                dist_tip_PSinter = orig_one_RISC_IJK[0]*temp[0] + orig_one_RISC_IJK[1]*temp[1] + orig_one_RISC_IJK[2]*temp[2] ;
            else
                dist_tip_PSinter = -999 ;

            double temp_1[] = {newlineintersect_XYZmm_2[0]-orig_one_RISC_XYZ[0], newlineintersect_XYZmm_2[1]-orig_one_RISC_XYZ[1], newlineintersect_XYZmm_2[2]-orig_one_RISC_XYZ[2]};
            if (std::abs(newlineintersect_XYZmm_2[0]) != +999)
                dist_tip_MSinter = orig_one_RISC_IJK[0]*temp_1[0] + orig_one_RISC_IJK[1]*temp_1[1] + orig_one_RISC_IJK[2]*temp_1[2] ;
            else
                dist_tip_MSinter = -999;

            /*
            * 4 - location of the needle tip on the (local) MR image.
            * See pencil notes page 23 for explaination why "ImageNormal" substituted for "locposPNT(4:6)".
            */
            double unused_var[3];
            std::pair<double, double> tip_plot_pix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, unused_var);

            /*
            * 5 - learn exactly what pixels need to be created for the NA (dashes = solid and blanks)
            * and what color each should be (prox, distal, or in plane)

            * and 6 - PLOT
            * (Original plan was to separate out calculations and plotting, but both are done in the sub-functions below ("plot_NApixels, etc) )


            * PROGRAMMING NOTE : The problem has been structured to determine
            * how much of the "physcial needle" must be plotted (16mm solid, 4mm blank)
            * and how much of the "virtual needle" must be plotted (6mm solid, 14mm blank).

            * My initial solution was to determine precisely the exact (plus some overshoot) distance of each.
            * This requires some detailed calculations to determine where the NA "enters" the MR image and where it "exists" - see "precise_phys_virt_na_dist".
            * Another solution is to pick 2 very large distances, the max possible that can occur,
            * and plot them blindly onto the MR image.

            * on 4/23/04, a speed test was conducted in Matlab. (See commented code for "tic", "toc", "temp_del_jc","accum_tictocs", etc.)
            * the "precise" method took M+-SD 0.06 +- 0.02 sec to calc and plot each case
            * distances=2000 (the diameter of the OpenSpeed magnets) took M+-SD 0.26 +- 0.02 sec to plot each case, 4 times longer.

            * You may want to perform the same type of speed test in the real time C program, to help decide which method is better.
            */
            double physical_max_distance, phys_dist_begins, virtual_max_distance , virt_dist_begins;
            std::tie(physical_max_distance, phys_dist_begins, virtual_max_distance , virt_dist_begins) =
                    precise_phys_virt_na_dist( FOVmm_LocalYX , PixSizeLocalYX , ImageNcols , ImageNrows , TLC_XYZmm , TRC_XYZmm , BRC_XYZmm , ImageNormal ,
                                               Im_rowdircoldir , orig_one_RISC_XYZ, orig_one_RISC_IJK);

            // Assign Constants :
            const int DBDB_mm = 20; // = "Dist_Bet_Dash_Begins" , set by GE, read "GE_FPmanual_NeedleTracking19-11-02.pdf" for more info

            if (physical_max_distance > 0)
            {// create physical needle pattern
                const int len_solid_part_mm = 16; // in physical pattern, 16 out of 20 mm are solid, determined by BF by counting on IP images.

                //target_distances_beg_end =   0:DBDB_mm:physical_max_distance+DBDB_mm ;  % overshoot how long you must go, allow auto-clipping
                std::vector<double> target_distances_beg_end, target_distances_beg_end_temp;
                int ii = 0;
                while (ii <= physical_max_distance+DBDB_mm){
                    // Keep only values less or equal phys_dist_begins
                    if (ii >= phys_dist_begins)
                        target_distances_beg_end.push_back(ii);
                    ii += DBDB_mm;
                }

                // Target_distances_beg_end = [  target_distances_beg_end    target_distances_beg_end+len_solid_part_mm ] ;
                target_distances_beg_end_temp.resize(target_distances_beg_end.size());
                for (int ii = 0; ii < target_distances_beg_end_temp.size(); ++ii)
                    target_distances_beg_end_temp[ii] = target_distances_beg_end[ii] + len_solid_part_mm;

                // Append the two vec.
                target_distances_beg_end.insert(target_distances_beg_end.end(), target_distances_beg_end_temp.begin(),target_distances_beg_end_temp.end());

                // Will travel retrograde, from the needle tip to off the MR image
                double dir2travel[3];
                dir2travel[0]= -1 * orig_one_RISC_IJK[0];
                dir2travel[1]= -1 * orig_one_RISC_IJK[1];
                dir2travel[2]= -1 * orig_one_RISC_IJK[2];

                // I can introduce the analysis/comparison to dist_tip_PSinter, dist_tip_MSinter
                // Here or inside plot_NA_dashes - do it inside.
                std::vector<double> additionalbreak;
                additionalbreak.reserve(2);

                int sign_of_dist_tip_PSinter = (dist_tip_PSinter > 0) ? 1 :((dist_tip_PSinter < 0) ? -1 : 0);

                if (sign_of_dist_tip_PSinter == -1 && std::abs(dist_tip_PSinter)!= 999 && std::abs(dist_tip_PSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_PSinter));
                // Do abs() - I checked that direction is correct already, so submit it as a positive distance (in dir2travel direction)

                int sign_of_dist_tip_MSinter = (dist_tip_MSinter > 0) ? 1 :((dist_tip_MSinter < 0) ? -1 : 0);
                if (sign_of_dist_tip_MSinter == -1 && std::abs(dist_tip_MSinter)!=999 && std::abs(dist_tip_MSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_MSinter));
                // Sort the elements of target_distances_beg_end
                std::sort(target_distances_beg_end.begin(), target_distances_beg_end.end());
                // Remove all the duplicated elements
                auto last = std::unique(target_distances_beg_end.begin(), target_distances_beg_end.end());
                target_distances_beg_end.erase(last,target_distances_beg_end.end());

                /* Start plotting
                 * Part 0 - add dist_tip_PSinter, dist_tip_MSinter to the mix of begin - end pairs  if necc.
                 * If I need to break a dash into 2 pieces
                */
                if (additionalbreak.size() != 0){
                    double newbreak_val1val2[2];
                    for (size_t ii = 0; ii < target_distances_beg_end.size()/2 ; ++ii){
                        for (size_t jj = 0; jj < additionalbreak.size(); ++jj){
                            //double thisAB = additionalbreak[perAB] ;
                            if (additionalbreak[jj] > target_distances_beg_end[2*ii] && additionalbreak[jj] < target_distances_beg_end[2*ii +1]){
                                newbreak_val1val2[0] = additionalbreak[jj] - 0.05;
                                newbreak_val1val2[1] = additionalbreak[jj] + 0.05;
                            }
                        }
                    }
                    if (sizeof(newbreak_val1val2)/sizeof(newbreak_val1val2[0]) != 0)
                        target_distances_beg_end.insert(target_distances_beg_end.end(), newbreak_val1val2, newbreak_val1val2+2);
                }

                // Part 1 - determine the true 3-D locations of 3-D jumps, up to plotreal3Ddistance, and then the pixel locations to plot

                size_t Npts = target_distances_beg_end.size();
                // Pixel_locs_X_Y = zeros(Npts,2);
                std::vector< std::pair<double, double>> pixel_locs_X_Y(Npts);
                double thisloc_XYZmm[3];
                double unused_var2[3];

                for(size_t ii = 0; ii < Npts; ++ii){
                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + target_distances_beg_end[ii]*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + target_distances_beg_end[ii]*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + target_distances_beg_end[ii]*dir2travel[2];

                    // get the pixel location of this 3-D point
                    std::pair<double, double> pix = pix_3d_ptsfrom3d( thisloc_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, unused_var2 );
                    pixel_locs_X_Y[ii].first = pix.first;
                    pixel_locs_X_Y[ii].second = pix.second;
                }

                // part 2 -
                // learn if these 3-D points, "thisloc_XYZmm", are distal, proximal, or in-slice
                // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
                // http://mathworld.wolfram.com/Point-PlaneDistance.html
                // analyze the central point of each dash. The edges may be coincident with the PS or MS,...

                size_t Npairs = Npts/2;
                std::vector<double> dist4color_perpair(Npairs);

                for (size_t ii = 0;  ii < Npairs; ++ii){

                    double thisdist = (target_distances_beg_end[2*ii] + target_distances_beg_end[2*ii +1])/2;
                    double thisloc_XYZmm[3];

                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + thisdist*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + thisdist*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + thisdist*dir2travel[2];

                    // from MR image to needle line. distance = final - initial. From Mr image to 3-D needle line.
                    double temp[] = {thisloc_XYZmm[0] - TLC_XYZmm[0], thisloc_XYZmm[1] - TLC_XYZmm[1], thisloc_XYZmm[2] - TLC_XYZmm[2]};
                    double D = ImageNormal[0]*temp[0] + ImageNormal[1]*temp[1]+ ImageNormal[2]*temp[2];

                    dist4color_perpair[ii] = D;
                }

                // part 3 - fill in the solid pattern part, from beginning to end
                // pre-make with all blue. Find the places where the color is different than this.
                //colorperpair = abs(inplanecolor) * ones(1,Npairs);
                std::vector<int> colorperpair(Npairs, 98); // abs('b')=98 in Matlab

                // Change color to yellow
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] < -1*Slice_Thick/2)
                        // Change to yellow
                        colorperpair[ii] = 121;                    // abs('y') = 121 in Matlab
                }

                // Change to red
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] > +1*Slice_Thick/2)
                        // Change to red
                        colorperpair[ii] = 114;                    // abs('r') = 114 in Matlab
                }

                // Start plotting
                QVector<QPoint> pointPairs;
                for(size_t ii = 0; ii < Npairs; ++ii){
                    if (2*ii+1 < Npts)
                        pointPairs << QPoint(std::ceil(pixel_locs_X_Y[2*ii].first * window_2D_width/ImageNrows), std::ceil(pixel_locs_X_Y[2*ii+1].first * window_2D_width/ImageNrows)) // scaling factor
                                << QPoint(std::ceil(pixel_locs_X_Y[2*ii].second * window_2D_width/ImageNcols), std::ceil(pixel_locs_X_Y[2*ii+1].second * window_2D_width/ImageNcols));
                }

                // PROGRAMMING NOTE : Decide, if it is better in C, to first test if the points are in the image, or to always plot and take advantage of auto-clip.
                // plot( [ pt1(1)  pt2(1) ] , [ pt1(2)  pt2(2) ] , '-' , 'color', colorperpair(perpair), 'LineWidth' , NALineWidth );

                for (int ii = 0; ii < Npairs; ++ii){
                    if (colorperpair[ii] == 121){
                        QPen pen1(Qt::yellow, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                    else if (colorperpair[ii] == 114){
                        QPen pen1(Qt::red, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                    else if (colorperpair[ii] == 98){
                        QPen pen1(Qt::blue, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                }

                // Load the dicom file
                ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                //update();

                // Save image
                auto myString =  QTime::currentTime().toString();
                QString hours   = myString.mid(0, 2);
                QString minutes = myString.mid(3, 2);
                QString seconds = myString.mid(6, 2);
                Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                              QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
            }


            if (virtual_max_distance > 0){//% create virtual needle pattern

                const int len_solid_part_mm = 6; // in virtual pattern, 6 out of 20 mm are solid, determined by BF by counting on IP images.

                // next 1, 4th lines  are different than for physical :
                // target_distances_beg_end =   DBDB_mm:DBDB_mm:virtual_max_distance+DBDB_mm;
                // i = find(target_distances_beg_end<virt_dist_begins);
                std::vector<double> target_distances_beg_end, target_distances_beg_end_temp;
                int ii = DBDB_mm;
                while (ii <= virtual_max_distance+DBDB_mm){
                    // Keep only values less or equal phys_dist_begins
                    //if (ii <= virt_dist_begins)
                    target_distances_beg_end.push_back(ii);
                    ii += DBDB_mm;
                }

                // Target_distances_beg_end = [  target_distances_beg_end    target_distances_beg_end-len_solid_part_mm ];
                target_distances_beg_end_temp.resize(target_distances_beg_end.size());
                for (int ii = 0; ii < target_distances_beg_end_temp.size(); ++ii)
                    target_distances_beg_end_temp[ii] = target_distances_beg_end[ii] - len_solid_part_mm;
                // Append the two vec.
                target_distances_beg_end.insert(target_distances_beg_end.end(), target_distances_beg_end_temp.begin(),target_distances_beg_end_temp.end());

                // Will travel travel the same direction the needle is pointing
                double dir2travel[3];
                dir2travel[0]= +1 * orig_one_RISC_IJK[0];
                dir2travel[1]= +1 * orig_one_RISC_IJK[1];
                dir2travel[2]= +1 * orig_one_RISC_IJK[2];

                // I can introduce the analysis/comparison to dist_tip_PSinter, dist_tip_MSinter
                // Here or inside plot_NA_dashes - do it inside.
                std::vector<double> additionalbreak;
                additionalbreak.reserve(2);

                // Next 2 lines are different than for physical :
                int sign_of_dist_tip_PSinter = (dist_tip_PSinter > 0) ? 1 :((dist_tip_PSinter < 0) ? -1 : 0);

                if (sign_of_dist_tip_PSinter == +1 && std::abs(dist_tip_PSinter)!= 999 && std::abs(dist_tip_PSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_PSinter));
                // Do abs() - I checked that direction is correct already, so submit it as a positive distance (in dir2travel direction)

                int sign_of_dist_tip_MSinter = (dist_tip_MSinter > 0) ? 1 :((dist_tip_MSinter < 0) ? -1 : 0);
                if (sign_of_dist_tip_MSinter == +1 && std::abs(dist_tip_MSinter)!=999 && std::abs(dist_tip_MSinter) < target_distances_beg_end[target_distances_beg_end.size()-1])
                    additionalbreak.push_back(std::abs(dist_tip_MSinter));
                // Sort the elements of target_distances_beg_end
                std::sort(target_distances_beg_end.begin(), target_distances_beg_end.end());
                // Remove all the duplicated elements
                auto last = std::unique(target_distances_beg_end.begin(), target_distances_beg_end.end());
                target_distances_beg_end.erase(last, target_distances_beg_end.end());


                /* Start plotting
                 * Part 0 - add dist_tip_PSinter, dist_tip_MSinter to the mix of begin - end pairs  if necc.
                 * If I need to break a dash into 2 pieces
                */
                if (additionalbreak.size() != 0){
                    double newbreak_val1val2[2];
                    for (size_t ii = 0; ii < target_distances_beg_end.size()/2 ; ++ii){
                        for (size_t jj = 0; jj < additionalbreak.size(); ++jj){
                            //double thisAB = additionalbreak[perAB] ;
                            if (additionalbreak[jj] > target_distances_beg_end[2*ii] && additionalbreak[jj] < target_distances_beg_end[2*ii +1]){
                                newbreak_val1val2[0] = additionalbreak[jj] - 0.05;
                                newbreak_val1val2[1] = additionalbreak[jj] + 0.05;
                            }
                        }
                    }
                    if (sizeof(newbreak_val1val2)/sizeof(newbreak_val1val2[0]) != 0)
                        target_distances_beg_end.insert(target_distances_beg_end.end(), newbreak_val1val2, newbreak_val1val2+2);
                }

                // Part 1 - determine the true 3-D locations of 3-D jumps, up to plotreal3Ddistance, and then the pixel locations to plot

                size_t Npts = target_distances_beg_end.size();
                // Pixel_locs_X_Y = zeros(Npts,2);
                std::vector< std::pair<double, double>> pixel_locs_X_Y(Npts);
                double thisloc_XYZmm[3];
                double unused_var2[3];

                for(size_t ii = 0; ii < Npts; ++ii){
                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + target_distances_beg_end[ii]*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + target_distances_beg_end[ii]*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + target_distances_beg_end[ii]*dir2travel[2];

                    // get the pixel location of this 3-D point
                    std::pair<double, double> pix = pix_3d_ptsfrom3d( thisloc_XYZmm , ImageNormal , TLC_XYZmm, BRC_XYZmm, TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, unused_var2 );
                    pixel_locs_X_Y[ii].first = pix.first;
                    pixel_locs_X_Y[ii].second = pix.second;
                }

                // part 2 -
                // learn if these 3-D points, "thisloc_XYZmm", are distal, proximal, or in-slice
                // Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
                // http://mathworld.wolfram.com/Point-PlaneDistance.html
                // analyze the central point of each dash. The edges may be coincident with the PS or MS,...

                size_t Npairs = Npts/2;
                std::vector<double> dist4color_perpair(Npairs);

                for (size_t ii = 0;  ii < Npairs; ++ii){

                    double thisdist = (target_distances_beg_end[2*ii] + target_distances_beg_end[2*ii +1])/2;
                    double thisloc_XYZmm[3];

                    thisloc_XYZmm[0] = orig_one_RISC_XYZ[0] + thisdist*dir2travel[0];
                    thisloc_XYZmm[1] = orig_one_RISC_XYZ[1] + thisdist*dir2travel[1];
                    thisloc_XYZmm[2] = orig_one_RISC_XYZ[2] + thisdist*dir2travel[2];

                    // from MR image to needle line. distance = final - initial. From Mr image to 3-D needle line.
                    double temp[] = {thisloc_XYZmm[0] - TLC_XYZmm[0], thisloc_XYZmm[1] - TLC_XYZmm[1], thisloc_XYZmm[2] - TLC_XYZmm[2]};
                    double D = ImageNormal[0]*temp[0] + ImageNormal[1]*temp[1]+ ImageNormal[2]*temp[2];

                    dist4color_perpair[ii] = D;
                }

                // part 3 - fill in the solid pattern part, from beginning to end
                // pre-make with all blue. Find the places where the color is different than this.
                //colorperpair = abs(inplanecolor) * ones(1,Npairs);
                std::vector<int> colorperpair(Npairs, 98); // abs('b')=98 in Matlab

                // Change color to yellow
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] < -1*Slice_Thick/2)
                        // Change to yellow
                        colorperpair[ii] = 121;                    // abs('y') = 121 in Matlab
                }

                // Change to red
                for (size_t ii = 0; ii < Npairs; ++ii){
                    if (dist4color_perpair[ii] > +1*Slice_Thick/2)
                        // Change to red
                        colorperpair[ii] = 114;                    // abs('r') = 114 in Matlab
                }

                // Start plotting
                QVector<QPoint> pointPairs;
                for(size_t ii = 0; ii < Npairs; ++ii){
                    if (2*ii+1 < Npts)
                        pointPairs << QPoint(std::ceil(pixel_locs_X_Y[2*ii].first * window_2D_width/ImageNrows), std::ceil(pixel_locs_X_Y[2*ii+1].first * window_2D_width/ImageNrows)) // scaling factor
                                << QPoint(std::ceil(pixel_locs_X_Y[2*ii].second * window_2D_width/ImageNcols), std::ceil(pixel_locs_X_Y[2*ii+1].second * window_2D_width/ImageNcols));
                }

                // PROGRAMMING NOTE : Decide, if it is better in C, to first test if the points are in the image, or to always plot and take advantage of auto-clip.
                // plot( [ pt1(1)  pt2(1) ] , [ pt1(2)  pt2(2) ] , '-' , 'color', colorperpair(perpair), 'LineWidth' , NALineWidth );

                for (int ii = 0; ii < Npairs; ++ii){
                    if (colorperpair[ii] == 121){
                        QPen pen1(Qt::yellow, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());

                        // Get last dashed line coordinates (=Mahamadou=)
                        //line.setP1(QPointF(pointPairs[0].x(),  pointPairs[0].y()));
                        //line.setP2(QPointF(pointPairs[2*Npairs-1].x(),  pointPairs[2*Npairs-1].y()));
                    }
                    else if (colorperpair[ii] == 114){
                        QPen pen1(Qt::red, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                    else if (colorperpair[ii] == 98){
                        QPen pen1(Qt::blue, 4);
                        pen1.setStyle(Qt::DashLine);
                        painter->setPen(pen1);
                        painter->drawLine(pointPairs[2*ii].x(), pointPairs[2*ii+1].x(), pointPairs[2*ii].y(), pointPairs[2*ii+1].y());
                    }
                }

                /* Also plot - mark where the Needle line intersects the Plus Surface and the Minus surface
                *  (i) Plus Surface : 3-component distance from PS_TLC to PS intersect
                */
                if( (psi_centralpix.first >= -1) && (psi_centralpix.first <= ImageNcols-2) && (psi_centralpix.second >= -1)
                        && (psi_centralpix.second <= ImageNrows-2) && (std::abs(psi_centralpix.first) != +999))
                {
                    int target_x =  std::ceil(psi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                    int target_y =  std::ceil(psi_centralpix.second * window_2D_width/ImageNcols);
                    int target_r = 5;

                    QPen pen(Qt::blue, 2);
                    pen.setWidth(4);
                    painter->setPen(pen);

                    QVector<QPoint> pointPairs;
                    pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y + 2*target_r)
                               << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                    painter->drawLines(pointPairs);
                }

                if( (msi_centralpix.first >= -1) && (msi_centralpix.first <= ImageNcols-2) && (msi_centralpix.second >= -1)
                        && (msi_centralpix.second <= ImageNrows-2) && (std::abs(msi_centralpix.first)!= +999))
                {
                    int target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                    int target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                    int target_r = 5;

                    QPen pen(Qt::blue, 2);
                    pen.setWidth(4);
                    painter->setPen(pen);

                    QVector<QPoint> pointPairs;
                    pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y + 2*target_r)
                               << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                    painter->drawLines(pointPairs);
                }

                // Show tip of the Needle
                bool ShowNeedleTip= true;
                if (ShowNeedleTip){
                    int target_x =  std::ceil(tip_plot_pix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                    int target_y =  std::ceil(tip_plot_pix.second * window_2D_width/ImageNcols);
                    //int target_r = 5;

                    QPen pen(Qt::green, 2);
                    pen.setCapStyle(Qt::RoundCap);
                    pen.setWidth(15);
                    painter->setRenderHint(QPainter::Antialiasing,true);
                    painter->setPen(pen);
                    painter->drawPoint(QPoint(target_x, target_y));

                    /********************************************************************************************
                     * Draw arrow (=Mahamadou=)
                     * ******************************************************************************************/
                    /*QPen myPen(Qt::green);
                    qreal arrowSize = 20;
                    painter->setPen(myPen);
                    painter->setBrush(Qt::green);

                    double angle = ::acos(line.dx() / line.length());
                    if (line.dy() >= 0)
                        angle = (M_PI * 2) - angle;

                    line.setP1(QPointF(0.0, 0.0));
                    line.setP2(QPointF(0.0, 0.0));

                    QPointF arrowP1 = QPointF(target_x, target_y) + QPointF(sin(angle + M_PI / 3) * arrowSize,
                                                                            cos(angle + M_PI / 3) * arrowSize);
                    QPointF arrowP2 = QPointF(target_x, target_y) + QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize,
                                                                            cos(angle + M_PI - M_PI / 3) * arrowSize);

                    arrowHead.clear();
                    arrowHead << QPoint(target_x, target_y) << arrowP1 << arrowP2;
                    // Draw the arrow
                    painter->drawPolygon(arrowHead);*/

                }

                // Load the dicom file
                ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                //update();

                auto myString =  QTime::currentTime().toString();
                QString hours   = myString.mid(0, 2);
                QString minutes = myString.mid(3, 2);
                QString seconds = myString.mid(6, 2);
                Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                              QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
            }

        }

        else{ // Draw a needle annotation as target point

            // Parse the image orientation string
            double orientation_matrix[6];
            stringParser(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix, 6);

            // Get image orientation
            char *Letter_H = getOrientation_H(orientation_matrix);
            char *Letter_V = getOrientation_V(orientation_matrix);


            // Get the largest image pixel value
            //uint LargestImagePixelValue = /*std::stoi(sf.ToStringPair(gdcm::Tag(0x0028,0x0107)).second)*/ 1482;

            const gdcm::Image &gimage = ir.GetImage();
            std::vector<char> vbuffer;
            vbuffer.resize( gimage.GetBufferLength() );
            char *buffer = &vbuffer[0];

            // Convert dicom to RGB format
            QImage *imageQt = NULL;
            if( !ConvertToFormat_RGB888( gimage, buffer, imageQt ) )
            {
                std::cerr << "Error during file conversion: ConvertToFormat_RGB888\n";
            }


            // FOV calculation
            // FOVx = Raw * Pixel_spacing[0];
            // FOVy = Col * Pixel_spacing[1];
            std::string token = "\\";
            std::string s = sf.ToStringPair(gdcm::Tag(0x0028,0x0030)).second;

            long double FOVx = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0010)).second) *
                                          std::stod(s.substr(0, s.find(token))) + 0.5);

            s.erase(0, s.find(token) + token.length());

            long double FOVy = std::floor(std::stod(sf.ToStringPair(gdcm::Tag(0x0028,0x0011)).second) *
                                          std::stod(s) + 0.5);


            // Keep aspect ratio
            QImage imageQt_Scaled = imageQt->scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

            // Write some Dicom Info on the display
            std::unique_ptr<QPainter> painter(new QPainter(&imageQt_Scaled));
            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));

            //Draw the text on the image
            painter->drawText( QPoint(2,14),    QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0010,0x0010)).second));
            painter->drawText( QPoint(2,28),    QString::fromStdString("FOV: " + std::to_string(FOVx) + "*" + std::to_string(FOVy)));

            painter->setPen(Qt::white);
            painter->setFont(QFont("Segoe UI",16));
            painter->drawText( QPoint(4,230),    QString::fromStdString(Letter_H));
            painter->drawText( QPoint(218,20),   QString::fromStdString(Letter_V));

            painter->setPen(Qt::green);
            painter->setFont(QFont("Segoe UI", 11));
            painter->drawText( QPoint(267,14),  QString::fromStdString(sf.ToStringPair(gdcm::Tag(0x0008,0x0080)).second));
            painter->drawText( QPoint(267,28),  QString::fromStdString("Date: " + sf.ToStringPair(gdcm::Tag(0x0008,0x0023)).second));
            painter->drawText( QPoint(2,390),   QString::fromStdString("TR: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0080)).second + " ms"));
            painter->drawText( QPoint(2,404),   QString::fromStdString("TE: " + sf.ToStringPair(gdcm::Tag(0x0018,0x0081)).second + " ms"));
            painter->drawText( QPoint(2,418),   QString::fromStdString("FA: " + sf.ToStringPair(gdcm::Tag(0x0018,0x1314)).second + " deg"));


            /********** Draw target on image ***************************/

            /* 2 - learn where the Plus Surface and the Minus Surface are intersected by the 3-D needle annotation.
            * The result of this function is the plot locations on the MR image - "x" and "y", in whole/integer pixel units.
            * Result will = -999 if image plane and needle are parallel.
            * Also, this is the "general" solution, result may be <1, >ImageNrows, read "pixptsfrom3d" for more information.
            */
            std::pair<double, double> psi_centralpix, msi_centralpix;
            double newlineintersect_XYZmm[3]; // Unused variable, but needed for the first part.
            double newlineintersect_XYZmm_2[3]; // Unused variable

            psi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, PS_TLC_XYZmm, PS_BRC_XYZmm, PS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm ) ;
            msi_centralpix =  pix_3d_ptsfrom3d( orig_one_RISC_XYZ, orig_one_RISC_IJK, MS_TLC_XYZmm, MS_BRC_XYZmm, MS_TRC_XYZmm, Im_rowdircoldir, PixSizeLocalYX, newlineintersect_XYZmm_2) ;


            /* 3 - Where is the needle tip in relation to slab - distal to all, proximal to all, or inside the slab?

            * Eric W. Weisstein. "Point-Plane Distance." From MathWorld--A Wolfram Web Resource.
            * http://mathworld.wolfram.com/Point-PlaneDistance.html

            * PROGRAMMING NOTE : in plot_NA_pixels.m, I measured this distance as " from MR image to needle line"
            * and the equation looked like : distance = final - initial, D = dot( ImageNormal , ( thisloc_XYZmm - TLC_XYZmm ) ) ;
            * However, here, the sign of the MRslice - tip distance matches the "Show Distance" reference
            * Octane annotation if the distance is done "from needle tip to MR slice"

            * PLEASE NOTE that D will be compared to HALF the SLICE THICKNESS -
            * "central" MR slice is half way between the PLUS Surface and the MINUS Surface
            */

            //D = dot( ImageNormal , ( TLC_XYZmm - locposPNT(1:3)) ) ;
            double temp[] = {TLC_XYZmm[0] - orig_one_RISC_XYZ[0], TLC_XYZmm[1] - orig_one_RISC_XYZ[1], TLC_XYZmm[2] - orig_one_RISC_XYZ[2]};
            long double D = ImageNormal[0]*temp[0] + ImageNormal[1]*temp[1] + ImageNormal[2]*temp[2];
            D = D /*- 69.5971*/;
            int target_x = -999; //Dummy values
            int target_y = -999;

            /* 3 possibilities :*/
            if (D > Slice_Thick/2){
                // 1) needle tip is proximal to the MR "slab" - physical needle has not entered nor exited the MR slab
                target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                const size_t target_r = 10;

                QPen pen(Qt::blue, 2);
                pen.setWidth(4);
                painter->setPen(pen);


                QVector<QPoint> pointPairs;
                pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y - target_r)
                           << QPoint(target_x, target_y + target_r) << QPoint(target_x, target_y + 2*target_r)
                           << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x - target_r, target_y)
                           << QPoint(target_x + target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                painter->drawEllipse(QPoint(target_x, target_y), target_r, target_r);
                painter->drawLines(pointPairs);

                // Show distance
                painter->setFont(QFont("Time New Roman",10,QFont::Bold));
                painter->drawText( QPoint(target_x+25,target_y+20),   QString::fromStdString(std::to_string(std::ceil(D)) + " mm"));

                // Load the dicom file
                ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                //update();

                //save image
                auto myString =  QTime::currentTime().toString();
                QString hours   = myString.mid(0, 2);
                QString minutes = myString.mid(3, 2);
                QString seconds = myString.mid(6, 2);
                Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                              QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
            }
            else if(D>=-1*Slice_Thick/2 && D<=Slice_Thick/2){
                // 2) needle tip is IN the MR "slab" - physical needle has entered but not exitted the MR slab
                target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                const size_t target_r = 10;

                QPen pen(Qt::green, 2);
                pen.setWidth(4);
                painter->setPen(pen);


                QVector<QPoint> pointPairs;
                pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y - target_r)
                           << QPoint(target_x, target_y + target_r) << QPoint(target_x, target_y + 2*target_r)
                           << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x - target_r, target_y)
                           << QPoint(target_x + target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                painter->drawEllipse(QPoint(target_x, target_y), target_r, target_r);
                painter->drawLines(pointPairs);

                // Show distance
                painter->setFont(QFont("Time New Roman",10,QFont::Bold));
                painter->drawText( QPoint(target_x+25,target_y+20),   QString::fromStdString(std::to_string(std::ceil(D)) + " mm"));

                // Load the dicom file
                ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                //update();

                // Save image
                auto myString =  QTime::currentTime().toString();
                QString hours   = myString.mid(0, 2);
                QString minutes = myString.mid(3, 2);
                QString seconds = myString.mid(6, 2);
                Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                              QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));

            }
            else // or make it explicit : elseif D<-1*Slice_Thick/2,
            {
                // 3) needle tip is distal to the MR slab
                target_x =  std::ceil(msi_centralpix.first * window_2D_width/ImageNrows); // 512/ImageNrows is the scaling factor
                target_y =  std::ceil(msi_centralpix.second * window_2D_width/ImageNcols);
                const size_t target_r = 10;

                QPen pen(Qt::red, 2);
                pen.setWidth(4);
                painter->setPen(pen);


                QVector<QPoint> pointPairs;
                pointPairs << QPoint(target_x, target_y - 2*target_r) << QPoint(target_x, target_y - target_r)
                           << QPoint(target_x, target_y + target_r) << QPoint(target_x, target_y + 2*target_r)
                           << QPoint(target_x - 2*target_r, target_y) << QPoint(target_x - target_r, target_y)
                           << QPoint(target_x + target_r, target_y) << QPoint(target_x + 2*target_r, target_y);

                painter->drawEllipse(QPoint(target_x, target_y), target_r, target_r);
                painter->drawLines(pointPairs);

                // Show distance
                painter->setFont(QFont("Time New Roman",10,QFont::Bold));
                painter->drawText( QPoint(target_x+25,target_y+20),   QString::fromStdString(std::to_string(std::ceil(D)) + " mm"));

                // Load the dicom file
                ui->label_1->setPixmap(QPixmap(QPixmap::fromImage(imageQt_Scaled)));
                ui->label_1->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                //update();

                // Save image
                auto myString =  QTime::currentTime().toString();
                QString hours   = myString.mid(0, 2);
                QString minutes = myString.mid(3, 2);
                QString seconds = myString.mid(6, 2);
                Q_ASSERT(imageQt_Scaled.save( Image_path + QString("/Image/")+ QDate::currentDate().toString("dd.MM.yyyy") +
                                              QString("_") + hours + QString(".") + minutes + QString(".")+ seconds + QString(".png")));
            }
        }
    }
}

void MainWindow::on_plot_clicked()
{
   // plot_XYZ.connect_to_sensor();
    plot_XYZ.show();
    plot_XYZ.raise();
    plot_XYZ.activateWindow();
}
