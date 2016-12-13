#ifndef DIETOY_MAIN_WINDOW_H
#define DIETOY_MAIN_WINDOW_H

#include "DrawWidget.h"

#include <QVector>
#include <QMainWindow>
#include <opencv2/opencv.hpp>


class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = Q_NULLPTR, Qt::WindowFlags flags = Qt::WindowFlags());
    ~MainWindow();

    bool loadImage(const QString& filename);
    bool saveDescriptionJson(const QString& filename);
    bool loadDescriptionJson(const QString& filename);

    void addOrMoveBoundsPoint(const QPointF& position);
    void dragBoundsPoint(const QPointF& position);
    void stopDraggingBoundsPoint(const QPointF& position);
    
    enum UiMode { Navigation, BoundsDefine, SliceDefineHorizontal, SlideDefineVertical };
    Q_ENUM(UiMode)
    
protected:
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
    
private:
    void clearGeneratedGeometry();
    void computePolyAndHomography();
    QVector<QPointF> sortedRectanglePoints(const QVector<QPointF>& inPoints);
    
private:
    UiMode m_uiMode;
    DrawWidget m_drawWidget;
    
    QImage m_qImage;
    int m_activeBoundsPoint;
    QVector<QPointF> m_boundsPoints;
    QVector<QPolygonF> m_polygons;
    
    cv::Mat m_romRegionHomography;
    
    QMetaObject::Connection m_lmbClickedConnection;
    QMetaObject::Connection m_lmbDraggedConnection;
    QMetaObject::Connection m_lmbReleasedConnection;
};

#endif // DIETOY_MAIN_WINDOW_H
