#include "MainWindow.h"

#include <QDebug>
#include <QWidget>
#include <QAction>
#include <QMenuBar>
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
// * Convert everything to Qt undo command structure
// * A view to see an enlarged version of the current bit region
// * Drag slice lines
// * A single click adds both a horizontal and vertical slice line
// * Flesh out more ways to paste (paste as an offset of last line, etc)
// * A range placement option - put start, put end, fill with X between
// * Export all bit regions to new image(s)
// * DrawWidget image chunking for clipping potential
// * Wrap my head further around what can be easily cleaned up as visual stuff
// * Convert the inefficient vectors to linked lists where necessary
// * Sort the vectors in various places other than just the bit creator
// * The diameter doesn't scale in the DrawWidget point clipping, nor do the line widths, etc.
//


MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : m_uiMode(Navigation)
    , m_drawWidget()
    , m_qImage()
    , m_dieDescriptionFilename("")
    , m_activeBoundsPoint(-1)
    , m_boundsPoints()
    , m_boundsPolygons()
    , m_horizSlices()
    , m_vertSlices()
    , m_sliceLines()
    , m_activeSlices()
    , m_bitLocations()
    , m_bitLocationTree()
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
    
    // Make sure the draw widget is focused when the app loop starts
    m_drawWidget.setFocus();

    // Register our local data with the pointers in the drawImage
    m_drawWidget.setImagePointer(&m_qImage);
    m_drawWidget.setCircleCoordsPointer(&m_boundsPoints);
    m_drawWidget.setConvexPolyPointer(&m_boundsPolygons);
    m_drawWidget.setLinesPointer(&m_sliceLines);
    m_drawWidget.setLineColorsPointer(&m_sliceLineColors);
    
    createMenu();
}


MainWindow::~MainWindow()
{
    
}


void MainWindow::createMenu()
{
    // Create the file menu actions and menu item
    QAction* openImageAct = new QAction(tr("Open Die &Image"), this);
    openImageAct->setShortcuts(QKeySequence::Open);
    openImageAct->setStatusTip(tr("Open a new die image"));
    connect(openImageAct, &QAction::triggered, this, &MainWindow::openImage);
    
    QAction* openDDFAct = new QAction(tr("Open Die &Description JSON"), this);
    openDDFAct->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_O));
    openDDFAct->setStatusTip(tr("Open a new die description JSON"));
    connect(openDDFAct, &QAction::triggered, this, &MainWindow::openDieDescription);

    QAction* saveDDFAct = new QAction(tr("&Save Die Description JSON"), this);
    saveDDFAct->setShortcut(QKeySequence(QKeySequence::Save));
    saveDDFAct->setStatusTip(tr("Save the current die description JSON"));
    connect(saveDDFAct, &QAction::triggered, this, &MainWindow::saveDieDescription);

    QAction* exportBitImageAct = new QAction(tr("&Export Bit PNG"), this);
    exportBitImageAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
    exportBitImageAct->setStatusTip(tr("Export the marked bits to a special bit image PNG"));
    connect(exportBitImageAct, &QAction::triggered, this, &MainWindow::exportBitImage);
    
    QAction* quitAct = new QAction(tr("E&xit"), this);
    quitAct->setShortcuts(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &MainWindow::close);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openImageAct);
    fileMenu->addAction(openDDFAct);
    fileMenu->addAction(saveDDFAct);
    fileMenu->addAction(exportBitImageAct);
    fileMenu->addAction(quitAct);


    // Create the edit menu actions and menu item
    QAction* copySlicesAct = new QAction(tr("&Copy slices"), this);
    copySlicesAct->setShortcut(QKeySequence(QKeySequence::Copy));
    copySlicesAct->setStatusTip(tr("Copy slices"));
    connect(copySlicesAct, &QAction::triggered, this, &MainWindow::copySlices);

    QAction* pasteSlicesAct = new QAction(tr("&Paste slices"), this);
    pasteSlicesAct->setShortcut(QKeySequence(QKeySequence::Paste));
    pasteSlicesAct->setStatusTip(tr("Paste slices"));
    connect(pasteSlicesAct, &QAction::triggered, this, &MainWindow::pasteSlices);
    
    QAction* deselectSlicesAct = new QAction(tr("&Deselect slices"), this);
    deselectSlicesAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
    deselectSlicesAct->setStatusTip(tr("Deselect selected slices"));
    connect(deselectSlicesAct, &QAction::triggered, this, &MainWindow::deselectSlices);
    
    QAction* deleteSlicesAct = new QAction(tr("&Delete slices"), this);
    deleteSlicesAct->setShortcut(QKeySequence(Qt::Key_Delete));
    deleteSlicesAct->setStatusTip(tr("Delete selected slices"));
    connect(deleteSlicesAct, &QAction::triggered, this, &MainWindow::deleteSlices);
    
    QAction* testAct = new QAction(tr("&Test operation"), this);
    testAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_T));
    testAct->setStatusTip(tr("Test!"));
    connect(testAct, &QAction::triggered, this, &MainWindow::testOperation);
    
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(copySlicesAct);
    editMenu->addAction(pasteSlicesAct);
    editMenu->addAction(deselectSlicesAct);
    editMenu->addAction(deleteSlicesAct);
    editMenu->addAction(testAct);
    
    
    // Create the mode menu actions and menu item
    QAction* modeNaviationAct = new QAction(tr("&Naviation mode"), this);
    modeNaviationAct->setShortcut(QKeySequence(Qt::Key_1));
    modeNaviationAct->setStatusTip(tr("Navigate the image and data"));
    connect(modeNaviationAct, &QAction::triggered, this, &MainWindow::setModeNavigation);
    modeNaviationAct->setCheckable(true);
    
    QAction* modeBoundsDefineAct = new QAction(tr("&Bounds define mode"), this);
    modeBoundsDefineAct->setShortcut(QKeySequence(Qt::Key_2));
    modeBoundsDefineAct->setStatusTip(tr("Define ROM die region"));
    connect(modeBoundsDefineAct, &QAction::triggered, this, &MainWindow::setModeBoundsDefine);
    modeBoundsDefineAct->setCheckable(true);
    
    QAction* modeSliceDefineHorizontalAct = new QAction(tr("Slice define &horizontal mode"), this);
    modeSliceDefineHorizontalAct->setShortcut(QKeySequence(Qt::Key_3));
    modeSliceDefineHorizontalAct->setStatusTip(tr("Define horizontal ROM slices"));
    connect(modeSliceDefineHorizontalAct, &QAction::triggered, this, &MainWindow::setModeSliceDefineHorizontal);
    modeSliceDefineHorizontalAct->setCheckable(true);
    
    QAction* modeSliceDefineVerticalAct = new QAction(tr("Slice define &vertical mode"), this);
    modeSliceDefineVerticalAct->setShortcut(QKeySequence(Qt::Key_4));
    modeSliceDefineVerticalAct->setStatusTip(tr("Define vertical ROM slices"));
    connect(modeSliceDefineVerticalAct, &QAction::triggered, this, &MainWindow::setModeSliceDefineVertical);
    modeSliceDefineVerticalAct->setCheckable(true);
    
    QAction* modeBitRegionDisplayAct = new QAction(tr("&Bit region display mode"), this);
    modeBitRegionDisplayAct->setShortcut(QKeySequence(Qt::Key_5));
    modeBitRegionDisplayAct->setStatusTip(tr("Show bit regions"));
    connect(modeBitRegionDisplayAct, &QAction::triggered, this, &MainWindow::setModeBitRegionDisplay);
    modeBitRegionDisplayAct->setCheckable(true);

    QActionGroup* modeGroup = new QActionGroup(this);
    modeGroup->addAction(modeNaviationAct);
    modeGroup->addAction(modeBoundsDefineAct);
    modeGroup->addAction(modeSliceDefineHorizontalAct);
    modeGroup->addAction(modeSliceDefineVerticalAct);
    modeGroup->addAction(modeBitRegionDisplayAct);
    modeNaviationAct->setChecked(true);
    
    QMenu* modeMenu = menuBar()->addMenu(tr("&Mode"));
    modeMenu->addAction(modeNaviationAct);
    modeMenu->addAction(modeBoundsDefineAct);
    modeMenu->addAction(modeSliceDefineHorizontalAct);
    modeMenu->addAction(modeSliceDefineVerticalAct);
    modeMenu->addAction(modeBitRegionDisplayAct);
    
    
    // Create the view menu actions and menu item
    QAction* centerImageAct = new QAction(tr("&Center Image"), this);
    centerImageAct->setShortcut(QKeySequence(Qt::Key_C));
    centerImageAct->setStatusTip(tr("Center image"));
    connect(centerImageAct, &QAction::triggered, &m_drawWidget, &DrawWidget::centerImage);

    QAction* frameImageAct = new QAction(tr("&Frame Image"), this);
    frameImageAct->setShortcut(QKeySequence(Qt::Key_F));
    frameImageAct->setStatusTip(tr("Frame image"));
    connect(frameImageAct, &QAction::triggered, &m_drawWidget, &DrawWidget::frameImage);

    QAction* resetImageAct = new QAction(tr("&Reset Image Display"), this);
    resetImageAct->setShortcut(QKeySequence(Qt::Key_Z));
    resetImageAct->setStatusTip(tr("Reset image display"));
    connect(resetImageAct, &QAction::triggered, &m_drawWidget, &DrawWidget::resetImage);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(centerImageAct);
    viewMenu->addAction(frameImageAct);
    viewMenu->addAction(resetImageAct);
}


void MainWindow::openImage()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Die Image"), "", tr("Images (*.png *.jpg *.tif)"));
    if (filename != "")
        loadImage(filename);
}


void MainWindow::openDieDescription()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Die Description File"), "", tr("ddf (*.ddf)"));
    if (filename != "")
        loadDescriptionJson(filename);
}


void MainWindow::saveDieDescription()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Die Description File"), m_dieDescriptionFilename, tr("ddf (*.ddf)"));
    if (filename != "")
        saveDescriptionJson(filename);
}


void MainWindow::exportBitImage()
{
    // Save all bit locations as a condensend image
    if (m_uiMode == BitRegionDisplay)
    {
        exportBitsToImage("/tmp/test.png");
    }
    else
    {
        qWarning() << "Switch to bit display mode to export";
    }
}


void MainWindow::copySlices()
{
    // Copy selected slice offsets
    m_copiedSliceOffsets.clear();
    QVector<qreal>& slices = (m_uiMode == SliceDefineHorizontal) ? m_horizSlices : m_vertSlices;
    for (int i = 0; i < m_activeSlices.size(); i++)
    {
        const int& asli = m_activeSlices[i];
        const qreal offset = slices[asli] - slices[asli-1];
        m_copiedSliceOffsets.push_back(offset);
    }
}


void MainWindow::pasteSlices()
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


void MainWindow::deselectSlices()
{
    // Deselect all slices
    m_activeSlices.clear();
    recomputeSliceLinesFromHomography();
    m_drawWidget.update();
}


void MainWindow::deleteSlices()
{
    deleteSelectedSlices();
    m_drawWidget.update();
}


void MainWindow::testOperation()
{
    qWarning() << "Executing test operation";
    
    // TEST for the point kdtree
    if (m_bitLocationTree.dims() <= 0)
        return;
    const QPointF mouseImagePosition = m_drawWidget.window2Image(m_drawWidget.mapFromGlobal(QCursor::pos()));
    cv::Mat mipVec(1, 2, CV_32F);
    mipVec.at<float>(0, 0) = mouseImagePosition.x();
    mipVec.at<float>(0, 1) = mouseImagePosition.y();
    
    const int K = 1;
    const int eMax = 1000000000;
    cv::Mat idx(K, 1, CV_32S);
    cv::Mat dist(K, 2, CV_32F);
    m_bitLocationTree.findNearest(mipVec, K, eMax, idx, cv::noArray(), dist);
    qDebug() << idx.at<int>(0, 0);
    //qDebug() << dist[0] << " " << dist[1] << " " << dist[2];
}


void MainWindow::setModeNavigation()
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
    recomputeSliceLinesFromHomography();    
    m_drawWidget.update();
}


void MainWindow::setModeBoundsDefine()
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


void MainWindow::setModeSliceDefineHorizontal()
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


void MainWindow::setModeSliceDefineVertical()
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


void MainWindow::setModeBitRegionDisplay()
{
    m_uiMode = BitRegionDisplay;
    QApplication::setOverrideCursor(Qt::ArrowCursor);
    disconnect(m_lmbClickedConnection);
    disconnect(m_lmbDraggedConnection);
    disconnect(m_lmbReleasedConnection);
    disconnect(m_rmbClickedConnection);
    disconnect(m_rmbDraggedConnection);
    m_sliceLines.clear();
    m_sliceLineColors.clear();
    m_drawWidget.setConvexPolyPointer(NULL);
    m_bitLocations = computeBitLocations();
    m_drawWidget.setCircleCoordsPointer(&m_bitLocations);
    
    m_drawWidget.update();
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
        m_drawWidget.scaleImageToViewport();

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
    
    m_dieDescriptionFilename = filename;
            
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
    
    if (m_uiMode == SliceDefineHorizontal || m_uiMode == Navigation)
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

    if (m_uiMode == SliceDefineVertical || m_uiMode == Navigation)
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


void MainWindow::exportBitsToImage(const QString& filename)
{
    // TODO: Didn't have the time to finish this one
    const int radius = 6;

    QSize resultImageSize((radius*2+1) * m_bitLocations.size(), radius*2);
    QImage result(resultImageSize, QImage::Format_RGB32);

    result.save(filename);
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
    
    // Build the bit location acceleration tree 
    cv::Mat foo(results.size(), 2, CV_32F);
    for (int i = 0; i < results.size(); i++)
    {
        foo.at<float>(i, 0) = results[i].x();
        foo.at<float>(i, 1) = results[i].y();
    }
    m_bitLocationTree.build(foo, false);
    
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


QColor MainWindow::qImageBilinear(const QImage& image, const QPointF& pixelCoord)
{
    // Some constants
    const int w = image.width();
    const int h = image.height();
    const qreal x = pixelCoord.x();
    const qreal y = pixelCoord.y();

    // Get the top and bottom coordinates
    const int x1 = static_cast<int>(floor(x));
    const int y1 = static_cast<int>(floor(y));
    const int x2 = x1 + 1;
    const int y2 = y1 + 1;

    // Boundary conditions
    if (x2 >= w || y2 >= h) return image.pixel(x1, y1);
    if (x1 < 0  || y1 < 0)  return QColor(0, 0, 0);
    
    // Pixel samples
    const QColor ltop = image.pixel(x1, y1);
    const QColor rtop = image.pixel(x1, y2);
    const QColor lbot = image.pixel(x2, y1);
    const QColor rbot = image.pixel(x2, y2);

    // Blerp for great success
    QColor result;
    result.setRed  (((x2 - x) * (y2 - y) * ltop.redF()   + (x2 - x) * (y - y1) * rtop.redF() + 
                     (x - x1) * (y2 - y) * lbot.redF()   + (x - x1) * (y - y1) * rbot.redF()) * 255.0);
    result.setGreen(((x2 - x) * (y2 - y) * ltop.greenF() + (x2 - x) * (y - y1) * rtop.greenF() + 
                     (x - x1) * (y2 - y) * lbot.greenF() + (x - x1) * (y - y1) * rbot.greenF()) * 255.0);
    result.setBlue (((x2 - x) * (y2 - y) * ltop.blueF()  + (x2 - x) * (y - y1) * rtop.blueF() + 
                     (x - x1) * (y2 - y) * lbot.blueF()  + (x - x1) * (y - y1) * rbot.blueF()) * 255.0);
    return result;
}
