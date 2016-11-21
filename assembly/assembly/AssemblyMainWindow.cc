#include <stdlib.h>

#include <string>
#include <iostream>

#include <QGroupBox>
#include <QFileDialog>
#include <QApplication>
#include <QPalette>

#include <nqlogger.h>
#include <ApplicationConfig.h>

#include "AssemblyMainWindow.h"

AssemblyMainWindow::AssemblyMainWindow(QWidget *parent) :
    QMainWindow(parent),
    camera_(0)
{
    ApplicationConfig* config = ApplicationConfig::instance();
    
    
    lStepExpressModel_ = new LStepExpressModel(config->getValue<std::string>("LStepExpressDevice").c_str(),
                                               1000, 100);
    motionManager_ = new LStepExpressMotionManager(lStepExpressModel_);
    motionThread_ = new LStepExpressMotionThread(this);
    motionThread_->start();
    motionManager_->myMoveToThread(motionThread_);
    

    tabWidget_ = new QTabWidget(this);
    tabWidget_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    finderView_ = new AssemblyUEyeSnapShooter(tabWidget_);
    tabWidget_->addTab(finderView_, "finder");

    edgeView_ = new AssemblyUEyeSnapShooter(tabWidget_);
    tabWidget_->addTab(edgeView_, "edges");

    rawView_ = new AssemblyUEyeSnapShooter(tabWidget_);
    tabWidget_->addTab(rawView_, "raw");
    
    uEyeModel_ = new AssemblyUEyeModel_t(10);
    cameraThread_ = new AssemblyUEyeCameraThread(uEyeModel_, this);
    cameraThread_->start();

    assembleView_ = new AssemblyModuleAssembler(uEyeModel_, lStepExpressModel_, motionManager_, conradModel_, tabWidget_);
    tabWidget_->addTab(assembleView_, "assemble");
    
    finder_ = new AssemblySensorMarkerFinder();
    finderThread_ = new AssemblyMarkerFinderThread(finder_, this);
    finderThread_->start();

    finderWidget_ = new AssemblySensorMarkerFinderWidget(finder_, this);
    tabWidget_->addTab(finderWidget_, "finder config");

    uEyeWidget_ = new AssemblyUEyeWidget(uEyeModel_, this);
    tabWidget_->addTab(uEyeWidget_, "uEye");
    
    
    QWidget * widget;
    widget= new QWidget(tabWidget_);
    tabWidget_->addTab(widget, "Motion Manager");

    
    QHBoxLayout * layout = new QHBoxLayout(widget);
    widget->setLayout(layout);
    
    QVBoxLayout * layoutv = new QVBoxLayout(widget);
    
    LStepExpressWidget *lStepExpressWidget = new LStepExpressWidget(lStepExpressModel_, widget);
    layoutv->addWidget(lStepExpressWidget);
    
    LStepExpressJoystickWidget *lStepJoystick = new LStepExpressJoystickWidget(lStepExpressModel_, widget);
    layoutv->addWidget(lStepJoystick);
    
    layout->addLayout(layoutv);
    
    QVBoxLayout * layoutv2 = new QVBoxLayout(widget);
    
    //LStepExpressStatusWindow *lStepStatusWindow = new LStepExpressStatusWindow(lStepExpressModel_, widget);
    //layoutv2->addWidget(lStepStatusWindow);
    
    LStepExpressPositionWidget *lStepPosition = new LStepExpressPositionWidget(motionManager_, lStepExpressModel_, widget);
    layoutv2->addWidget(lStepPosition);
    
    
    

    connect(QApplication::instance(), SIGNAL(aboutToQuit()),
            this, SLOT(quit()));

    toolBar_ = addToolBar("Tools");
    toolBar_->addAction("open", this, SLOT(onOpenCamera()));
    toolBar_->addAction("close", this, SLOT(onCloseCamera()));
    toolBar_->addAction("snapshot", this, SLOT(onSnapShot()));
  
    setCentralWidget(tabWidget_);
    updateGeometry();

    NQLog("AssemblyMainWindow") << "main window constructed";

    liveTimer_ = new QTimer(this);
    connect(liveTimer_, SIGNAL(timeout()), this, SLOT(liveUpdate()));

    QTimer::singleShot(1, uEyeModel_, SLOT(updateInformation()));
}

void AssemblyMainWindow::liveUpdate()
{
    NQLog("AssemblyMainWindow") << ":liveUpdate()";

    emit acquireImage();
}

void AssemblyMainWindow::onOpenCamera()
{
    NQLog("AssemblyMainWindow") << ":onOpenCamera() ";

    camera_ = uEyeModel_->getCameraByID(10);

    connect(this, SIGNAL(openCamera()),
            camera_, SLOT(open()));
    connect(this, SIGNAL(closeCamera()),
            camera_, SLOT(close()));
    connect(this, SIGNAL(acquireImage()),
            camera_, SLOT(acquireImage()));
    connect(camera_, SIGNAL(cameraOpened()),
            this, SLOT(cameraOpened()));
    connect(camera_, SIGNAL(cameraClosed()),
            this, SLOT(cameraClosed()));

    emit openCamera();
}

void AssemblyMainWindow::onCloseCamera()
{
    NQLog("AssemblyMainWindow") << ":onCloseCamera()";

    emit closeCamera();
}

void AssemblyMainWindow::onSnapShot()
{
   NQLog("AssemblyUEyeCamera::onSnapShot", NQLog::Message) << " pre ";
    emit acquireImage();
   NQLog("AssemblyUEyeCamera::onSnapShot", NQLog::Message) << " post ";

}



void AssemblyMainWindow::testTimer()
{
    NQLog("AssemblyMainWindow") << "timeOut = " << testTimerCount_;
    testTimerCount_ += 0.1;
}

void AssemblyMainWindow::cameraOpened()
{
    NQLog("AssemblyMainWindow") << ":cameraOpened()";

    finderView_->connectImageProducer(finder_, SIGNAL(markerFound(const cv::Mat&)));

    edgeView_->connectImageProducer(finder_, SIGNAL(edgesDetected(const cv::Mat&)));

    rawView_->connectImageProducer(camera_, SIGNAL(imageAcquired(const cv::Mat&)));
    

    connect(camera_, SIGNAL(imageAcquired(const cv::Mat&)),
            finder_, SLOT(findMarker(const cv::Mat&)));

    //liveTimer_->start(2000);
}

void AssemblyMainWindow::cameraClosed()
{
    NQLog("AssemblyMainWindow") << ":cameraClosed()";

    finderView_->disconnectImageProducer(finder_, SIGNAL(markerFound(const cv::Mat&)));

    edgeView_->disconnectImageProducer(finder_, SIGNAL(edgesDetected(const cv::Mat&)));

    rawView_->disconnectImageProducer(camera_, SIGNAL(imagef(const cv::Mat&)));

    //    playView_->disconnectImageProducer(camera_, SIGNAL(imageAcquired(const cv::Mat&)));
    disconnect(camera_, SIGNAL(imageAcquired(const cv::Mat&)),
               finder_, SLOT(findMarker(const cv::Mat&)));

    liveTimer_->stop();
}

void AssemblyMainWindow::quit()
{
    NQLog("AssemblyMainWindow") << "quit";

    if (camera_) {
        emit closeCamera();
        camera_ = 0;
    }

    if (cameraThread_) {
        if (cameraThread_->wait(2000) == false)
            cameraThread_->terminate();
        cameraThread_->quit();
        //delete cameraThread_;
    }
}
