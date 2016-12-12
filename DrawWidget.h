#ifndef __DRAW_WIDGET_H__
#define __DRAW_WIDGET_H__

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
    
    bool setImage(const QImage& image, const bool& refresh=true);
    void centerImage(const bool& refresh=true);
    void scaleImageToViewport(const bool& refresh=true);
    
signals:
    void leftButtonClicked(const QPointF& position);
    void leftButtonDragged(const QPointF& position);
    void middleButtonClicked(const QPointF& position);
    void middleButtonDragged(const QPointF& position);
    void rightButtonClicked(const QPointF& position);
    void rightButtonDragged(const QPointF& position);
    
protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent* event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;

protected slots:
    void imagePanStart(const QPointF& position);
    void imagePanDrag(const QPointF& position);
    
private:
    QImage m_qImage;
    
    // Mouse movement
    QPointF m_lastPos;
    QPointF m_currentPos;
    qreal m_zoomFactor;
    QPointF m_imageLoc;
    QPointF window2Image(const QPointF& window);
    QPointF image2Window(const QPointF& image);
};



#endif // __DRAW_WIDGET_H__
