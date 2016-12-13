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
    , m_activeBoundsPoint(-1)
    , m_boundsPoints()
    , m_boundsPolygons()
    , m_horizSlices()
    , m_sliceLines()
    , m_romRegionHomography()
    , m_lmbClickedConnection()
    , m_lmbDraggedConnection()
    , m_lmbReleasedConnection()
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
    m_drawWidget.setConvexPolyPointer(&m_boundsPolygons);
    m_drawWidget.setLinesPointer(&m_sliceLines);
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
    
    // Clear current state
    clearGeneratedGeometry();
    m_boundsPoints.clear();
    m_activeBoundsPoint = -1;
    
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
    
    QJsonArray array;
    for (int i = 0; i < m_boundsPoints.size(); i++)
    {
        QJsonArray qPointFJson;
        qPointFJson.append(m_boundsPoints[i].x());
        qPointFJson.append(m_boundsPoints[i].y());
        array.append(qPointFJson);
    }
    root["romBounds"] = array;
    
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
    bool success = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!success)
        return false;
    
    // Read the entire file
    QString jsonString = file.readAll();
    file.close();
    
    // Parse the JSON from the die descriptor file
    QJsonParseError jError;
    QJsonDocument jDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &jError);
    if(jError.error != QJsonParseError::NoError)
        return false;
    
    // Begin interpreting
    QJsonObject docObj = jDoc.object();
    
    // Make sure we're the correct file type
    const QString fileType = docObj["fileType"].toString();
    if (fileType.isEmpty() || fileType != "Die Description File")
    {
        qWarning() << "Invalid DDF file.  Aborting read";
        return false;
    }

    // Check the version
    const int version = docObj["version"].toInt();
    if (version > 1 || version == 0)
    {
        qWarning() << "Can only read DDF file versions 1 or less";
        return false;
    }
    
    // Read the bounds points
    m_boundsPoints.clear();
    m_activeBoundsPoint = -1;
    const QJsonArray romBounds = docObj["romBounds"].toArray();
    for (int i = 0; i < romBounds.size(); i++)
    {
        const QJsonArray pointArray = romBounds[i].toArray();
        m_boundsPoints.push_back(QPointF(pointArray[0].toDouble(), pointArray[1].toDouble()));
    }
    
    
    // Compute the geometry, and update the view
    computePolyAndHomography();
    recomputeLinesFromHomography();
    m_drawWidget.update();
    
    return true;
}


void MainWindow::addOrMoveBoundsPoint(const QPointF& position)
{
    // First check to see if a current bounds point is close (you're selecting instead of adding a new)
    for (int i = 0; i < m_boundsPoints.size(); i++)
    {
        // TODO: Scale selection radius based on drawWidget zoom factor
        const qreal distance = (m_boundsPoints[i] - position).manhattanLength();
        if (distance < 10.0f)
        {
            m_activeBoundsPoint = i;
            return;
        }
    }
    
    // Our ROM region can only be 4-sided
    if (m_boundsPoints.size() < 4)
    {
        m_boundsPoints.push_back(position);

        if (m_boundsPoints.size() == 4)
        {
            computePolyAndHomography();
            recomputeLinesFromHomography();
        }

        m_drawWidget.update();
    }
}


void MainWindow::dragBoundsPoint(const QPointF& position)
{
    if (m_activeBoundsPoint < 0)
        return;
    
    if (m_boundsPoints.size() == 4)
    {
        computePolyAndHomography();
        recomputeLinesFromHomography();
    }
    
    m_boundsPoints[m_activeBoundsPoint] = position;
    m_drawWidget.update();
}


void MainWindow::stopDraggingBoundsPoint(const QPointF& position)
{
    if (m_activeBoundsPoint < 0)
        return;
    
    if (m_boundsPoints.size() == 4)
    {
        computePolyAndHomography();
        recomputeLinesFromHomography();
    }
    
    m_boundsPoints[m_activeBoundsPoint] = position;
    m_activeBoundsPoint = -1;
}


void MainWindow::addOrMoveSlice(const QPointF& position)
{
    // You can only add a slice if there's a bounds poly
    if (m_boundsPolygons.size() == 0)
        return;
    
    // You can only do something by clicking side the bounds poly
    if (!m_boundsPolygons[0].containsPoint(position, Qt::OddEvenFill))
        return;
    
//    // First check to see if a current slice line is close (you're selecting instead of adding a new)
//    for (int i = 0; i < m_boundsPoints.size(); i++)
//    {
//        // TODO: Scale selection radius based on drawWidget zoom factor
//        const qreal distance = (m_boundsPoints[i] - position).manhattanLength();
//        if (distance < 10.0f)
//        {
//            m_activeBoundsPoint = i;
//            return;
//        }
//    }
    
    // Convert the image-space position into ROM-die-space using the homography
    cv::Mat pointMat = cv::Mat(3, 1, CV_64F);
    pointMat.at<double>(0, 0) = position.x();
    pointMat.at<double>(1, 0) = position.y();
    pointMat.at<double>(2, 0) = 1.0;
    
    // Compute the 2d point
    const cv::Mat originPointCv = m_romRegionHomography * pointMat;
    const cv::Mat originPointNormalizedCv = originPointCv / originPointCv.at<double>(0,2);
    const QPointF romDiePoint(originPointNormalizedCv.at<double>(0,0), originPointNormalizedCv.at<double>(0,1));
    m_horizSlices.push_back(romDiePoint.x());

    recomputeLinesFromHomography();
    
    m_drawWidget.update();    
}


void MainWindow::keyPressEvent(QKeyEvent* event)
{
    bool ctrlHeld = event->modifiers() & Qt::ControlModifier;
    
    // TODO: Make these menu options soon
    if (ctrlHeld && event->key() == Qt::Key_O)
    {
        QString filename = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Images (*.png *.jpg *.tif)"));
        if (filename != "")
            loadImage(filename);
    }
    else if (ctrlHeld && event->key() == Qt::Key_I)
    {
        QString filename = QFileDialog::getOpenFileName(this, tr("Open Die Description File"), "", tr("ddf (*.ddf)"));
        if (filename != "")
            loadDescriptionJson(filename);
    }
    else if (ctrlHeld && event->key() == Qt::Key_S)
    {
        QString filename = QFileDialog::getSaveFileName(this, tr("Save Die Description File"), "", tr("ddf (*.ddf)"));
        if (filename != "")
            saveDescriptionJson(filename);
    }
    else if (event->key() == Qt::Key_0)
    {
        m_uiMode = Navigation;
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
    }
    else if (event->key() == Qt::Key_1)
    {
        m_uiMode = BoundsDefine;
        QApplication::setOverrideCursor(Qt::CrossCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        m_lmbClickedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonClicked, this, &MainWindow::addOrMoveBoundsPoint);
        m_lmbDraggedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonDragged, this, &MainWindow::dragBoundsPoint);
        m_lmbReleasedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonReleased, this, &MainWindow::stopDraggingBoundsPoint);
    }
    else if (event->key() == Qt::Key_2)
    {
        m_uiMode = SliceDefineHorizontal;
        QApplication::setOverrideCursor(Qt::CrossCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        m_lmbClickedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonClicked, this, &MainWindow::addOrMoveSlice);
        //m_lmbDraggedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonDragged, this, &MainWindow::dragBoundsPoint);
        //m_lmbReleasedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonReleased, this, &MainWindow::stopDraggingBoundsPoint);
    }
    else
    {
        // We're not interested in the keypress?  Pass it up the inheritance chain
        QMainWindow::keyPressEvent(event);
    }
}


void MainWindow::clearGeneratedGeometry()
{
    m_boundsPolygons.clear();
    m_romRegionHomography.release();    
}


void MainWindow::computePolyAndHomography()
{
    clearGeneratedGeometry();
    
    // Sort the points and create a convex polygon from them
    m_boundsPoints = sortedRectanglePoints(m_boundsPoints);
    m_boundsPolygons.push_back(QPolygonF(m_boundsPoints));
    
    // Compute a homography mapping the almost-rectangular ROM region to a rectangle
    std::vector<cv::Point2f> imageSpacePoints;
    imageSpacePoints.push_back(cv::Point2f(m_boundsPoints[0].x(), m_boundsPoints[0].y()));
    imageSpacePoints.push_back(cv::Point2f(m_boundsPoints[1].x(), m_boundsPoints[1].y()));
    imageSpacePoints.push_back(cv::Point2f(m_boundsPoints[2].x(), m_boundsPoints[2].y()));
    imageSpacePoints.push_back(cv::Point2f(m_boundsPoints[3].x(), m_boundsPoints[3].y()));
    
    std::vector<cv::Point2f> romDieSpacePoints;
    romDieSpacePoints.push_back(cv::Point2f(0.0f, 0.0f));
    romDieSpacePoints.push_back(cv::Point2f(1.0f, 0.0f));
    romDieSpacePoints.push_back(cv::Point2f(1.0f, 1.0f));
    romDieSpacePoints.push_back(cv::Point2f(0.0f, 1.0f));
    
    m_romRegionHomography = cv::findHomography(imageSpacePoints, romDieSpacePoints, 0);        
}


void MainWindow::recomputeLinesFromHomography()
{
    m_sliceLines.clear();
    
    for (int i = 0; i < m_horizSlices.size(); i++)
    {
        const qreal& horizSlicePoint = m_horizSlices[i];
        
        // Compute the points for the visible line
        const cv::Mat homographyInverse = m_romRegionHomography.inv();
        cv::Mat topPointMat = cv::Mat(3, 1, CV_64F);
        topPointMat.at<double>(0, 0) = horizSlicePoint;
        topPointMat.at<double>(1, 0) = 0.0;
        topPointMat.at<double>(2, 0) = 1.0;
        const cv::Mat topPointCv = homographyInverse * topPointMat;
        const cv::Mat topPointNormalizedCv = topPointCv / topPointCv.at<double>(0,2);
        
        cv::Mat bottomPointMat = cv::Mat(3, 1, CV_64F);
        bottomPointMat.at<double>(0, 0) = horizSlicePoint;
        bottomPointMat.at<double>(1, 0) = 1.0;
        bottomPointMat.at<double>(2, 0) = 1.0;
        const cv::Mat bottomPointCv = homographyInverse * bottomPointMat;
        const cv::Mat bottomPointNormalizedCv = bottomPointCv / bottomPointCv.at<double>(0,2);
        
        QLineF pb(QPointF(topPointNormalizedCv.at<double>(0,0), topPointNormalizedCv.at<double>(0,1)),
                  QPointF(bottomPointNormalizedCv.at<double>(0,0), bottomPointNormalizedCv.at<double>(0,1)));
        m_sliceLines.push_back(pb);
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

