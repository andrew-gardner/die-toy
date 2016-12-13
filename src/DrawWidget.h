#ifndef DIETOY_DRAW_WIDGET_H
#define DIETOY_DRAW_WIDGET_H

#include <QImage>
#include <QString>
#include <QWidget>
#include <QPainter>


/// GUI draw widget ///////////////////////////////////////////////////////////

class DrawWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DrawWidget(QWidget* parent = NULL);
    ~DrawWidget();

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;

    bool setImagePointer(const QImage* image);
    bool setCircleCoordsPointer(const QVector<QPointF>* points);
    bool setConvexPolyPointer(const QVector<QPolygonF>* polys);
    
    void centerImage(const bool& refresh=true);
    void scaleImageToViewport(const bool& refresh=true);
    
signals:
    void leftButtonClicked(const QPointF& position);
    void leftButtonDragged(const QPointF& position);
    void leftButtonReleased(const QPointF& position);
    void middleButtonClicked(const QPointF& position);
    void middleButtonDragged(const QPointF& position);
    void middleButtonReleased(const QPointF& position);
    void rightButtonClicked(const QPointF& position);
    void rightButtonDragged(const QPointF& position);
    void rightButtonReleased(const QPointF& position);
    
protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent* event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent* event) Q_DECL_OVERRIDE;

protected slots:
    void imagePanStart(const QPointF& position);
    void imagePanDrag(const QPointF& position);
    
private:
    // Things that may need to be drawn
    const QImage* m_qImage;
    const QVector<QPointF>* m_circleCoords;
    const QVector<QPolygonF>* m_convexPolygons;
    
    // Mouse movement
    QPointF m_lastPos;
    QPointF m_currentPos;
    qreal m_zoomFactor;
    QPointF m_imageLoc;
    QPointF image2Window(const QPointF& image);
    QPointF window2Image(const QPointF& window);
};



#endif // DIETOY_DRAW_WIDGET_H
