#-------------------------------------------------
#
# Project created by QtCreator 2015-01-13T10:30:35
#
#-------------------------------------------------

QT       += core gui\
            opengl network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = MINIRII
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    widget_3d.cpp \
    ReadRawData.cpp \
    realtimeplot.cpp \
    qcustomplot.cpp

HEADERS  += mainwindow.h \
    widget_3d.h \
    mdh.h \
    MrBasicTypes.h \
    pack.h \
    ReadRawData.h \
    unpack.h \
    realtimeplot.h \
    qcustomplot.h \
    Vector_3D.h

FORMS    += mainwindow.ui \
    realtimeplot.ui

# Boost
INCLUDEPATH += C:/boost_1_57_0

# GDCM
INCLUDEPATH += C:/gdcm-2.4.4/Source/Common
INCLUDEPATH += C:/gdcm-2.4.4/Source/DataDictionary
INCLUDEPATH += C:/gdcm-2.4.4/Source/DataStructureAndEncodingDefinition
INCLUDEPATH += C:/gdcm-2.4.4/Source/InformationObjectDefinition
INCLUDEPATH += C:/gdcm-2.4.4/Source/MediaStorageAndFileFormat
INCLUDEPATH += C:/gdcm-2.4.4/Source/MessageExchangeDefinition
INCLUDEPATH += C:/gdcm-2.4.4_Build/Source/Common

LIBS += -L"C:/gdcm-2.4.4_Build/bin/Debug" \
    -lgdcmcharls \
    -lgdcmCommon \
    -lgdcmDICT \
    -lgdcmDSED \
    -lgdcmexpat \
    -lgdcmgetopt \
    -lgdcmIOD \
    -lgdcmjpeg8 \
    -lgdcmjpeg12 \
    -lgdcmjpeg16 \
    -lgdcmMEXD \
    -lgdcmMSFF \
    -lgdcmopenjpeg \
    -lgdcmzlib \
    -lsocketxx

# Armadillo
INCLUDEPATH += C:/armadillo-3.920.1/include
LIBS += -L"C:/armadillo-3.920.1/examples/lib_win32" \
    -lblas_win32_MT \
    -llapack_win32_MT

# Opencv
INCLUDEPATH += C:/opencv/MyBuild/include

LIBS += -L"C:/opencv/MyBuild/lib/Debug" \
    -lopencv_calib3d300d \
    -lopencv_core300d \
    -lopencv_features2d300d \
    -lopencv_flann300d \
    -lopencv_highgui300d \
    -lopencv_imgcodecs300d \
    -lopencv_imgproc300d \
    -lopencv_ml300d \
    -lopencv_objdetect300d \
    -lopencv_photo300d \
    -lopencv_shape300d \
    -lopencv_stitching300d \
    -lopencv_superres300d \
    -lopencv_ts300d \
    -lopencv_video300d \
    -lopencv_videoio300d \
    -lopencv_videostab300d


RESOURCES += \
    My_Images.qrc
