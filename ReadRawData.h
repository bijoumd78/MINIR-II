/*************************************************************************************
* Description: Class to read Siemens 3T MRI raw data
*              This is a modified version of Ralf Loeffler/Xeni Deligianni original
*              source code. This class is dependent on 4 others files:
*              - mdh.h
*              - MrBasicTypes.h
*              - pack.h
*              - unpack.h
*              
*               NB: The main functions are:
*                  void  ReadMeasVB17(char *filename, MDHType *mdh, RawDataType *Data)
*                  which returns the raw data header info as well as the raw data itself.
*                  QImage imageRcon(const QString &path) which reconstructs the image.
*
* Author: Mahamadou Diakite, PhD
* Institution: University of Maryland Medical Center (UMMC)
* Version: 0.0
* Date: 2/21/2015
*************************************************************************************/

#pragma once

// Turn off legacy C code warnings
#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE 
#endif

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

#include <armadillo>
#include "ReadRawData.h"
#include <opencv2/opencv.hpp>
#include <QImage>


using namespace std;

typedef  vector<pair<float, float>>   RawDataType;
typedef  unordered_map<string, size_t>    MDHType;

class ReadRawData
{
public:
    ReadRawData();

    // Main functions
    bool ReadMeasVB17(char *filename, MDHType *mdh, RawDataType *Data);

    QImage reconImage(const QString &path);


private: // List of utility functions

    // Skipping the initial dummy data that precedes the actual meas data.
    // This function is called by DetermineSize and MeasRead and skips the
    // file pointer through the unnecessary bytes and places it (file ptr)
    // at the beginning of raw data.
    bool skipInitialBytes(FILE *readptr);

    // This routine is used to determine the raw data size,
    // i.e. the number of MDH's and the number of samples.
    bool DetermineSize( char *dataFileName, long *MdhCount, long *SamplesPerADC );

    // The actual transformation takes place in this routine
    bool MeasRead( char *name, long SamplesPerADC, MDHType *mdh, RawDataType* Data);


    //Image recon functions
    arma::cx_cube ifft2_shift(arma::cx_cube& ks);

    arma:: mat Sum_Of_Square(arma::cx_cube& ims);

    void Arma_mat_to_cv_mat(const arma::Mat<double>& arma_mat_in,cv::Mat_<double>& cv_mat_out);

    QImage  cvMatToQImage( const cv::Mat &inMat );

    // Variables
    const int window_2D_width;
};

