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
#include <QtAlgorithms>
#include <QJsonDocument>

//
// TODO list
// ---------
//
// * Status bar
// * Menu bar & menu items
// * Convert everything to Qt undo command structure
// * A view to see an enlarged version of the current bit region
// * Drag slice lines
// * A single click adds both a horizontal and vertical slice line
// * Flesh out more ways to paste (paste as an offset of last line, etc)
// * Export all bit regions to new image(s)
// * A range placement option - put start, put end, fill with X between
// * DrawWidget image chunking for clipping potential
// * Wrap my head further around what can be easily cleaned up as visual stuff
// * Convert the inefficient vectors to linked lists where necessary
// * Sort the vectors in various places other than just the bit creator
// * The diameter doesn't scale in the DrawWidget point clipping
//


MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : m_uiMode(Navigation)
    , m_drawWidget()
    , m_qImage()
    , m_activeBoundsPoint(-1)
    , m_boundsPoints()
    , m_boundsPolygons()
    , m_horizSlices()
    , m_vertSlices()
    , m_sliceLines()
    , m_activeSlices()
    , m_bitLocations()
    , m_sliceLineColors()
    , m_copiedSliceOffsets()
    , m_romRegionHomography()
    , m_lmbClickedConnection()
    , m_lmbDraggedConnection()
    , m_lmbReleasedConnection()
    , m_rmbClickedConnection()
    , m_rmbDraggedConnection()
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

    // Write the vertical slice offsets
    QJsonArray vertSliceArray;
    for (int i = 0; i < m_vertSlices.size(); i++)
    {
        vertSliceArray.append(m_vertSlices[i]);
    }
    root["verticalSlices"] = vertSliceArray;
    
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
    m_activeSlices.clear();
    const QJsonArray horizSlices = docObj["horizontalSlices"].toArray();
    for (int i = 0; i < horizSlices.size(); i++)
    {
        m_horizSlices.push_back(horizSlices[i].toDouble());
    }
    
    // Read the vertical slice offsets
    m_vertSlices.clear();
    m_activeSlices.clear();
    const QJsonArray vertSlices = docObj["verticalSlices"].toArray();
    for (int i = 0; i < vertSlices.size(); i++)
    {
        m_vertSlices.push_back(vertSlices[i].toDouble());
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
    
    // Switch the slice we're operating on based on the current ui mode
    QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;

    // Convert the image-space position into ROM-die-space using the homography
    slices.push_back(romDieSpaceFromImagePoint(position, m_uiMode));

    recomputeSliceLinesFromHomography();
    m_drawWidget.update();
}


void MainWindow::selectSlice(const QPointF& position)
{
    // You can only add a slice if there's a bounds poly
    if (m_boundsPolygons.size() == 0)
        return;
    
    // You can only do something by clicking side the bounds poly
    if (!m_boundsPolygons[0].containsPoint(position, Qt::OddEvenFill))
        return;
    
    // Switch the slice we're operating on based on the current ui mode
    QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;
    
    // First check to see if a current slice line is close (you're selecting instead of adding a new)
    for (int i = 0; i < slices.size(); i++)
    {
        // TODO: Scale selection distance based on drawWidget zoom factor
        const qreal distance = linePointDistance(slicePositionToLine(slices[i], m_uiMode), position);
        if (distance < 5.0f)
        {
            // TODO: inefficient
            const int alreadySelected = m_activeSlices.indexOf(i);
            if (alreadySelected != -1)
                m_activeSlices.remove(alreadySelected);
            else
                m_activeSlices.push_back(i);
            
            qSort(m_activeSlices);
            
            recomputeSliceLinesFromHomography();
            m_drawWidget.update();
            return;
        }
    }
}


void MainWindow::selectMoreSlices(const QPointF& position)
{
    // You can only add a slice if there's a bounds poly
    if (m_boundsPolygons.size() == 0)
        return;
    
    // You can only do something by clicking side the bounds poly
    if (!m_boundsPolygons[0].containsPoint(position, Qt::OddEvenFill))
        return;
    
    // Switch the slice we're operating on based on the current ui mode
    QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;
    
    // First check to see if a current slice line is close (you're selecting instead of adding a new)
    for (int i = 0; i < slices.size(); i++)
    {
        // TODO: Scale selection distance based on drawWidget zoom factor
        const qreal distance = linePointDistance(slicePositionToLine(slices[i], m_uiMode), position);
        if (distance < 5.0f)
        {
            // TODO: inefficient
            if (!m_activeSlices.contains(i))
                m_activeSlices.push_back(i);
            else
                return;
            
            qSort(m_activeSlices);
            
            recomputeSliceLinesFromHomography();
            m_drawWidget.update();
            return;
        }
    }
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
    else if (ctrlHeld && event->key() == Qt::Key_E)
    {
        // Save all bit locations as a condensend image
        
    }
    else if (ctrlHeld && event->key() == Qt::Key_D)
    {
        // Deselect all slices
        m_activeSlices.clear();
        recomputeSliceLinesFromHomography();
        m_drawWidget.update();
    }
    else if (ctrlHeld && event->key() == Qt::Key_B)
    {
        // Copy selected slice offsets
        // TODO: Give me back my CTRL+C, you mean ole' GUI widget!  (this will be fixed when everything is menu-ized)
        m_copiedSliceOffsets.clear();
        QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;
        for (int i = 0; i < m_activeSlices.size(); i++)
        {
            const int& asli = m_activeSlices[i];
            const qreal offset = slices[asli] - slices[asli-1];
            m_copiedSliceOffsets.push_back(offset);
        }
    }
    else if (ctrlHeld && event->key() == Qt::Key_V)
    {
        // Paste selected slice offsets right where the mouse is
        QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;
        const QPointF mouseImagePosition = m_drawWidget.window2Image(m_drawWidget.mapFromGlobal(QCursor::pos()));
        const qreal pushOffset = romDieSpaceFromImagePoint(mouseImagePosition, m_uiMode);
        
        qreal runningSum = pushOffset;
        for (int i = 0; i < m_copiedSliceOffsets.size(); i++)
        {
            // The first slice has no offset with this method of pasting (it appears right where the mouse is)
            const qreal offset = (i == 0) ? 0.0 : m_copiedSliceOffsets[i];
            
            if (runningSum + offset >= 1.0)
                continue;
            
            slices.push_back(runningSum + offset);
            runningSum += offset;
        }
        recomputeSliceLinesFromHomography();
        m_drawWidget.update();
    }
    else if (event->key() == Qt::Key_0)
    {
        m_uiMode = Navigation;
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        m_drawWidget.setConvexPolyPointer(&m_boundsPolygons);
        m_drawWidget.setCircleCoordsPointer(&m_boundsPoints);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        disconnect(m_rmbClickedConnection);
        disconnect(m_rmbDraggedConnection);
        m_drawWidget.update();
    }
    else if (event->key() == Qt::Key_1)
    {
        m_uiMode = BoundsDefine;
        QApplication::setOverrideCursor(Qt::CrossCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        disconnect(m_rmbClickedConnection);
        disconnect(m_rmbDraggedConnection);
        m_drawWidget.setConvexPolyPointer(&m_boundsPolygons);
        m_drawWidget.setCircleCoordsPointer(&m_boundsPoints);
        m_lmbClickedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonClicked, this, &MainWindow::addOrMoveBoundsPoint);
        m_lmbDraggedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonDragged, this, &MainWindow::dragBoundsPoint);
        m_lmbReleasedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonReleased, this, &MainWindow::stopDraggingBoundsPoint);
        m_drawWidget.update();
    }
    else if (event->key() == Qt::Key_2)
    {
        m_uiMode = SliceDefineHorizontal;
        QApplication::setOverrideCursor(Qt::CrossCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        disconnect(m_rmbClickedConnection);
        disconnect(m_rmbDraggedConnection);
        m_drawWidget.setConvexPolyPointer(&m_boundsPolygons);
        m_drawWidget.setCircleCoordsPointer(&m_boundsPoints);
        m_lmbClickedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonClicked, this, &MainWindow::addOrMoveSlice);
        m_rmbClickedConnection = connect(&m_drawWidget, &DrawWidget::rightButtonClicked, this, &MainWindow::selectSlice);
        m_rmbDraggedConnection = connect(&m_drawWidget, &DrawWidget::rightButtonDragged, this, &MainWindow::selectMoreSlices);
        m_activeSlices.clear();
        recomputeSliceLinesFromHomography();
        m_drawWidget.update();
    }
    else if (event->key() == Qt::Key_3)
    {
        m_uiMode = SliceDefineVertical;
        QApplication::setOverrideCursor(Qt::CrossCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        disconnect(m_rmbClickedConnection);
        disconnect(m_rmbDraggedConnection);
        m_drawWidget.setConvexPolyPointer(&m_boundsPolygons);
        m_drawWidget.setCircleCoordsPointer(&m_boundsPoints);
        m_lmbClickedConnection = connect(&m_drawWidget, &DrawWidget::leftButtonClicked, this, &MainWindow::addOrMoveSlice);
        m_rmbClickedConnection = connect(&m_drawWidget, &DrawWidget::rightButtonClicked, this, &MainWindow::selectSlice);
        m_rmbDraggedConnection = connect(&m_drawWidget, &DrawWidget::rightButtonDragged, this, &MainWindow::selectMoreSlices);
        m_activeSlices.clear();
        recomputeSliceLinesFromHomography();
        m_drawWidget.update();
    }
    else if (event->key() == Qt::Key_4)
    {
        m_uiMode = BitRegionDisplay;
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        disconnect(m_lmbClickedConnection);
        disconnect(m_lmbDraggedConnection);
        disconnect(m_lmbReleasedConnection);
        disconnect(m_rmbClickedConnection);
        
        m_sliceLines.clear();
        m_sliceLineColors.clear();
        m_drawWidget.setConvexPolyPointer(NULL);
        m_drawWidget.setCircleCoordsPointer(&m_bitLocations);
        m_bitLocations = computeBitLocations();
        
        m_drawWidget.update();
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
    QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;
    
    QVector<qreal> newSlices;
    for (int i = 0; i < slices.size(); i++)
    {
        if (!m_activeSlices.contains(i))
        {
            newSlices.push_back(slices[i]);
        }
    }
    slices = newSlices;
    m_activeSlices.clear();
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
    
    if (m_uiMode == SliceDefineHorizontal)
    {
        for (int i = 0; i < m_horizSlices.size(); i++)
        {
            // Compute the points for the visible line
            QLineF pb = slicePositionToLine(m_horizSlices[i], SliceDefineHorizontal);
            m_sliceLines.push_back(pb);
    
            if (m_activeSlices.contains(i))
                m_sliceLineColors.push_back(QColor(255, 255, 0));
            else
                m_sliceLineColors.push_back(QColor(0, 0, 255));
        }
    }

    if (m_uiMode == SliceDefineVertical)
    {
        for (int i = 0; i < m_vertSlices.size(); i++)
        {
            // Compute the points for the visible line
            QLineF pb = slicePositionToLine(m_vertSlices[i], SliceDefineVertical);
            m_sliceLines.push_back(pb);
    
            if (m_activeSlices.contains(i))
                m_sliceLineColors.push_back(QColor(255, 255, 0));
            else
                m_sliceLineColors.push_back(QColor(0, 0, 255));
        }
    }
}


QVector<QPointF> MainWindow::computeBitLocations()
{
    // Sort the vectors
    qSort(m_horizSlices);
    qSort(m_vertSlices);
    
    // Returns a list of image-space points representing where the bits are
    // These are created in standard image scanline-order (top=[0,0], left->right)
    QVector<QPointF> results;

    // Top line of bits
    results.push_back(m_boundsPoints[0]);
    for (int x = 0; x < m_horizSlices.size(); x++)
    {
        const QLineF hLine = slicePositionToLine(m_horizSlices[x], SliceDefineHorizontal);
        results.push_back(hLine.p1());
    }
    results.push_back(m_boundsPoints[1]);

    // The bulk of the bits
    for (int y = 0; y < m_vertSlices.size(); y++)
    {
        const QLineF vLine = slicePositionToLine(m_vertSlices[y], SliceDefineVertical);
        for (int x = -1; x <= m_horizSlices.size(); x++)
        {
            const QLineF hLine = slicePositionToLine(m_horizSlices[x], SliceDefineHorizontal);
            
            QPointF pusher;
            if (x == -1)
            {
                pusher = vLine.p1();
            }
            else if (x == m_horizSlices.size())
            {
                pusher = vLine.p2();
            }
            else
            {
                hLine.intersect(vLine, &pusher);
            }
            
            results.push_back(pusher);
        }
    }
    
    // Bottom line of bits
    results.push_back(m_boundsPoints[3]);
    for (int x = 0; x < m_horizSlices.size(); x++)
    {
        const QLineF hLine = slicePositionToLine(m_horizSlices[x], SliceDefineHorizontal);
        results.push_back(hLine.p2());
    }
    results.push_back(m_boundsPoints[2]);
    
    return results;
}


qreal MainWindow::romDieSpaceFromImagePoint(const QPointF& iPoint, const UiMode& hv)
{
    cv::Mat pointMat = cv::Mat(3, 1, CV_64F);
    pointMat.at<double>(0, 0) = iPoint.x();
    pointMat.at<double>(1, 0) = iPoint.y();
    pointMat.at<double>(2, 0) = 1.0;
    
    // Compute image point to ROM die space
    const cv::Mat originPointCv = m_romRegionHomography * pointMat;
    const cv::Mat originPointNormalizedCv = originPointCv / originPointCv.at<double>(0,2);
    const QPointF romDiePoint(originPointNormalizedCv.at<double>(0,0), originPointNormalizedCv.at<double>(0,1));
    const qreal pushPoint = (hv == SliceDefineHorizontal) ? romDiePoint.x() : romDiePoint.y();
    return pushPoint;
}


QLineF MainWindow::slicePositionToLine(const qreal& slicePosition, const UiMode& hv)
{
    // Get the image space position of one extreme of the slice
    const cv::Mat homographyInverse = m_romRegionHomography.inv();
    cv::Mat topPointMat = cv::Mat(3, 1, CV_64F);
    topPointMat.at<double>(0, 0) = (hv == SliceDefineHorizontal) ? slicePosition : 0.0;
    topPointMat.at<double>(1, 0) = (hv == SliceDefineVertical) ? slicePosition : 0.0;
    topPointMat.at<double>(2, 0) = 1.0;
    const cv::Mat topPointCv = homographyInverse * topPointMat;
    const cv::Mat topPointNormalizedCv = topPointCv / topPointCv.at<double>(0,2);
    
    // Get the image space position of the other extreme of the slice
    cv::Mat bottomPointMat = cv::Mat(3, 1, CV_64F);
    bottomPointMat.at<double>(0, 0) = (hv == SliceDefineHorizontal) ? slicePosition : 1.0;
    bottomPointMat.at<double>(1, 0) = (hv == SliceDefineVertical) ? slicePosition : 1.0;
    bottomPointMat.at<double>(2, 0) = 1.0;
    const cv::Mat bottomPointCv = homographyInverse * bottomPointMat;
    const cv::Mat bottomPointNormalizedCv = bottomPointCv / bottomPointCv.at<double>(0,2);
    
    // Construct and return a QLineF from the results
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


