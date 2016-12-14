#include "MainWindow.h"

#include <QDebug>
#include <QWidget>
#include <QAction>
#include <QVector2D>
#include <QVector3D>
#include <QKeyEvent>
#include <QJsonArray>
#include <QFileDialog>
#include <QJsonObject>
#include <QApplication>
#include <QJsonDocument>

//
// TODO list
//
// * Convert everything to Qt undo command structure
// * Come up with a better way to paste (interactively, likely)
// * Select multiple slice lines with a drag
// * A single click adds both a horizontal and vertical slice line
// * Vertical slices
// * Sorting with copy/paste (instead of taking offset order)
// * Drag slice lines
// * Menu bar & menu items
// * Status bar
// 

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : m_uiMode(Navigation)
    , m_drawWidget()
    , m_qImage()
    , m_activeBoundsPoint(-1)
    , m_boundsPoints()
    , m_boundsPolygons()
    , m_horizSlices()
    , m_sliceLines()
    , m_activeHorizSlices()
    , m_sliceLineColors()
    , m_copiedSliceOffsets()
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
    m_drawWidget.setLineColorsPointer(&m_sliceLineColors);
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
    clearBoundsGeometry();
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
    
    // Write the ROM boundary points
    QJsonArray boundaryArray;
    for (int i = 0; i < m_boundsPoints.size(); i++)
    {
        QJsonArray qPointFJson;
        qPointFJson.append(m_boundsPoints[i].x());
        qPointFJson.append(m_boundsPoints[i].y());
        boundaryArray.append(qPointFJson);
    }
    root["romBounds"] = boundaryArray;
    
    // Write the horizontal slice offsets
    QJsonArray horizSliceArray;
    for (int i = 0; i < m_horizSlices.size(); i++)
    {
        horizSliceArray.append(m_horizSlices[i]);
    }
    root["horizontalSlices"] = horizSliceArray;
    
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
    
    // Read the horizontal slice offsets
    m_horizSlices.clear();
    m_activeHorizSlices.clear();
    const QJsonArray horizSlices = docObj["horizontalSlices"].toArray();
    for (int i = 0; i < horizSlices.size(); i++)
    {
        m_horizSlices.push_back(horizSlices[i].toDouble());
    }
    
    // Compute the geometry, and update the view
    computeBoundsPolyAndHomography();
    recomputeSliceLinesFromHomography();
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
            computeBoundsPolyAndHomography();
            recomputeSliceLinesFromHomography();
        }

        m_drawWidget.update();
    }
}


void MainWindow::dragBoundsPoint(const QPointF& position)
{
    if (m_activeBoundsPoint < 0)
        return;
    
    m_boundsPoints[m_activeBoundsPoint] = position;
    if (m_boundsPoints.size() == 4)
    {
        computeBoundsPolyAndHomography();
        recomputeSliceLinesFromHomography();
    }
    
    m_drawWidget.update();
}


void MainWindow::stopDraggingBoundsPoint(const QPointF& position)
{
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
    
    // First check to see if a current slice line is close (you're selecting instead of adding a new)
    for (int i = 0; i < m_horizSlices.size(); i++)
    {
        // TODO: Scale selection distance based on drawWidget zoom factor
        const qreal distance = linePointDistance(slicePositionToLine(m_horizSlices[i]), position);
        if (distance < 5.0f)
        {
            // TODO: inefficient
            const int alreadySelected = m_activeHorizSlices.indexOf(i);
            if (alreadySelected != -1)
                m_activeHorizSlices.remove(alreadySelected);
            else
                m_activeHorizSlices.push_back(i);
            recomputeSliceLinesFromHomography();
            m_drawWidget.update();
            return;
        }
    }

    // If you did't select anything, you must want to clear the selection
    m_activeHorizSlices.clear();
    
    // Convert the image-space position into ROM-die-space using the homography
    cv::Mat pointMat = cv::Mat(3, 1, CV_64F);
    pointMat.at<double>(0, 0) = position.x();
    pointMat.at<double>(1, 0) = position.y();
    pointMat.at<double>(2, 0) = 1.0;
    
    // Compute image point to ROM die space
    const cv::Mat originPointCv = m_romRegionHomography * pointMat;
    const cv::Mat originPointNormalizedCv = originPointCv / originPointCv.at<double>(0,2);
    const QPointF romDiePoint(originPointNormalizedCv.at<double>(0,0), originPointNormalizedCv.at<double>(0,1));
    m_horizSlices.push_back(romDiePoint.x());

    recomputeSliceLinesFromHomography();
    
    m_drawWidget.update();
}


void MainWindow::keyPressEvent(QKeyEvent* event)
{
    bool ctrlHeld = event->modifiers() & Qt::ControlModifier;
    
    // TODO: Make these menu options soon
    if (ctrlHeld && event->key() == Qt::Key_O)
    {
        // Load Image
        QString filename = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Images (*.png *.jpg *.tif)"));
        if (filename != "")
            loadImage(filename);
    }
    else if (ctrlHeld && event->key() == Qt::Key_I)
    {
        // Load DDF
        QString filename = QFileDialog::getOpenFileName(this, tr("Open Die Description File"), "", tr("ddf (*.ddf)"));
        if (filename != "")
            loadDescriptionJson(filename);
    }
    else if (ctrlHeld && event->key() == Qt::Key_S)
    {
        // Save DDF
        QString filename = QFileDialog::getSaveFileName(this, tr("Save Die Description File"), "", tr("ddf (*.ddf)"));
        if (filename != "")
            saveDescriptionJson(filename);
    }
    else if (ctrlHeld && event->key() == Qt::Key_B)
    {
        // TODO: Give me back my CTRL+C, you mean ole' GUI widget!
        // Copy selected slice offsets
        m_copiedSliceOffsets.clear();
        for (int i = 0; i < m_activeHorizSlices.size(); i++)
        {
            const int& asli = m_activeHorizSlices[i];
            const qreal offset = m_horizSlices[asli] - m_horizSlices[asli-1];
            m_copiedSliceOffsets.push_back(offset);
        }
    }
    else if (ctrlHeld && event->key() == Qt::Key_V)
    {
        // Paste selected slice offsets after the last slice
        for (int i = 0; i < m_copiedSliceOffsets.size(); i++)
        {
            const qreal& lastSlicePosition = m_horizSlices.back();
            m_horizSlices.push_back(lastSlicePosition + m_copiedSliceOffsets[i]);
        }
        recomputeSliceLinesFromHomography();
        m_drawWidget.update();
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
    else if (event->key() == Qt::Key_Delete)
    {
        deleteSelectedSlices();
        m_drawWidget.update();
    }
    else
    {
        // We're not interested in the keypress?  Pass it up the inheritance chain
        QMainWindow::keyPressEvent(event);
    }
}


void MainWindow::deleteSelectedSlices()
{
    // TODO: SUUUUUPER-inefficient
    QVector<qreal> newHorizSlices;
    for (int i = 0; i < m_horizSlices.size(); i++)
    {
        if (!m_activeHorizSlices.contains(i))
        {
            newHorizSlices.push_back(m_horizSlices[i]);
        }
    }
    m_horizSlices = newHorizSlices;
    m_activeHorizSlices.clear();
    recomputeSliceLinesFromHomography();
}


void MainWindow::clearBoundsGeometry()
{
    m_boundsPolygons.clear();
    m_romRegionHomography.release();
}


void MainWindow::computeBoundsPolyAndHomography()
{
    clearBoundsGeometry();
    
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


void MainWindow::recomputeSliceLinesFromHomography()
{
    m_sliceLines.clear();
    m_sliceLineColors.clear();
    
    for (int i = 0; i < m_horizSlices.size(); i++)
    {
        const qreal& horizSlicePoint = m_horizSlices[i];
        
        // Compute the points for the visible line
        QLineF pb = slicePositionToLine(horizSlicePoint);
        m_sliceLines.push_back(pb);
        if (m_activeHorizSlices.contains(i)) // TODO: Inefficient
            m_sliceLineColors.push_back(QColor(255, 255, 0));
        else
            m_sliceLineColors.push_back(QColor(0, 0, 255));
    }
}


QLineF MainWindow::slicePositionToLine(const qreal& slicePosition)
{
    const cv::Mat homographyInverse = m_romRegionHomography.inv();
    cv::Mat topPointMat = cv::Mat(3, 1, CV_64F);
    topPointMat.at<double>(0, 0) = slicePosition;
    topPointMat.at<double>(1, 0) = 0.0;
    topPointMat.at<double>(2, 0) = 1.0;
    const cv::Mat topPointCv = homographyInverse * topPointMat;
    const cv::Mat topPointNormalizedCv = topPointCv / topPointCv.at<double>(0,2);
    
    cv::Mat bottomPointMat = cv::Mat(3, 1, CV_64F);
    bottomPointMat.at<double>(0, 0) = slicePosition;
    bottomPointMat.at<double>(1, 0) = 1.0;
    bottomPointMat.at<double>(2, 0) = 1.0;
    const cv::Mat bottomPointCv = homographyInverse * bottomPointMat;
    const cv::Mat bottomPointNormalizedCv = bottomPointCv / bottomPointCv.at<double>(0,2);
    
    QLineF pb(QPointF(topPointNormalizedCv.at<double>(0,0), topPointNormalizedCv.at<double>(0,1)),
              QPointF(bottomPointNormalizedCv.at<double>(0,0), bottomPointNormalizedCv.at<double>(0,1)));
    
    return pb;
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


qreal MainWindow::linePointDistance(const QLineF& line, const QPointF& point)
{
    const qreal A = point.x() - line.p1().x();
    const qreal B = point.y() - line.p1().y();
    const qreal C = line.p2().x() - line.p1().x();
    const qreal D = line.p2().y() - line.p1().y();
    
    const qreal dot = A * C + B * D;
    const qreal len_sq = C * C + D * D;
    const qreal param = dot / len_sq;
    
    QPointF result;
    if(param < 0)
    {
        result = line.p1();
    }
    else if(param > 1)
    {
        result = line.p2();
    }
    else
    {
        result.setX(line.p1().x() + param * C);
        result.setY(line.p1().y() + param * D);
    }
     
    const qreal dist = (point - result).manhattanLength();
    return dist;
}


