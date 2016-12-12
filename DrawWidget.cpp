#include "DrawWidget.h"

#include <QDebug>
#include <QPalette>
#include <QMouseEvent>


DrawWidget::DrawWidget(QWidget* parent)
    : QWidget(parent)
    , m_qImage()
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


bool DrawWidget::setImage(const QImage& image, const bool& refresh)
{
    m_qImage = image;
    if (refresh) update();
}


void DrawWidget::centerImage(const bool& refresh)
{
    QSizeF foo = (size() - (m_qImage.size() * m_zoomFactor)) * 0.5f;
    m_imageLoc.setX(foo.width());
    m_imageLoc.setY(foo.height());
    
    if (refresh) update();
}


void DrawWidget::scaleImageToViewport(const bool& refresh)
{
    if (m_qImage.size().width() < m_qImage.size().height())
        m_zoomFactor = (float)width() / (float)m_qImage.size().width();
    else
        m_zoomFactor = (float)height() / (float)m_qImage.size().height();
    
    if (refresh) update();
}



/// QWidget events ////////////////////////////////////////////////////////////

void DrawWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);    // TODO: Verify this is a good idea for images
    
    painter.translate(m_imageLoc);
    painter.scale(m_zoomFactor, m_zoomFactor);
    painter.drawImage(QPoint(0, 0), m_qImage);
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

    QWidget::keyPressEvent(event);
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



/// Private ///////////////////////////////////////////////////////////////////

QPointF DrawWidget::window2Image(const QPointF& window)
{
    return (window - m_imageLoc) / m_zoomFactor;
}


QPointF DrawWidget::image2Window(const QPointF& image)
{
    return m_imageLoc + (image * m_zoomFactor);
}
