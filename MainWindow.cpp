#include "MainWindow.h"

#include <QDebug>
#include <QWidget>
#include <QAction>
#include <QKeyEvent>
#include <QJsonArray>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>


MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : m_drawWidget()
{
    m_drawWidget.setFocusPolicy(Qt::ClickFocus);
    setCentralWidget(&m_drawWidget);
    
    // A decent quit keyboard shortcut
    QAction* quitAct = new QAction(this);
    quitAct->setShortcuts(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &MainWindow::close);
    addAction(quitAct);
    
    // Make sure the draw widget is focused when the app loop starts
    m_drawWidget.setFocus();
}


MainWindow::~MainWindow()
{
    
}


bool MainWindow::loadImage(const QString& filename)
{
    QImage foo(filename);
    if (foo.isNull())
    {
        qWarning() << "Error opening image " << filename;
        return false;
    }

    m_drawWidget.setImage(foo, false);
    if (foo.size().width() > m_drawWidget.size().width() ||
        foo.size().height() > m_drawWidget.size().height())
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


void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_O)
    {
        // TEST
        //QString filename = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Images (*.png *.jpg *.tif)"));
        //if (filename != "")
        //    loadImage(filename);
        
        qInfo() << loadDescriptionJson("/tmp/boo.ddf");
        event->accept();
    }
    if (event->key() == Qt::Key_S)
    {
        // TEST
        qInfo() << saveDescriptionJson("/tmp/boo.ddf");
        event->accept();
    }
    else
    {
        event->ignore();
    }

    QMainWindow::keyPressEvent(event);
}

