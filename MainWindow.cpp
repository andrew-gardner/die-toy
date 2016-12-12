#include "MainWindow.h"

#include <QDebug>
#include <QWidget>
#include <QAction>
#include <QVector2D>
#include <QKeyEvent>
#include <QJsonArray>
#include <QFileDialog>
#include <QJsonObject>
#include <QApplication>
#include <QJsonDocument>


MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : m_uiMode(Navigation)
    , m_drawWidget()
    , m_qImage()
    , m_boundsPoints()
    , m_polygons()
    , m_romRegionHomography()
    , m_lmbConnection()
{
    // Set the draw widget to be central and make sure it can receive focus via the mouse
    setCentralWidget(&m_drawWidget);
    m_drawWidget.setFocusPolicy(Qt::ClickFocus);
    
    // A decent quit keyboard shortcut
    QAction* quitAct = new QAction(this);
    quitAct->setShortcuts(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &MainWindow::close);
    addAction(quitAct);
    
    // Make sure the draw widget is focused when the app loop starts
    m_drawWidget.setFocus();

    // Register our local data with the pointers in the drawImage    
    m_drawWidget.setImagePointer(&m_qImage);
    m_drawWidget.setCircleCoordsPointer(&m_boundsPoints);
    m_drawWidget.setConvexPolyPointer(&m_polygons);
}


MainWindow::~MainWindow()
{
    
}


bool MainWindow::loadImage(const QString& filename)
{
    // Load off disk
    bool success = m_qImage.load(filename);
    if (success == false)
    {
        qWarning() << "Error opening image " << filename;
        return false;
    }
    
    // Scale the image to the viewport if need be
    if (m_qImage.size().width() > m_drawWidget.size().width() ||
        m_qImage.size().height() > m_drawWidget.size().height())
        m_drawWidget.scaleImageToViewport(false);

    m_drawWidget.centerImage();
    
    return true;
}


bool MainWindow::saveDescriptionJson(const QString& filename)
{
    // Open the file for writing
    QFile file;
    file.setFileName(filename);
    bool success = file.open(QIODevice::WriteOnly | QIODevice::Text);
    if (!success)
        return false;
    
    // Write 
    QJsonObject root;
    root["fileType"] = "Die Description File";
    root["version"] = (int)1;
    
    //QJsonArray array;
    //QJsonObject foo;
    //foo["bar"] = 5;
    //array.append(foo);
    //root["hi"] = array;
    
    // Write, close, and cleanup
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}


bool MainWindow::loadDescriptionJson(const QString& filename)
{
    // Open and read the file
    QFile file;
    file.setFileName(filename);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString jsonString = file.readAll();
    file.close();
    
    // Parse the JSON from the die descriptor file
    QJsonParseError jError;
    QJsonDocument jDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &jError);
    if(jError.error != QJsonParseError::NoError)
        return false;
    
    // Begin interpreting
    QJsonObject jObj = jDoc.object();
    
    // TODO: Read
    
    return true;
}


void MainWindow::addBoundsPoint(const QPointF& position)
{
    // Our ROM region can only be 4-sided
    if (m_boundsPoints.size() < 4)
    {
        m_boundsPoints.push_back(position);

        m_polygons.clear();
        m_romRegionHomography.release();
        
        if (m_boundsPoints.size() == 4)
        {
            m_boundsPoints = sortedRectanglePoints(m_boundsPoints);
            m_polygons.push_back(QPolygonF(m_boundsPoints));
            
            std::vector<cv::Point2f> worldSpacePoints;
            worldSpacePoints.push_back(cv::Point2f(m_boundsPoints[0].x(), m_boundsPoints[0].y()));
            worldSpacePoints.push_back(cv::Point2f(m_boundsPoints[1].x(), m_boundsPoints[1].y()));
            worldSpacePoints.push_back(cv::Point2f(m_boundsPoints[2].x(), m_boundsPoints[2].y()));
            worldSpacePoints.push_back(cv::Point2f(m_boundsPoints[3].x(), m_boundsPoints[3].y()));
            
            std::vector<cv::Point2f> lensletSpacePoints;
            lensletSpacePoints.push_back(cv::Point2f(0.0f, 0.0f));
            lensletSpacePoints.push_back(cv::Point2f(1.0f, 0.0f));
            lensletSpacePoints.push_back(cv::Point2f(1.0f, 1.0f));
            lensletSpacePoints.push_back(cv::Point2f(0.0f, 1.0f));
            
            m_romRegionHomography = cv::findHomography(lensletSpacePoints, worldSpacePoints, 0);        
        }

        m_drawWidget.update();
    }
}


void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_O)
    {
        // TEST
        //QString filename = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Images (*.png *.jpg *.tif)"));
        //if (filename != "")
        //    loadImage(filename);
        
        qInfo() << loadDescriptionJson("/tmp/boo.ddf");
    }
    else if (event->key() == Qt::Key_S)
    {
        // TEST
        qInfo() << saveDescriptionJson("/tmp/boo.ddf");
    }
    else if (event->key() == Qt::Key_0)
    {
        m_uiMode = Navigation;
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        disconnect(m_lmbConnection);
    }
    else if (event->key() == Qt::Key_1)
    {
        m_uiMode = BoundsDefine;
        QApplication::setOverrideCursor(Qt::CrossCursor);
        m_lmbConnection = connect(&m_drawWidget, &DrawWidget::leftButtonClicked, this, &MainWindow::addBoundsPoint);
    }
    else
    {
        // We're not interested in the keypress?  Pass it up the inheritance chain
        QMainWindow::keyPressEvent(event);
    }
}


QVector<QPointF> MainWindow::sortedRectanglePoints(const QVector<QPointF>& inPoints)
{
    // Get the points' centroid
    QVector2D centroid;
    for (int i = 0; i < inPoints.size(); i++)
    {
        centroid += QVector2D(inPoints[i]);
    }
    centroid /= inPoints.size();
    
    // Now organize each point by their angles compared to this centroid
    QVector<QPointF> results(4);
    for (int i = 0; i < inPoints.size(); i++)
    {
        const qreal pi2 = M_PI / 2.0;
        const QVector2D normalizedPoint = (QVector2D(inPoints[i]) - centroid).normalized();
        const qreal angle = atan2(normalizedPoint.y(), normalizedPoint.x());
        if (angle < 0.0f)
        {
            if (angle < -pi2) 
                results[0] = inPoints[i];
            else 
                results[1] = inPoints[i];
        }
        else
        {
            if (angle < pi2) 
                results[2] = inPoints[i];
            else 
                results[3] = inPoints[i];
        }
    }
    
    return results;
}
