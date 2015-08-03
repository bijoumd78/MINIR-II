#ifndef WIDGET_3D_H
#define WIDGET_3D_H

#include <QGLWidget>
#include "gdcmImageReader.h"
#include <QImage>
#include <QImageWriter>
#include <QMouseEvent>
#include <map>
#include <QDragEnterEvent>
#include <QMimeData>

typedef std::map<std::string, std::string> map_type;


class Widget_3D : public QGLWidget
{
    Q_OBJECT
public:
    explicit Widget_3D(QWidget *parent = 0);
    ~Widget_3D();

    QSize minimumSizeHint() const;

   public slots:
        void loadNewSetOfImages();


   protected:
       void initializeGL();
       void resizeGL(int width, int height);
       void paintGL();
       void mousePressEvent(QMouseEvent *event);
       void mouseMoveEvent(QMouseEvent *event);
       void wheelEvent(QWheelEvent *event);
       void dragEnterEvent(QDragEnterEvent *event);
       void dropEvent(QDropEvent *event);

   private:
       // For internal use only (Utility functions)
       void draw();
       QImage readDicomFile(const char* filename);
       std::unique_ptr<QImage> ConvertToFormat_RGB888(gdcm::Image const & gimage, char *buffer);
       // Image orientation
       char* getOrientation_H(double orientation_Matrix[]);
       char* getOrientation_V(double orientation_Matrix[]);

       // Sort localizer files
       map_type sortDicomFiles();
       map_type sortDicomFiles_dropped(QList<QUrl> urls);

       // Parse image orientation string
       void parseImagOrient(std::string ImageOrientation, double orientation_Matrix[]);

       // Internal variables
       GLfloat rotationX;
       GLfloat rotationY;
       GLfloat rotationZ;
       GLfloat Zoom_along_Z;
       QPoint lastPos;
       QImage GL_formatted_image_1;
       QImage GL_formatted_image_2;
       QImage GL_formatted_image_3;

};

#endif // WIDGET_3D_H
