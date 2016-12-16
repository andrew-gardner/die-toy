#include "DrawWidget.h"

#include <QDebug>
#include <QPalette>
#include <QMouseEvent>


DrawWidget::DrawWidget(QWidget* parent)
    : QWidget(parent)
    , m_qImage(NULL)
    , m_circleCoords(NULL)
    , m_convexPolygons(NULL)
    , m_lines(NULL)
    , m_lineColors(NULL)
    , m_lastPos(0, 0)
    , m_currentPos(0, 0)
    , m_zoomFactor(1.0)
    , m_imageLoc(0.0f, 0.0f)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);            // TODO: Look into to see if necessary (and/or how to change color)
    
    // The middle button (panning) gets a default implementation
    connect(this, &DrawWidget::middleButtonClicked, this, &DrawWidget::imagePanStart);
    connect(this, &DrawWidget::middleButtonDragged, this, &DrawWidget::imagePanDrag);
}


DrawWidget::~DrawWidget()
{
    
}


QSize DrawWidget::sizeHint() const
{
    return QSize(400, 200);
}


QSize DrawWidget::minimumSizeHint() const
{
    return QSize(200, 200);
}


void DrawWidget::centerImage(const bool& refresh)
{
    if (m_qImage == NULL)
        return;

    QSizeF foo = (size() - (m_qImage->size() * m_zoomFactor)) * 0.5f;
    m_imageLoc.setX(foo.width());
    m_imageLoc.setY(foo.height());
    
    if (refresh) update();
}


void DrawWidget::scaleImageToViewport(const bool& refresh)
{
    if (m_qImage == NULL)
        return;
    
    if (m_qImage->size().width() < m_qImage->size().height())
        m_zoomFactor = (float)width() / (float)m_qImage->size().width();
    else
        m_zoomFactor = (float)height() / (float)m_qImage->size().height();
    
    if (refresh) update();
}


QPointF DrawWidget::image2Window(const QPointF& image)
{
    return m_imageLoc + (image * m_zoomFactor);
}


QPointF DrawWidget::window2Image(const QPointF& window)
{
    return (window - m_imageLoc) / m_zoomFactor;
}



/// QWidget events ////////////////////////////////////////////////////////////

void DrawWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setWorldMatrixEnabled(true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Transform the camera
    painter.translate(m_imageLoc);
    painter.scale(m_zoomFactor, m_zoomFactor);
    
    // Draw the image
    if (m_qImage)
    {
        painter.drawImage(QPoint(0, 0), *m_qImage);
    }
    
    // Draw the circles
    if (m_circleCoords)
    {
        // TODO: Make the diameter and pen size scale appropriately with zoomFactor
        const float diameter = 10.0f;
        painter.setPen(QColor(255, 0, 0));

        for (int i = 0; i < m_circleCoords->size(); i++)
        {
            // Clipping
            const QRectF& viewportRect = painter.viewport();
            const QPointF clipLocation = (*m_circleCoords)[i] * painter.worldMatrix();
            if (clipLocation.x() < -diameter || clipLocation.y() < -diameter)
                continue;
            if (clipLocation.x() > viewportRect.width()+diameter || clipLocation.y() > viewportRect.height()+diameter)
                continue;
            
            // Draw everyone that survives the clippage
            painter.save();
            painter.translate((*m_circleCoords)[i]);
            painter.drawEllipse(QRectF(-diameter / 2.0, -diameter / 2.0, diameter, diameter));
            painter.restore();
        }
    }
    
    // Draw the polygons
    if (m_convexPolygons)
    {
        painter.setPen(QColor(0, 255, 0));
        
        for (int i = 0; i < m_convexPolygons->size(); i++)
        {
            painter.drawConvexPolygon((*m_convexPolygons)[i]);
        }
    }
    
    // Draw the lines
    if (m_lines)
    {
        painter.setPen(QColor(0, 0, 255));
        
        for (int i = 0; i < m_lines->size(); i++)
        {
            if (m_lineColors && m_lineColors->size() > i)
            {
                painter.setPen((*m_lineColors)[i]);
            }
            
            painter.drawLine((*m_lines)[i]);
        }
    }
}


void DrawWidget::wheelEvent(QWheelEvent* event)
{
    QPointF b = window2Image(event->pos());

    // Compute a new zoom factor, but clamp at (0.01, 500.0)
    qreal newZoom = m_zoomFactor;
    if (event->delta() > 0)
        newZoom *= 1.5f;
    else
        newZoom *= 0.75f;
    
    if (newZoom > 500.0f || newZoom < 0.01f)
        return;
    
    m_zoomFactor = newZoom;
    m_imageLoc = event->pos() - (b * m_zoomFactor);

    update();
}


void DrawWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_C)
    {
        centerImage();
    }
    else if (event->key() == Qt::Key_F)
    {
        scaleImageToViewport(false);
        centerImage();
    }
    else if (event->key() == Qt::Key_Z)
    {
        m_zoomFactor = 1.0f;
        centerImage(false);
        update();
    }
    else
    {
        // We not interested in the keypress?  Pass it up the inheritance chain
        QWidget::keyPressEvent(event);
    }
}


void DrawWidget::mousePressEvent(QMouseEvent* event)
{
    QPointF imagePointF = window2Image(event->pos());

    if (event->buttons() & Qt::LeftButton)
    {
        emit leftButtonClicked(imagePointF);
    }
    
    if (event->buttons() & Qt::MidButton)
    {
        emit middleButtonClicked(event->pos());
    }
    
    if (event->buttons() & Qt::RightButton)
    {
      	emit rightButtonClicked(imagePointF);
    }
}


void DrawWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPointF imagePointF = window2Image(event->pos());

    if (event->buttons() & Qt::LeftButton)
    {
        emit leftButtonDragged(imagePointF);
    }

    if (event->buttons() & Qt::MidButton)
    {
        emit middleButtonDragged(event->pos());
    }
    
    if (event->buttons() & Qt::RightButton)
    {
      	emit rightButtonDragged(imagePointF);
    }
}


void DrawWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QPointF imagePointF = window2Image(event->pos());

    if (event->buttons() & Qt::LeftButton)
    {
        emit leftButtonReleased(imagePointF);
    }

    if (event->buttons() & Qt::MidButton)
    {
        emit middleButtonReleased(event->pos());
    }
    
    if (event->buttons() & Qt::RightButton)
    {
      	emit rightButtonReleased(imagePointF);
    }
}


/// Default implementation of slots ///////////////////////////////////////////

void DrawWidget::imagePanStart(const QPointF& position)
{
    m_lastPos = position;    
}


void DrawWidget::imagePanDrag(const QPointF& position)
{
    const QPointF dMove = position - m_lastPos;
    m_imageLoc += dMove;

    m_lastPos = position;
    update();
}
