/*************************************************************************************
* Description: Class to read Siemens 3T MRI raw data
*              This is a modified version of Ralf Loeffler/Xeni Deligianni original
*              source code. This class is dependent on 4 others files:
*              - mdh.h
*              - MrBasicTypes.h
*              - pack.h
*              - unpack.h
*              
*               NB: The main function is:
*                  void  ReadMeasVB17(char *filename, MDHType *mdh, RawDataType *Data)
*                  which returns the raw data header info as well as the raw data itself.
*
* Author: Mahamadou Diakite, PhD
* Institution: University of Maryland Medical Center (UMMC)
* Version: 0.0
* Date: 2/21/2015
*************************************************************************************/


#include "ReadRawData.h"
#include "mdh.h"                 // Numaris include

#define MDH_ACQEND 1L << 0
// We do not have ADC's with more than 4k points, so we should be save
enum{MAX_NO_DATA_POINTS = 8192};

// Ctor
ReadRawData::ReadRawData():
    window_2D_width(420)
{

}

// DetermineSize determines the number of MDH's and the number of samples
bool ReadRawData::DetermineSize( char *dataFileName, long *MdhCount, long *SamplesPerADC )
{	
    sMDH MDH;				                // MDH structure, data from file is read into it
    float raw_columns[MAX_NO_DATA_POINTS];	// Float array, used to read the raw data into.
    FILE *readptr;

    // open file for reading
    readptr = fopen( dataFileName,"rb");
    if ( readptr == nullptr)
    {
        cerr << "Error source file: " <<  dataFileName << " not found\n";
        return false;
    }
    
    // First N bytes of the meas file have to be skipped before we reach the actual meas data
    // (MDH-ComplexData-MDH-ComplexData-,..); here N is the int_32t given by the first 4 bytes.
    if (!skipInitialBytes(readptr))
    {
        cerr << "Error", "Call to skipInitialBytes(FILE*) failed.\n";
        return false ;
    }

    // Start reading in the actual data
    long tempcounter       = 0;
    long ComplexDataPoints = 0;
    long DataSamples       = 0;

    // Loop over the raw data
    // We only know whether the MDH is the last one once we have read it
    do {
        // By updating SamplesPerADC and MdhCount at the beginning, we ensure that the last acqusition (ACQEND) is ignored.
        *SamplesPerADC = max(ComplexDataPoints, *SamplesPerADC);
        *MdhCount      = tempcounter;

        // Read the MDH which precedes the raw data
        long readitems = fread(& MDH,sizeof(sMDH),1,readptr);
        if (readitems != 1){
            cerr << "Error could not read full length of MDH from source file: " << dataFileName << endl;
            return false;
        }

        // Here goes the MDH filling
        ComplexDataPoints = MDH.ushSamplesInScan;
        DataSamples = 2 * ComplexDataPoints;

        // Read the actual data
        readitems = fread(& raw_columns, sizeof(float), DataSamples, readptr);
        if (readitems != DataSamples)
        {
            cerr << "Error could only read " << readitems << "instead of desired " << DataSamples
                 << "data points  from source file: " << dataFileName << endl;
            return false;
        }

        // Increment the help counter with each MDH read
        tempcounter++;
    } while (!(MDH.aulEvalInfoMask[0] & MDH_ACQEND));

    // close file and fill return variable
    fclose(readptr);

    return true;
}

/// MeasRead does the transformation
bool ReadRawData::MeasRead( char *dataFileName, long SamplesPerADC, MDHType *mdh, RawDataType *RawData)
{
    sMDH MDH;
    float raw_columns[MAX_NO_DATA_POINTS];
    FILE *readptr;

    // Avoid unnecessary reallocation
    RawData->reserve(MAX_NO_DATA_POINTS);

    // Open file for read access
    readptr = fopen( dataFileName,"rb");
    if ( readptr == nullptr)
    {
        cerr << "Error source file " << dataFileName << "not found\n";
        return false;
    }

    // First N bytes of the meas file have to be skipped before we reach the actual meas data
    // (MDH-ComplexData-MDH-ComplexData-,.....); here N is the int_32t given by the first 4 bytes.
    if (! skipInitialBytes(readptr))
    {
        cerr << "Call to skipInitialBytes(FILE*) failed.\n";
        return false;
    }

    // Helper variables
    long MdhCounter=0;
    long DataSamples = 0;
    long ComplexDataPoints = 0;

    // Loop over the raw data
    // We only know whether the MDH is the last one once we have read it
    do {
        // read in the MDH
        long readitems = fread(& MDH,sizeof(sMDH),1,readptr);
        if (readitems != 1){
            cerr << "Error could not read full length of MDH from source file: " << dataFileName << endl;
            return false;
        }

        // Here goes the MDH filling
        ComplexDataPoints = MDH.ushSamplesInScan ;
        DataSamples = 2 * ComplexDataPoints;

        if(DataSamples > MAX_NO_DATA_POINTS){
            cerr << "Error: data vector longer that 8k (which is weird)\n";
            return false;
        }

        // Here we start reading the raw data
        readitems = fread(& raw_columns,sizeof(float),DataSamples,readptr);
        if (readitems != DataSamples)
        {
            cerr << "Error could only read "<< readitems << "instead of desired " << DataSamples
                 << " data samples  from source file: " << dataFileName << endl;
            return false;
        }

        // Loop over the single data points
        for (int i = 0; i < ComplexDataPoints; i++){
            RawData->push_back( make_pair(raw_columns[i*2], raw_columns[(i*2)+1]) );
        }

        MdhCounter++;

    } while (!(MDH.aulEvalInfoMask[0] & MDH_ACQEND));


    // Remark: we do not read to the end of the file, if there are more than one coils connected,
    // but the last ADC carries no informatin anyway, so no harm done...
    fclose(readptr);

    //cout <<"\nI converted " << MdhCounter-1 << " MDH's with a maximum length of " << SamplesPerADC << " samples\n";

    // Fill MDH header
    mdh->insert( make_pair("NoOfColumns"  , static_cast<size_t>(SamplesPerADC)          ));
    mdh->insert( make_pair("NoOfLine"     , static_cast<size_t>(MDH.sLC.ushLine+1)      ));
    mdh->insert( make_pair("NoOfCoil"     , static_cast<size_t>(MDH.ushUsedChannels)    ));
    mdh->insert( make_pair("NoOfSlice"    , static_cast<size_t>(MDH.sLC.ushSlice+1)     ));
    mdh->insert( make_pair("NoOfRep"      , static_cast<size_t>(MDH.sLC.ushRepetition+1)));
    mdh->insert( make_pair("NoOfEcho"     , static_cast<size_t>(MDH.sLC.ushEcho+1)      ));
    mdh->insert( make_pair("NoOfPartition", static_cast<size_t>(MDH.sLC.ushPartition+1) ));

    return true;
}

arma::cx_cube ReadRawData::ifft2_shift(arma::cx_cube &ks)
{
    arma::cx_cube temp = arma::zeros<arma::cx_cube>(ks.n_rows, ks.n_cols, ks.n_slices);

    // ifft2
    for (size_t ii = 0 ; ii < ks.n_slices; ++ii)
        temp.slice(ii) = ifft2(ks.slice(ii));

    arma::cx_cube ims = arma::zeros<arma::cx_cube>(temp.n_rows, temp.n_cols, temp.n_slices);
    // Shifting
    ims( arma::span(0, temp.n_rows/2-1)           , arma::span(0, temp.n_cols/2-1),           arma::span::all )    =  temp( arma::span(temp.n_rows/2, temp.n_rows-1), arma::span(temp.n_cols/2, temp.n_cols-1), arma::span::all   );
    ims( arma::span(temp.n_rows/2, temp.n_rows-1) , arma::span(0, temp.n_cols/2-1),           arma::span::all )    =  temp( arma::span(0, temp.n_rows/2-1)          , arma::span(temp.n_cols/2, temp.n_cols-1), arma::span::all   );
    ims( arma::span(0, temp.n_rows/2-1)           , arma::span(temp.n_cols/2, temp.n_cols-1), arma::span::all )    =  temp( arma::span(temp.n_rows/2, temp.n_rows-1), arma::span(0, temp.n_cols/2-1),           arma::span::all   );
    ims( arma::span(temp.n_rows/2, temp.n_rows-1 ), arma::span(temp.n_cols/2, temp.n_cols-1), arma::span::all )    =  temp( arma::span(0, temp.n_rows/2-1)          , arma::span(0, temp.n_cols/2-1),           arma::span::all   );

    return ims;
}

arma::mat ReadRawData::Sum_Of_Square(arma::cx_cube &ims)
{
    // combine coils R.O.S
    arma::cube temp = arma::square(abs(ims));
    arma::mat temp_1 = arma::zeros(ims.n_rows, ims.n_cols);

    for(size_t ii = 0; ii < ims.n_slices; ++ii)
        temp_1 += temp.slice(ii);

    return arma::sqrt(temp_1);
}

void ReadRawData::Arma_mat_to_cv_mat(const arma::Mat<double> &arma_mat_in, cv::Mat_<double> &cv_mat_out)
{
    cv::transpose(cv::Mat_<double>(static_cast<int>(arma_mat_in.n_cols),
                                   static_cast<int>(arma_mat_in.n_rows),
                                   const_cast<double*>(arma_mat_in.memptr())),
                  cv_mat_out);
}

QImage ReadRawData::cvMatToQImage(const cv::Mat &inMat)
{
    // 8-bit, 1 channel (gray scaled image)
    static QVector<QRgb>  sColorTable;

    // only create our color table once
    if ( sColorTable.isEmpty() )
    {
        for ( int i = 0; i < 256; ++i )
            sColorTable.push_back( qRgb( i, i, i ) );
    }

    QImage image( inMat.data, inMat.cols, inMat.rows, inMat.step, QImage::Format_Indexed8 );

    image.setColorTable( sColorTable );

    return image;
}

/// Skips the initial bytes and places the FILE pointer at the start of actual meas data. 
bool ReadRawData::skipInitialBytes(FILE *readptr)
{
    long bytesToBeSkipped = 0;
    long readitems = fread(&bytesToBeSkipped, sizeof(int32_t), 1, readptr);

    rewind(readptr); // now put the pointer back to tbe beginning of the file.

    // Dynamically allocated memory
    char *dummy = new char [bytesToBeSkipped+1]; // 1 extra bytes allocated; just to be safe

    if (dummy == nullptr)
    {
        cerr << "Error allocating memory!\n";
        return false;
    }

    readitems = fread(dummy, sizeof(char), bytesToBeSkipped, readptr); // This is where we skip the bytes to reach actual meas data.

    if (readitems != bytesToBeSkipped)
    {
        cerr << "Source of error: skipInitialBytes(FILE *).  Could only read " << readitems << " instead of desired "
             << bytesToBeSkipped << " dummy characters from source file.\n";
        return false;
    }

    // Be tidy
    delete [] dummy;

    return true;
}

bool ReadRawData::ReadMeasVB17(char *filename, MDHType *mdh, RawDataType *Data)
{
    // Declare some helper variables
    long MdhCount = 0;
    long SamplesPerADC = 0;

    // Determine MdhCount and SamplesPerADC
    if (DetermineSize(filename, &MdhCount, &SamplesPerADC) == false)
    {
        cerr << "Function DetermineSize returned with an error\n";
        return false;
    }

    // Read in the raw data
    if (!MeasRead(filename, SamplesPerADC, mdh, Data)){
        cerr << "Source of Error: MeasRead(), abort...\n";
        return false;
    }

    return true;
}

QImage ReadRawData::reconImage(const QString &path)
{
    //Input parmameters
    static long factor = 10000000L; //Scaling factor of the raw data
    ReadRawData ks1;
    MDHType mdh;
    RawDataType Data;

    // Convert Qstring to string literal
    QByteArray byteArray = path.toUtf8();
    const char *filename = byteArray.constData();

    // Read the raw data
    if (!ks1.ReadMeasVB17(const_cast<char *>(filename), &mdh, &Data)){
        cout << "Error calling ReadMeasVB17(), abort...\n";
        exit(-1);
    }

    // Get the matrix dimensions
    size_t rows     = mdh["NoOfLine"];
    size_t columns  = mdh["NoOfColumns"];
    size_t channels = mdh["NoOfCoil"];

    // Allocate memory
    arma::cube real_p(columns, rows, channels);
    arma::cube imag_p(columns, rows,  channels);


    // Reorder the raw data (Mahamadou)
    for(size_t pp = 0; pp < channels; ++pp){
        size_t kk = 0;
        kk = pp*columns;
        for (size_t jj = 0; jj < rows; ++jj){
            for (size_t ii = 0; ii <columns; ++ii){
                real_p(ii,jj,pp) = Data[kk].first;
                imag_p(ii,jj,pp) = Data[kk].second;
                ++kk;
            }
            if (kk <= columns*rows*channels)
                kk += (channels-1)*columns;
        }
    }

    // Create the complex matrix
    arma::cx_cube ks = arma::cx_cube(real_p, imag_p);

    // FFT
    arma::cx_cube ims = ifft2_shift(ks);

    // Combine the coils using Sum Of the Squares
    arma::mat ims_mag = Sum_Of_Square(ims);

    // Get rid off the readout oversampling
    arma::mat ims_mag_1 = ims_mag(arma::span((ims_mag.n_rows/4 +1), 3*ims_mag.n_rows/4), arma::span::all);

    // Create a cv matrix
    cv::Mat_<double> ims_mag_cv(ims_mag_1.n_rows, ims_mag_1.n_cols);

    // Convert Armadillo matrix to cv matrix
    Arma_mat_to_cv_mat(ims_mag_1, ims_mag_cv);

    // Cast the magnitude image to integer values
    cv::Mat_<int> ims_mag_cv_int = static_cast<cv::Mat_<int>>(factor*ims_mag_cv);

    // Compute maximum pixel value
    int largestPixelValue = 0;
    for (int i = 0; i < ims_mag_cv_int.rows; i++)
        for (int j = 0; j < ims_mag_cv_int.cols; j++)
            largestPixelValue = max(largestPixelValue, ims_mag_cv_int[i][j]);

    // Get the image
    cv::Mat img = cv::Mat(ims_mag_cv_int.rows, ims_mag_cv_int.cols, CV_8UC1);
    for (int i = 0; i < ims_mag_cv_int.rows; i++)
        for (int j = 0; j < ims_mag_cv_int.cols; j++)
            img.at<uchar>(i, j) = static_cast<uchar> (255 * ims_mag_cv_int[i][j] / largestPixelValue);

    // Convert CV image to QT image
    QImage image = cvMatToQImage(img);

    // Keep aspect ratio
    QImage imageQt_Scaled = image.scaled(window_2D_width, window_2D_width, Qt::KeepAspectRatio);

    return imageQt_Scaled;
}
