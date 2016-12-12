#ifndef __MAIN_WINDOW_H__
#define __MAIN_WINDOW_H__

#include "DrawWidget.h"

#include <QVector>
#include <QMainWindow>


class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = Q_NULLPTR, Qt::WindowFlags flags = Qt::WindowFlags());
    ~MainWindow();

    bool loadImage(const QString& filename);
    bool saveDescriptionJson(const QString& filename);
    bool loadDescriptionJson(const QString& filename);

    void addBoundsPoint(const QPointF& position);
    
    enum UiMode { Navigation, BoundsDefine, SliceDefine };
    Q_ENUM(UiMode)
    
protected:
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
    
private:
    DrawWidget m_drawWidget;
    UiMode m_uiMode;
    
    QImage m_qImage;
    QVector<QPointF> m_boundsPoints;
    QVector<QPolygonF> m_polygons;
};

#endif // __MAIN_WINDOW_H__
