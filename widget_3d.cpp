#include "widget_3d.h"
#include <GL/GL.h>
#include <gl/GLU.h>
#include "gdcmTag.h"
#include "gdcmStringFilter.h"
#include <QFileDialog>

// Number of texture(s)to be loaded
const int n = 3;
static GLuint *texName = new GLuint[n];


Widget_3D::Widget_3D(QWidget *parent) :
    QGLWidget(parent)
{
    setAcceptDrops(true);

    rotationX    =  10.0;
    rotationY    =  5.0;
    rotationZ    =  0.0;
    Zoom_along_Z = -4.20f;
}

Widget_3D::~Widget_3D()
{
    // Cleanup
    delete[] texName;
}

QSize Widget_3D::minimumSizeHint() const
{
    return QSize(300, 300);
}

void Widget_3D::loadNewSetOfImages()
{
    // Load sorted images
    map_type images = sortDicomFiles();

    if(images.size() < 3)
        return;

    GL_formatted_image_1 = readDicomFile(images["Coronal"].c_str());
    GL_formatted_image_2 = readDicomFile(images["Sagittal"].c_str());
    GL_formatted_image_3 = readDicomFile(images["Axial"].c_str());
    updateGL();
}

void Widget_3D::initializeGL()
{
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glClearColor(0.8f, 0.8f, 1.0f, 0.0f);
    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);

    GL_formatted_image_1 = QImage();
    GL_formatted_image_2 = QImage();
    GL_formatted_image_3 = QImage();
}

void Widget_3D::resizeGL(int width, int height)
{
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(40.0, (GLfloat)width / (GLfloat)height, 1.0, 30.0);
}

void Widget_3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw();
}

void Widget_3D::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void Widget_3D::mouseMoveEvent(QMouseEvent *event)
{
    GLfloat dx = GLfloat(event->x() - lastPos.x()) / width();
    GLfloat dy = GLfloat(event->y() - lastPos.y()) / height();

    if(event->buttons() & Qt::LeftButton){
        rotationX += 180 * dy;
        rotationY += 180 * dx;
        updateGL();
    } else if(event->buttons() & Qt::RightButton) {
        rotationX += 180 * dy;
        rotationZ += 180 * dx;
        updateGL();
    }

    lastPos = event->pos();
}

void Widget_3D::wheelEvent(QWheelEvent *event)
{
    Zoom_along_Z += (event->delta()/30);
    updateGL();
}

void Widget_3D::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void Widget_3D::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    if(urls.isEmpty())
        return;

    // Read files
    map_type images = sortDicomFiles_dropped(urls);

    if(images.size() < 3)
        return;

    GL_formatted_image_1 = readDicomFile(images["Coronal"].c_str());
    GL_formatted_image_2 = readDicomFile(images["Sagittal"].c_str());
    GL_formatted_image_3 = readDicomFile(images["Axial"].c_str());
    updateGL();

    event->acceptProposedAction();
}

void Widget_3D::draw()
{
    GLubyte rasters_A[13] = {0x00, 0x00, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3, 0xc3, 0x66, 0x3c, 0x18};
    GLubyte rasters_P[13] = {0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0xc7, 0xc3, 0xc3, 0xc7, 0xfe};
    GLubyte rasters_R[13] = {0x00, 0x00, 0xc3, 0xc6, 0xcc, 0xd8, 0xf0, 0xfe, 0xc7, 0xc3, 0xc3, 0xc7, 0xfe};
    GLubyte rasters_L[13] = {0x00, 0x00, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0};
    GLubyte rasters_H[13] = {0x00, 0x00, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3};
    GLubyte rasters_F[13] = {0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xc0, 0xff};

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, Zoom_along_Z);
    // Camera position
    glRotatef(rotationX, 1.0, 0.0, 0.0);
    glRotatef(rotationY, 0.0, 1.0, 0.0);
    glRotatef(rotationZ, 0.0, 0.0, 1.0);

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glGenTextures(n, texName);
    glLineWidth(10);

    // Draw characters
    glColor3f (0.0f, 0.0f, 0.0f);
    glRasterPos3f (1.1f, 0.0f, 0.0f);
    glBitmap (8, 13, 0.0, 0.0, 10.0, 0.0, rasters_L);
    glRasterPos3f (-1.1f, 0.0f, 0.0f);
    glBitmap (8, 13, 0.0, 0.0, 10.0, 0.0, rasters_R);

    glRasterPos3f (0.0f, 1.1f, 0.0f);
    glBitmap (8, 13, 0.0, 0.0, 10.0, 0.0, rasters_A);
    glRasterPos3f (0.0f, -1.1f, 0.0f);
    glBitmap (8, 13, 0.0, 0.0, 10.0, 0.0, rasters_P);

    glRasterPos3f (0.0f, 0.0f, 1.1f);
    glBitmap (8, 13, 0.0, 0.0, 10.0, 0.0, rasters_F);
    glRasterPos3f (0.0f, 0.0f, -1.1f);
    glBitmap (8, 13, 0.0, 0.0, 10.0, 0.0, rasters_H);

    glBegin(GL_LINE_LOOP);
    glColor3f(1.0f, 0.0f, 0.0f); // Red (Coronal)
    glVertex3f(-1.0f, 0.0f, 1.0f);
    glVertex3f(-1.0f, 0.0f, -1.0f);
    glVertex3f(1.0f, 0.0f, -1.0f);
    glVertex3f(1.0f, 0.0f, 1.0f);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glColor3f(0.0f, 1.0f, 0.0f); // Green (Sagittal)
    glVertex3f(0.0f, 1.0f, 1.03f);
    glVertex3f(0.0f, 1.0f, -1.0f);
    glVertex3f(0.0f, -1.0f, -1.0f);
    glVertex3f(0.0f, -1.0f, 1.03f);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glColor3f(0.0f, 0.0f, 1.0f); // Blue (Axial)
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glVertex3f(-1.0f, 1.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 0.0f);
    glEnd();


    if(!GL_formatted_image_1.isNull() && !GL_formatted_image_2.isNull() && !GL_formatted_image_3.isNull()){

        // Proceed with the rest of the initialization (Coronal)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, texName[2]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_formatted_image_1.width(), GL_formatted_image_1.height(), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, GL_formatted_image_1.bits());
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 0.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0);  glVertex3f(1.0f, 0.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, 0.0f, 1.0f);
        glEnd();


        // Proceed with the rest of the initialization (Sagittal)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, texName[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_formatted_image_2.width(), GL_formatted_image_2.height(), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, GL_formatted_image_2.bits());
        glBegin(GL_QUADS);

        glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 1.0f, 1.03f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(0.0f, 1.0f, -0.97f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(0.0f, -1.0f, -0.97f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(0.0f, -1.0f, 1.03f);

        glEnd();


        // Proceed with the rest of the initialization (Axial)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, texName[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_formatted_image_3.width(), GL_formatted_image_3.height(), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, GL_formatted_image_3.bits());
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 0.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 1.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, -1.0f, 0.0f);
        glEnd();

        // Force to execute
        glFlush();

        // Standard cleanup
        glDeleteTextures(n, texName);
        glDisable(GL_TEXTURE_2D);
    }
}

QImage Widget_3D::readDicomFile(const char *filename)
{
    if(filename != nullptr){
        // Read input dicom file
        gdcm::ImageReader ir;
        ir.SetFileName( filename );
        if(!ir.Read())
        {
            std::cerr << "Cannot load Dicom file, abort...\n";
            exit(1);
        }

        // Let's get some info from the header file
        gdcm::File &file = ir.GetFile();
        gdcm::StringFilter  sf = gdcm::StringFilter();
        sf.SetFile(file);

        const gdcm::Image &gimage = ir.GetImage();
        std::vector<char> vbuffer;
        vbuffer.resize( gimage.GetBufferLength() );
        char *buffer = &vbuffer[0];

        // Convert dicom to RGB format
        std::unique_ptr<QImage>imageQt = ConvertToFormat_RGB888( gimage, buffer);

        // Convert QImage to GL format
        QImage GL_formatted_image = QGLWidget::convertToGLFormat(*imageQt);
        if( GL_formatted_image.isNull() )
        {
            std::cerr << "error GL_formatted_image" << std::endl ;
            exit(1);
        }

        return GL_formatted_image;
    }

    return QImage();
}

std::unique_ptr<QImage> Widget_3D::ConvertToFormat_RGB888(const gdcm::Image &gimage, char *buffer)
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

        std::unique_ptr<QImage> imageQt(new QImage(ubuffer, dimX, dimY, QImage::Format_RGB888));

        return imageQt;

        // Be tidy
        delete ubuffer;
        ubuffer = NULL;
    }

    else
    {
        std::cerr << "Unhandled PhotometricInterpretation: " << gimage.GetPhotometricInterpretation() << std::endl;
        std::cerr << "Pixel Format is: " << gimage.GetPixelFormat() << std::endl;
        exit(-1);
    }

    return nullptr;
}

char *Widget_3D::getOrientation_H(double orientation_Matrix[])
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

char *Widget_3D::getOrientation_V(double orientation_Matrix[])
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

map_type Widget_3D::sortDicomFiles()
{
    map_type images;

    // Read all files
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select one or more files to open"), QDir::currentPath());

    // Sort them
    for(size_t ii = 0; ii < files.size(); ++ii)
    {
        // Read input dicom file
        gdcm::ImageReader ir;
        ir.SetFileName( files.at(ii).toLocal8Bit().constData() );
        if(!ir.Read())
        {
            std::cerr << "One or more file(s) are not supported, abort...\n";
            exit(-1);
        }

        // Let's get some info from the header file
        gdcm::File &file = ir.GetFile();
        gdcm::StringFilter  sf = gdcm::StringFilter();
        sf.SetFile(file);
        // Parse the image orientation string
        double orientation_matrix[6];
        parseImagOrient(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix);

        // Get image orientation
        char *Letter_H = getOrientation_H(orientation_matrix);
        char *Letter_V = getOrientation_V(orientation_matrix);

        // Start sorting images
        if(*Letter_H == 'R' && *Letter_V == 'H')
            images["Coronal"] = files.at(ii).toLocal8Bit().constData();

        else if(*Letter_H == 'A' && *Letter_V == 'H')
            images["Sagittal"] = files.at(ii).toLocal8Bit().constData();

        else if(*Letter_H == 'R' && *Letter_V == 'A')
            images["Axial"] = files.at(ii).toLocal8Bit().constData();
        else
        {
            std::cout << "This should never happen, abort...\n";
            exit(-1);
        }
    }

    return images;
}

map_type Widget_3D::sortDicomFiles_dropped(QList<QUrl> urls)
{
    map_type images;

    if(urls.size() >= 3){

        // Sort them
        for(size_t ii = 0; ii < urls.size(); ++ii)
        {
            // Convert Qstring to string literal
            QByteArray byteArray = urls.at(ii).toLocalFile().toUtf8();
            const char *cString = byteArray.constData();

            // Read input dicom file
            gdcm::ImageReader ir;
            ir.SetFileName( cString );
            if(!ir.Read())
            {
                std::cerr << "One or more file(s) are not supported, abort...\n";
                exit(-1);
            }

            // Let's get some info from the header file
            gdcm::File &file = ir.GetFile();
            gdcm::StringFilter  sf = gdcm::StringFilter();
            sf.SetFile(file);
            // Parse the image orientation string
            double orientation_matrix[6];
            parseImagOrient(sf.ToStringPair(gdcm::Tag(0x0020,0x0037)).second, orientation_matrix);

            // Get image orientation
            char *Letter_H = getOrientation_H(orientation_matrix);
            char *Letter_V = getOrientation_V(orientation_matrix);

            // Start sorting images
            if(*Letter_H == 'R' && *Letter_V == 'H')
                images["Coronal"] = cString;

            else if(*Letter_H == 'A' && *Letter_V == 'H')
                images["Sagittal"] = cString;

            else if(*Letter_H == 'R' && *Letter_V == 'A')
                images["Axial"] = cString;
            else
            {
                std::cout << "This should never happen, abort...\n";
                exit(-1);
            }
        }
    }
    return images;
}

void Widget_3D::parseImagOrient(std::string ImageOrientation, double orientation_Matrix[])
{
    int i = 0;
    size_t pos = 0;
    std::string token;
    std::string delimiter = "\\";

    while((pos = ImageOrientation.find(delimiter)) != std::string::npos){
        token = ImageOrientation.substr(0, pos);
        orientation_Matrix[i] = std::stod(token);
        ImageOrientation.erase(0, pos + delimiter.length());
        i++;
    }

    orientation_Matrix[5] = std::stod(ImageOrientation);
}
