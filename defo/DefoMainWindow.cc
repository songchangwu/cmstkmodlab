
#include "DefoMainWindow.h"



// contains DefoMainWindow::setupUi and ::retranslateUi
#include "defoMainWindowObjSettings.inc.cc"

///
///
///
DefoMainWindow::DefoMainWindow( QWidget* parent ) : QWidget( parent ) {

  setupUi();
  setupSignalsAndSlots();

  DefoConfigReader cfgReader( "defo.cfg" );
  debugLevel_ = cfgReader.getValue<unsigned int>( "DEBUG_LEVEL" ); // only parameter directly needed by this class

  // supply combo boxes with allowed values;
  // must be done before cfgReading!
  fillComboBoxes();

  // update cfg settings
  seedingThresholdsStep1Spinbox_->setValue( cfgReader.getValue<double>( "STEP1_THRESHOLD" ) );
  seedingThresholdsStep2Spinbox_->setValue( cfgReader.getValue<double>( "STEP2_THRESHOLD" ) );
  seedingThresholdsStep3Spinbox_->setValue( cfgReader.getValue<double>( "STEP3_THRESHOLD" ) );
  blueishnessSpinBox_->setValue( cfgReader.getValue<double>( "BLUEISHNESS_THRESHOLD" ) );
  hswSpinBox_->setValue( cfgReader.getValue<double>( "HALF_SQUARE_WIDTH" ) );
  geometryFSpinbox_->setValue( cfgReader.getValue<double>( "LENS_FOCAL_LENGTH" ) * 1000 ); // mm -> meter conversion!!
  geometryLgSpinbox_->setValue( cfgReader.getValue<double>( "NOMINAL_GRID_DISTANCE" ) * 1000 ); // mm -> meter conversion!!
  geometryLcSpinbox_->setValue( cfgReader.getValue<double>( "NOMINAL_CAMERA_DISTANCE" ) * 1000 ); // mm -> meter conversion!!
  geometryDeltaSpinbox_->setValue( cfgReader.getValue<double>( "NOMINAL_VIEWING_ANGLE" ) );
  geometryPitchSpinbox1_->setValue( cfgReader.getValue<double>( "PIXEL_PITCH_X" ) * 1.e6 ); // m -> micrometer
  geometryPitchSpinbox2_->setValue( cfgReader.getValue<double>( "PIXEL_PITCH_Y" ) * 1.e6); // m -> micrometer
  surfaceRecoSpacingSpinbox_->setValue( cfgReader.getValue<int>( "SPACING_ESTIMATE" ) );
  surfaceRecoSearchpathSpinbox_->setValue( cfgReader.getValue<int>( "SEARCH_PATH_HALF_WIDTH" ) );
  chillerParametersSpinbox1_->setValue( cfgReader.getValue<double>( "CHILLER_PARAMETER_XP" ) );
  chillerParametersSpinbox2_->setValue( cfgReader.getValue<int>( "CHILLER_PARAMETER_TN" ) );
  chillerParametersSpinbox3_->setValue( cfgReader.getValue<int>( "CHILLER_PARAMETER_TV" ) );

  readCameraParametersFromCfgFile(); // this is grouped

  cameraConnectionCheckBox_->setChecked( "true" == cfgReader.getValue<std::string>( "CAMERA_ON_WHEN_START" )?true:false);
  isCameraEnabled_ = cameraConnectionCheckBox_->isChecked();
  cameraEnabledButtonToggled( isCameraEnabled_ );

  conradEnabledCheckbox_->setChecked( "true" == cfgReader.getValue<std::string>( "RELAY_POWER_WHEN_START" )?true:false);
  isConradEnabled_ = conradEnabledCheckbox_->isChecked();
  
  pollingDelay_ = new QTimer();

  isManual_ = false;
  isRefImage_ = false; refCheckBox_->setChecked( false );

  // set default measurement id
  baseFolderName_ = basefolderTextedit_->toPlainText(); // LOAD DEFAULT
  defaultMeasurementId();

  // read light panel startup cfg...
  lightPanelStates_.resize( 5 );
  initLightPanelStates( cfgReader.getValue<std::string>( "PANEL_STATE_WHEN_START" ) );
  
  // .. and set buttons
  for( unsigned int i = 0; i < 5; ++i ) {
    if( lightPanelStates_.at( i ) ) lightPanelsButtons_.at( i )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
    else lightPanelsButtons_.at( i )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
  }
  
}



///
///
///
void DefoMainWindow::setupSignalsAndSlots( void ) {

  // rawimage, areas and indices
  connect( areaNewButton_, SIGNAL(clicked()), rawimageLabel_, SLOT(defineArea()) );
  connect( rawimageLabel_, SIGNAL(areaDefined(DefoArea)), this, SLOT(receiveArea(DefoArea)) );
  connect( areaDeleteButton_, SIGNAL(clicked()), this, SLOT(deleteCurrentArea()) );
  connect( displayAreasButton_, SIGNAL(toggled(bool)), this, SLOT(toggleAreaDisplay(bool)) );
  connect( displayIndicesButton_, SIGNAL(toggled(bool)), this, SLOT(toggleIndicesDisplay(bool)) );
  connect( displayIndicesButton_, SIGNAL(toggled(bool)), this, SLOT(toggleIndicesDisplay(bool)) );
  connect( displayRecoitemButton_, SIGNAL(toggled(bool)), this, SLOT(togglePointSquareDisplay(bool)) );
  connect( this, SIGNAL(imagelabelDisplayAreas(bool)), rawimageLabel_, SLOT(displayAreas(bool)) );
  connect( this, SIGNAL(imagelabelRefreshAreas(std::vector<DefoArea>)), rawimageLabel_, SLOT(refreshAreas(std::vector<DefoArea>)) );
  connect( this, SIGNAL(imagelabelDisplayIndices(bool)), rawimageLabel_, SLOT(displayIndices(bool)) );
  connect( this, SIGNAL(imagelabelRefreshIndices(std::vector<DefoPoint>)), rawimageLabel_, SLOT(refreshIndices(std::vector<DefoPoint>)) );
  connect( this, SIGNAL(imagelabelDisplayPointSquares(bool)), rawimageLabel_, SLOT(displayPointSquares(bool)) );
  connect( this, SIGNAL(imagelabelRefreshPointSquares(std::vector<DefoSquare>)), rawimageLabel_, SLOT(refreshPointSquares(std::vector<DefoSquare>)) );
  connect( refreshCameraButton_, SIGNAL(clicked()), this, SLOT(loadImageFromCamera()) );
  qRegisterMetaType<DefoCamHandler::Action>("DefoCamHandler::Action"); // signal is from qthread
  connect( &camHandler_, SIGNAL(actionFinished(DefoCamHandler::Action)), this, SLOT(handleCameraAction(DefoCamHandler::Action)) );
  connect( refreshFileButton_, SIGNAL(clicked()), this, SLOT(loadImageFromFile()) );
  connect( rawimageHistoButton_, SIGNAL(clicked()), rawimageLabel_, SLOT( showHistogram() ) );
  connect( displayCoordsButton_, SIGNAL(toggled(bool)), rawimageLabel_, SLOT( displayCoords(bool)) );

  // action polling
  connect( this, SIGNAL(pollAction()), schedule_, SLOT(pollAction()) );
  connect( schedule_, SIGNAL(newAction(DefoSchedule::scheduleItem)), this, SLOT( handleAction(DefoSchedule::scheduleItem)) );
  connect( schedule_, SIGNAL(unableToDeliverAction()), this, SLOT(stopPolling()) );
  connect( schedule_, SIGNAL(newRow(int)), scheduleTableview_, SLOT(selectRow(int)) );
  connect( scheduleVerifyButton_, SIGNAL(clicked()), schedule_, SLOT(validate()) );

  // manual action buttons
  connect( manualREFButton_, SIGNAL(clicked()), this, SLOT(manualFileRef()) );
  connect( manualDEFOButton_, SIGNAL(clicked()), this, SLOT(manualFileDefo()) );

  // schedule buttons & misc
  connect( scheduleStartButton_, SIGNAL(clicked()), this, SLOT(startPolling()) );
  connect( schedulePauseButton_, SIGNAL(clicked()), this, SLOT(pausePolling()) );
  connect( scheduleStopButton_, SIGNAL(clicked()), this, SLOT(stopPolling()) );
  connect( scheduleClearButton_, SIGNAL(clicked()), schedule_, SLOT(clear()) );
  connect( scheduleLoadButton_, SIGNAL(clicked()), schedule_, SLOT(loadFromFile()) );
  connect( scheduleSaveButton_, SIGNAL(clicked()), schedule_, SLOT(saveToFile()) );

  // measurement id
  connect( measurementidEditButton_, SIGNAL(clicked()), this, SLOT(editMeasurementId()) );
  connect( measurementidDefaultButton_, SIGNAL(clicked()), this, SLOT(defaultMeasurementId()) );
  connect( quitButton_, SIGNAL(clicked()), qApp, SLOT(quit()));

  // offline display
  connect( displaytype3DButton_, SIGNAL(toggled( bool ) ), surfacePlot_, SLOT( toggleView( bool ) ) );
  connect( displayitemsAxesButton_, SIGNAL( toggled( bool ) ), surfacePlot_, SLOT( setIsAxes( bool ) ) );
  connect( displayitemsMeshButton_, SIGNAL( toggled( bool ) ), surfacePlot_, SLOT( setIsMesh( bool ) ) );
  connect( displayitemsShadeButton_, SIGNAL( toggled( bool ) ), surfacePlot_, SLOT( setIsShade( bool ) ) );
  connect( displayitemsLegendButton_, SIGNAL( toggled( bool ) ), surfacePlot_, SLOT( setIsLegend( bool ) ) );
  connect( displayitemsIsolinesButton_, SIGNAL( toggled( bool ) ), surfacePlot_, SLOT( setIsIsolines( bool ) ) );
  connect( displayitemsIsolinesSpinbox_, SIGNAL( valueChanged( int ) ), surfacePlot_, SLOT( setNIsolines( int ) ) );
  connect( surfacePlot_, SIGNAL( shadeEnabled( bool ) ), displayitemsShadeButton_, SLOT( setChecked( bool ) ) );
  connect( surfacePlot_, SIGNAL( meshEnabled( bool ) ), displayitemsMeshButton_, SLOT( setChecked( bool ) ) );
  connect( surfacePlot_, SIGNAL( legendEnabled( bool ) ), displayitemsLegendButton_, SLOT( setChecked( bool ) ) );
  connect( surfacePlot_, SIGNAL( axesEnabled( bool ) ), displayitemsAxesButton_, SLOT( setChecked( bool ) ) );
  connect( surfacePlot_, SIGNAL( isolinesEnabled( bool ) ), displayitemsIsolinesButton_, SLOT( setChecked( bool ) ) );
  connect( surfacePlot_, SIGNAL( nIsolines( int ) ), displayitemsIsolinesSpinbox_, SLOT( setValue( int ) ) );
  connect( displayitemsIsolinesButton_, SIGNAL( toggled( bool ) ), displayitemsIsolinesSpinbox_, SLOT( setEnabled( bool ) ) );

  connect( zscalePlusButton_, SIGNAL( clicked() ), surfacePlot_, SLOT( increaseZScale() ) );
  connect( zscaleMinusButton_, SIGNAL( clicked() ), surfacePlot_, SLOT( decreaseZScale() ) );

  // advanced
  connect( basefolderEditButton_, SIGNAL( clicked() ), this, SLOT( editBaseFolder() ) );
  connect( cameraConnectionCheckBox_, SIGNAL( toggled(bool) ), this, SLOT( cameraEnabledButtonToggled(bool) ) );
  connect( advancedApplyButton_, SIGNAL( clicked() ), this, SLOT( writeParameters() ) );

  connect( lightPanelsButtons_.at( 0 ), SIGNAL( clicked() ), this, SLOT( panelButton1Clicked() ) );
  connect( lightPanelsButtons_.at( 1 ), SIGNAL( clicked() ), this, SLOT( panelButton2Clicked() ) );
  connect( lightPanelsButtons_.at( 2 ), SIGNAL( clicked() ), this, SLOT( panelButton3Clicked() ) );
  connect( lightPanelsButtons_.at( 3 ), SIGNAL( clicked() ), this, SLOT( panelButton4Clicked() ) );
  connect( lightPanelsButtons_.at( 4 ), SIGNAL( clicked() ), this, SLOT( panelButton5Clicked() ) );
  connect( allPanelsOnButton_, SIGNAL(clicked()), this, SLOT(allPanelsOn() ) );
  connect( allPanelsOffButton_, SIGNAL(clicked()), this, SLOT(allPanelsOff() ) );

}



///
/// to be called upon a DefoSchedule::newAction signal
///
void DefoMainWindow::handleAction( DefoSchedule::scheduleItem item ) {

  bool isContinuePolling = true; // local decision

  switch( item.first ) {


  case DefoSchedule::SET:
    {
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received SET" << std::endl;
      
      if( !isCameraEnabled_ ) {
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [SET] camera is disabled. Exit." << std::endl;
	return;
      }

      // disable polling, wait for PAUSE/RES button pressed
      pausePolling();
      
      // by default, enable area display & points, disable indices
      displayAreasButton_->setChecked( true );
      displayRecoitemButton_->setChecked( true );
      displayIndicesButton_->setChecked( false );
      displayCoordsButton_->setChecked( true );

      // output folder
      QDir outputDir = checkAndCreateOutputFolder( "set" );

      // get the image
      loadImageFromCamera();

      // switch state
      if( areas_.empty() ) {
	areaNewButton_->setEnabled( true ); 
	areaDeleteButton_->setEnabled( false );
      } else {
	areaNewButton_->setEnabled( false ); // for the moment, restricted to 1 rea // @@@@
	areaDeleteButton_->setEnabled( true );
      }


    }
    break;
    


  case DefoSchedule::REF:
    {

      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received REF" << std::endl;

      if( !isCameraEnabled_ ) {
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [REF] camera is disabled. Skip." << std::endl;
	break;
      }

      // disable any area modification action now
      areaNewButton_->setEnabled( false );
      areaDeleteButton_->setEnabled( false );

      loadImageFromCamera();
      DefoRawImage refImage( rawimageLabel_->getOriginalImage() );

      QDir outputDir = checkAndCreateOutputFolder( "ref" );

      // get the image & save it
      QString rawImageFileName = outputDir.path() + "/refimage_raw.jpg";
      refImage.getImage().save( rawImageFileName, 0, 100 );


      // point reconstruction

      // check if we have an area defined;
      // otherwise define area as the whole image
      DefoArea area;
      if( areas_.empty() ) {
	if( debugLevel_ >= 3 ) std::cout << " [DefoMainWindow::handleAction] =3= [REF] creating pseudo area over whole image." << std::endl;
	QRect rect( 0, 0, refImage.getImage().width(), refImage.getImage().height() );
	area = DefoArea( rect );
      }
      else area = areas_.at( 0 );
      referenceImageOutput_ = defoRecoImage_.reconstruct( refImage, area );
      emit( imagelabelRefreshPointSquares( defoRecoImage_.getForbiddenAreas() ) );

      // do the indexing
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= [REF] Indexing ref points" << std::endl;
      std::pair<unsigned int, unsigned int> refPointIndexRange = defoRecoSurface_.indexPoints( referenceImageOutput_.first );
      Q_UNUSED( refPointIndexRange ); // for the moment

      // get the indexedPoints & pass them to the display
      std::vector<DefoPoint> const& indexedPoints = defoRecoSurface_.getIndexedPoints();
      emit( imagelabelRefreshIndices( indexedPoints ) );

      // write to text file, should later be serialized for easier reading
      QString pointFilePath = outputDir.path() + "/points.txt";
      writePointsToFile( indexedPoints, pointFilePath );

      // display info:
      // displaying is done by handleCameraAction()

      // we have a reference now, enable future defo
      isRefImage_ = true; refCheckBox_->setChecked( true );

      // by default, enable area display & points, disable indices
      displayAreasButton_->setChecked( true );
      displayRecoitemButton_->setChecked( true );
      displayIndicesButton_->setChecked( false );
      displayCoordsButton_->setChecked( true );

    }
    break;




  case DefoSchedule::DEFO:
    {
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received DEFO" << std::endl;
      

      if( !isCameraEnabled_ ) {
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [DEFO] camera is disabled. Skip." << std::endl;
	break;
      }

      if( !isRefImage_ ) { // check if we have a reference for the reconstruction
	QMessageBox::critical( this, tr("[DefoMainWindow::handleAction]"),
			       QString("[FILE_DEFO]: no reference image taken."),//.arg(item.second),
			       QMessageBox::Ok );
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [FILE_DEFO]: no reference image taken." << item.second.toStdString() << std::endl;
	scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 255,0,0 ); "));
	stopPolling();
	isContinuePolling = false; // stop the schedule
	break;
      }

      // get image: first to display, then grab it for reco
      loadImageFromCamera();
      DefoRawImage defoImage( rawimageLabel_->getOriginalImage() );

      // output folder
      QDir outputDir = checkAndCreateOutputFolder( "defo" );

      // get the image & save the raw version
      QString rawImageFileName = outputDir.path() + "/defoimage_raw.jpg";
      defoImage.getImage().save( rawImageFileName, 0, 100 );
      
      // point reconstruction

      // check if we have an area defined;
      // otherwise define the whole image
      DefoArea area;
      if( areas_.empty() ) {
	if( debugLevel_ >= 3 ) std::cout << " [DefoMainWindow::handleAction] =3= [DEFO] creating pseudo area." << std::endl;
	QRect rect( 0, 0, defoImage.getImage().width(), defoImage.getImage().height() );
	area = DefoArea( rect );
      }
      else area = areas_.at( 0 );

      defoImageOutput_ = defoRecoImage_.reconstruct( defoImage, area );
      emit( imagelabelRefreshPointSquares( defoRecoImage_.getForbiddenAreas() ) );

      // run the indexing
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= [DEFO] Indexing defo points" << std::endl;
      std::pair<unsigned int, unsigned int> recoPointIndexRange = defoRecoSurface_.indexPoints( defoImageOutput_.first );
      Q_UNUSED( recoPointIndexRange );

      // get the indexedPoints & pass them to the display
      std::vector<DefoPoint> const& indexedPoints = defoRecoSurface_.getIndexedPoints();
      emit( imagelabelRefreshIndices( indexedPoints ) );

      // write to text file, should later be serialized for easier reading
      QString pointFilePath = outputDir.path() + "/points.txt";
      writePointsToFile( indexedPoints, pointFilePath );

      // display info
      // displaying is done by handleCameraAction()

      // surface reconstruction
      const DefoSurface defoSurface = defoRecoSurface_.reconstruct( defoImageOutput_.first, referenceImageOutput_.first );

      surfacePlot_->setDisplayTitle( measurementId_ );
      surfacePlot_->setData( defoSurface );
      surfacePlot_->draw();

      { // serialize the output
	QString surfaceFileName = outputDir.path() + "/surface.defosurface";
	std::ofstream ofs( surfaceFileName.toStdString().c_str() );
	boost::archive::binary_oarchive oa( ofs );
	oa << defoSurface;
      }

      if( areas_.empty() ) {
	areaNewButton_->setEnabled( true ); 
	areaDeleteButton_->setEnabled( false );
      } else {
	areaNewButton_->setEnabled( false ); // for the moment, restricted to 1 rea // @@@@
	areaDeleteButton_->setEnabled( true );
      }

      // by default, enable area display & points, disable indices
      displayAreasButton_->setChecked( true );
      displayRecoitemButton_->setChecked( true );
      displayIndicesButton_->setChecked( false );
      displayCoordsButton_->setChecked( true );

    }
    break;




  case DefoSchedule::FILE_SET:
    //
    // 
    //
    {
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received FILE_SET" << std::endl;

      // disable polling, wait for PAUSE/RES button pressed
      pausePolling();
      
      // by default, enable area display & points, disable indices
      displayAreasButton_->setChecked( true );
      displayRecoitemButton_->setChecked( true );
      displayIndicesButton_->setChecked( false );
      displayCoordsButton_->setChecked( true );


      QFileInfo fileInfo( item.second );
      if( !fileInfo.exists() ) {
	QMessageBox::critical( this, tr("[DefoMainWindow::handleAction]"),
			       QString("[FILE_SET]: file \'%1\' not found.").arg(item.second),
			       QMessageBox::Ok );
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [FILE_SET] cannot open file: " << item.second.toStdString() << std::endl;
	imageinfoTextedit_->appendPlainText( QString( "ERROR: [FILE_SET] cannot open file: \'%1\'" ).arg( item.second ) );
	scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 255,0,0 ); "));
	break;
      }

      QDir outputDir = checkAndCreateOutputFolder( "file_set" );

      // get the image & save it
      DefoRawImage setImage( item.second.toStdString().c_str() );
      QString rawImageFileName = outputDir.path() + "/setimage_raw.jpg";
      setImage.getImage().save( rawImageFileName, 0, 100 );

      // display info
      imageinfoTextedit_->clear();
      QDateTime datime = QDateTime::currentDateTime(); // resuse!!
      imageinfoTextedit_->appendPlainText( QString( "Raw image size: %1 x %2 pixel" ).arg(setImage.getImage().width()).arg(setImage.getImage().height()) );
      imageinfoTextedit_->appendPlainText( QString( "Fetched: %1" ).arg( datime.toString( QString( "dd.MM.yy hh:mm:ss" ) ) ) );
      imageinfoTextedit_->appendPlainText( QString( "Type: from disk [%1]" ).arg( item.second ) );

      // tell the label to rotate the image and display
      rawimageLabel_->setRotation( true );
      rawimageLabel_->displayImageToSize( setImage.getImage() );

      if( areas_.empty() ) {
	areaNewButton_->setEnabled( true ); 
	areaDeleteButton_->setEnabled( false );
      } else {
	areaNewButton_->setEnabled( false ); // for the moment, restricted to 1 rea // @@@@
	areaDeleteButton_->setEnabled( true );
      }

    } break;
    





  case DefoSchedule::FILE_REF:
    {
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received FILE_REF" << std::endl;

      DefoRawImage refImage;
      
      // disable any area modification action now
      areaNewButton_->setEnabled( false );
      areaDeleteButton_->setEnabled( false );

     if( isManual_ ) { // operation with manual buttons: use image from imageLabel_

	if( !( rawimageLabel_->isImage() ) ) {
	  if( debugLevel_ >= 1 ) std::cout << " [DefoMainWindow::handleAction] =1= [FILE_REF] quitting since no image loaded." << std::endl;
	  return;
	}

	refImage.setImage( rawimageLabel_->getOriginalImage() );

	QString subdirName = measurementidTextedit_->toPlainText();
	currentFolderName_ = baseFolderName_ + "/" + subdirName;

      }

      else { // operation via schedule: load image from file

	// check file name/path
	QFileInfo fileInfo( item.second );
	if( !fileInfo.exists() ) {
	  QMessageBox::critical( this, tr("[DefoMainWindow::handleAction]"),
				 QString("[FILE_REF]: file \'%1\' not found.").arg(item.second),
				 QMessageBox::Ok );
	  std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [FILE_REF] cannot open file: " << item.second.toStdString() << std::endl;
	  imageinfoTextedit_->appendPlainText( QString( "ERROR: [FILE_REF] cannot open file: \'%1\'" ).arg( item.second ) );
	  scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 255,0,0 ); "));
	  stopPolling();
	  isContinuePolling = false;
	  break;

	}
	refImage.loadFromFile( item.second.toStdString().c_str() );
      }

     QDir outputDir = checkAndCreateOutputFolder( "file_ref" );

      // get the image & save it
      QString rawImageFileName = outputDir.path() + "/refimage_raw.jpg";
      refImage.getImage().save( rawImageFileName, 0, 100 );


      // point reconstruction

      // check if we have an area defined;
      // otherwise define area as the whole image
      DefoArea area;
      if( areas_.empty() ) {
	if( debugLevel_ >= 3 ) std::cout << " [DefoMainWindow::handleAction] =3= [FILE_REF] creating pseudo area over whole image." << std::endl;
	QRect rect( 0, 0, refImage.getImage().width(), refImage.getImage().height() );
	area = DefoArea( rect );
      }
      else area = areas_.at( 0 );
      referenceImageOutput_ = defoRecoImage_.reconstruct( refImage, area );
      QImage& qImage =  referenceImageOutput_.second.getImage();
      emit( imagelabelRefreshPointSquares( defoRecoImage_.getForbiddenAreas() ) );

      // do the indexing
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= [FILE_REF] Indexing ref points" << std::endl;
      std::pair<unsigned int, unsigned int> refPointIndexRange = defoRecoSurface_.indexPoints( referenceImageOutput_.first );
      Q_UNUSED( refPointIndexRange ); // for the moment

      // get the indexedPoints & pass them to the display
      std::vector<DefoPoint> const& indexedPoints = defoRecoSurface_.getIndexedPoints();
      emit( imagelabelRefreshIndices( indexedPoints ) );

      // write to text file, should later be serialized for easier reading
      QString pointFilePath = outputDir.path() + "/points.txt";
      writePointsToFile( indexedPoints, pointFilePath );

      // display info
      imageinfoTextedit_->clear();
      QDateTime datime = QDateTime::currentDateTime(); // resuse!!
      imageinfoTextedit_->appendPlainText( QString( "Raw image size: %1 x %2 pixel" ).arg(qImage.width()).arg(qImage.height()) );
      imageinfoTextedit_->appendPlainText( QString( "Fetched: %1" ).arg( datime.toString( QString( "dd.MM.yy hh:mm:ss" ) ) ) );
      imageinfoTextedit_->appendPlainText( QString( "Type: from disk [%1]" ).arg( item.second ) );

      // tell the label to rotate the image and display
      rawimageLabel_->setRotation( true );
      rawimageLabel_->displayImageToSize( qImage );

      // we have a reference now, enable future defo
      isRefImage_ = true; refCheckBox_->setChecked( true );

      // by default, enable area display & points, disable indices
      displayAreasButton_->setChecked( true );
      displayRecoitemButton_->setChecked( true );
      displayIndicesButton_->setChecked( false );
      displayCoordsButton_->setChecked( true );
      
    }
    break;






  case DefoSchedule::FILE_DEFO:
    {
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received FILE_DEFO" << std::endl;

      DefoRawImage defoImage;

      if( !isRefImage_ ) { // check if we have a reference for the reconstruction
	QMessageBox::critical( this, tr("[DefoMainWindow::handleAction]"),
			       QString("[FILE_DEFO]: no reference image taken."),//.arg(item.second),
			       QMessageBox::Ok );
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [FILE_DEFO]: no reference image taken." << item.second.toStdString() << std::endl;
	scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 255,0,0 ); "));
	stopPolling();
	isContinuePolling = false;
	break;
      }

      // disable any area modification action now
      areaNewButton_->setEnabled( false );
      areaDeleteButton_->setEnabled( false );


      if( isManual_ ) { // operation with manual buttons: use image from imageLabel_
	
	if( !( rawimageLabel_->isImage() ) ) {
	  if( debugLevel_ >= 1 ) std::cout << " [DefoMainWindow::handleAction] =1= [FILE_REF] quitting since no image loaded." << std::endl;
	  return;
	}
	
	defoImage.setImage( rawimageLabel_->getOriginalImage() );
	
	QString subdirName = measurementidTextedit_->toPlainText();
	currentFolderName_ = baseFolderName_ + "/" + subdirName;
	
      }

      else {

	// check file name/path
	QFileInfo fileInfo( item.second );
	if( !fileInfo.exists() ) {
	  QMessageBox::critical( this, tr("[DefoMainWindow::handleAction]"),
				 QString("[FILE_DEFO]: file \'%1\' not found.").arg(item.second),
				 QMessageBox::Ok );
	  std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [FILE_DEFO] cannot open file: " << item.second.toStdString() << std::endl;
	  imageinfoTextedit_->appendPlainText( QString( "ERROR: [FILE_DEFO] cannot open file: \'%1\'" ).arg( item.second ) );
	  scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 255,0,0 ); "));
	  stopPolling();
	  isContinuePolling = false;
	  break;
	}

	defoImage.loadFromFile( item.second.toStdString().c_str() );

      }
	
      // output folder
      QDir outputDir = checkAndCreateOutputFolder( "file_defo" );

      // get the image & save the raw version
      QString rawImageFileName = outputDir.path() + "/defoimage_raw.jpg";
      defoImage.getImage().save( rawImageFileName, 0, 100 );
      
      // point reconstruction

      // check if we have an area defined;
      // otherwise define the whole image
      DefoArea area;
      if( areas_.empty() ) {
	if( debugLevel_ >= 3 ) std::cout << " [DefoMainWindow::handleAction] =3= [FILE_DEFO] creating pseudo area." << std::endl;
	QRect rect( 0, 0, defoImage.getImage().width(), defoImage.getImage().height() );
	area = DefoArea( rect );
      }
      else area = areas_.at( 0 );

      defoImageOutput_ = defoRecoImage_.reconstruct( defoImage, area );
      QImage& qImage =  defoImageOutput_.second.getImage();
      emit( imagelabelRefreshPointSquares( defoRecoImage_.getForbiddenAreas() ) );

      // run the indexing
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= [FILE_DEFO] Indexing defo points" << std::endl;
      std::pair<unsigned int, unsigned int> recoPointIndexRange = defoRecoSurface_.indexPoints( defoImageOutput_.first );
      Q_UNUSED( recoPointIndexRange );

      // get the indexedPoints & pass them to the display
      std::vector<DefoPoint> const& indexedPoints = defoRecoSurface_.getIndexedPoints();
      emit( imagelabelRefreshIndices( indexedPoints ) );

      // write to text file, should later be serialized for easier reading
      QString pointFilePath = outputDir.path() + "/points.txt";
      writePointsToFile( indexedPoints, pointFilePath );

      // display info
      imageinfoTextedit_->clear();
      QDateTime datime = QDateTime::currentDateTime(); // resuse!
      imageinfoTextedit_->appendPlainText( QString( "Raw image size: %1 x %2 pixel" ).arg(qImage.width()).arg(qImage.height()) );
      imageinfoTextedit_->appendPlainText( QString( "Fetched: %1" ).arg( datime.toString( QString( "dd.MM.yy hh:mm:ss" ) ) ) );
      imageinfoTextedit_->appendPlainText( QString( "Type: from disk [%1]" ).arg( item.second ) );

      // tell the label to rotate the image and display
      rawimageLabel_->setRotation( true );
      rawimageLabel_->displayImageToSize( qImage );

      // surface reconstruction
      const DefoSurface defoSurface = defoRecoSurface_.reconstruct( defoImageOutput_.first, referenceImageOutput_.first );

      surfacePlot_->setDisplayTitle( measurementId_ );
      surfacePlot_->setData( defoSurface );
      surfacePlot_->draw();

      { // serialize the output
	QString surfaceFileName = outputDir.path() + "/surface.defosurface";
	std::ofstream ofs( surfaceFileName.toStdString().c_str() );
	boost::archive::binary_oarchive oa( ofs );
	oa << defoSurface;
      }

      if( areas_.empty() ) {
	areaNewButton_->setEnabled( true ); 
	areaDeleteButton_->setEnabled( false );
      } else {
	areaNewButton_->setEnabled( false ); // for the moment, restricted to 1 rea // @@@@
	areaDeleteButton_->setEnabled( true );
      }

      // by default, enable area display & points, disable indices
      displayAreasButton_->setChecked( true );
      displayRecoitemButton_->setChecked( true );
      displayIndicesButton_->setChecked( false );
      displayCoordsButton_->setChecked( true );
      
    }
    break;




  case DefoSchedule::TEMP:
    std::cout << " TEMP action yet unsupported." << std::endl;
    break;



  case DefoSchedule::SLEEP:
    {
      if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= received SLEEP " << item.second.toStdString() << std::endl;
      bool isOk = false;
      unsigned int time = item.second.toUInt( &isOk, 10 );
      if( !isOk ) {
	std::cerr << " [DefoMainWindow::handleAction] ** ERROR: [SLEEP] bad conversion to uint: " << item.second.toStdString() << std::endl;
	stopPolling();
	isContinuePolling = false;
	break;
      }
      isContinuePolling = false; // further polling done by timer

      if( isPolling_ ) {
	sleepTimer_.setInterval( time*1000 );
	connect( &sleepTimer_, SIGNAL(timeout()), schedule_, SLOT(pollAction()) );
	sleepTimer_.run();
      }
    }
    break;



  case DefoSchedule::GOTO:
    // this case is caught and handled by the schedule, so we should never arrive here
    std::cerr << " [DefoMainWindow::handleAction] ** WARNING: [GOTO] this should not happen :-(" << std::endl;
    break;



  case DefoSchedule::END:
    if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::handleAction] =2= Stop polling on END" << std::endl;
    stopPolling();
    isContinuePolling = false;
    break;

  default:
    break;

  }

  // we poll with a delay to give everybody time to update
  if( isContinuePolling && isPolling_ ) QTimer::singleShot( 1000, schedule_, SLOT(pollAction()) );

}



///
///
///
void DefoMainWindow::startPolling( void ) {

  schedule_->setIndex( 0 ); // start from beginning
  
  // create output folder
  QDir baseDir( baseFolderName_ );
  QString subdirName = measurementidTextedit_->toPlainText();
  QDir subdir = QDir( baseFolderName_ + "/" + subdirName );

  if( subdir.exists() ) {
    QMessageBox::StandardButton rVal = QMessageBox::question( this, tr("[DefoMainWindow::startPolling]"),
        QString("Directory \'%1\' already exists, do you want to save data from this measurement to it?\n\nIf not, hit CANCEL and change the measurement ID.").arg(subdirName), 
        QMessageBox::Ok | QMessageBox::Cancel );

    if( QMessageBox::Cancel == rVal ) return;

  }

  else if( !baseDir.mkdir( subdirName ) ) {
    QMessageBox::critical( this, tr("[DefoMainWindow::startPolling]"),
			   QString("Cannot create measurement dir: \'%1\'").arg(subdirName),
			   QMessageBox::Ok );
    std::cerr << " [DefoMainWindow::handleAction] ** ERROR: cannot create measurement dir: " 
	      << subdirName.toStdString() << std::endl;
  }

  currentFolderName_ = baseFolderName_ + "/" + subdirName;

  // highlight color for table view to green
  scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 0,155,0 ); "));

  // toggle some buttons enable
  scheduleStartButton_->setEnabled( false ); 
  schedulePauseButton_->setEnabled( true );
  scheduleStopButton_->setEnabled( true );
  measurementidEditButton_->setEnabled( false );
  measurementidDefaultButton_->setEnabled( false );
  basefolderEditButton_->setEnabled( false );
  scheduleLoadButton_->setEnabled( false );
  scheduleClearButton_->setEnabled( false );

  // no manual operation when schedule is active
  manualREFButton_->setEnabled( false );
  manualDEFOButton_->setEnabled( false );
  manualTEMPButton_->setEnabled( false );

  // and no schedule editing either
  scheduleTableview_->setEnabled( false );

  // clear display items (with empty vectors)
  emit( imagelabelRefreshIndices( std::vector<DefoPoint>() ) );
  emit( imagelabelRefreshPointSquares( std::vector<DefoSquare>() ) );

  imageinfoTextedit_->clear(); // clear the imageinfo

  isPolling_ = true;

  emit pollAction(); // and go...
  
}



///
/// stop polling actions from the scheduler
///
void DefoMainWindow::stopPolling( void ) {

  if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::stopPolling] =2= Stop polling." << std::endl;

  // free the buttons etc.
  scheduleStartButton_->setEnabled( true );
  schedulePauseButton_->setEnabled( false );
  scheduleStopButton_->setEnabled( false );
  measurementidEditButton_->setEnabled( true );
  measurementidDefaultButton_->setEnabled( true );
  basefolderEditButton_->setEnabled( true );
  scheduleLoadButton_->setEnabled( true );
  scheduleClearButton_->setEnabled( true );

  // also enable manual operation
  manualREFButton_->setEnabled( true );
  manualDEFOButton_->setEnabled( true );
  scheduleTableview_->setEnabled( true ); // enable editing

  // gray out
  scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 200,200,200 ); "));

  isPolling_ = false;
  
  // take care that the timer stops, otherwise it will trigger polling again
  if( sleepTimer_.isActive() ) sleepTimer_.halt();
  
  
}



///
/// pause polling actions from the scheduler
///
void DefoMainWindow::pausePolling( void ) {

  if( isPolling_ ) {
    if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::pausePolling] =2= Pause polling." << std::endl;
    scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 200,200,0 ); "));
    isPolling_ = false;
    if( sleepTimer_.isRunning() ) sleepTimer_.pause();
  }
  else {
    if( debugLevel_ >= 2 ) std::cout << " [DefoMainWindow::pausePolling] =2= Resume polling." << std::endl;
    scheduleTableview_->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);\n selection-background-color: rgb( 0,200,0 ); "));
    isPolling_ = true;
    
    // if schedule has been paused during a sleep, we resume sleeping
    if( sleepTimer_.isRunning() )  sleepTimer_.resume();
    else emit pollAction(); // otherwise goto next schedule item
  }

}



///
///
///
void DefoMainWindow::manualFileRef( void ) {

  // string is empty in that case, since we load from image label
  isManual_ = true;
  DefoSchedule::scheduleItem item( DefoSchedule::FILE_REF, QString( "" ) );
  handleAction( item );
  isManual_ = false;

  // by default, enable area display & points, disable indices
  displayAreasButton_->setChecked( true );
  displayRecoitemButton_->setChecked( true );
  displayIndicesButton_->setChecked( false );
  displayCoordsButton_->setChecked( true );
  
}



///
///
///
void DefoMainWindow::manualFileDefo( void ) {

  // string is empty in that case, since we load from image label
  isManual_ = true;
  DefoSchedule::scheduleItem item( DefoSchedule::FILE_DEFO, QString( "" ) );
  handleAction( item );
  isManual_ = false;

  // by default, enable area display & points, disable indices
  displayAreasButton_->setChecked( true );
  displayRecoitemButton_->setChecked( true );
  displayIndicesButton_->setChecked( false );
  displayCoordsButton_->setChecked( true );
  
}



///
/// set the measurementid string
///
void DefoMainWindow::editMeasurementId( void ) {
  
  bool ok;
  measurementId_ = QInputDialog::getText( 0, "MeasurementID", "MeasurementID: ", QLineEdit::Normal, measurementidTextedit_->toPlainText(), &ok);
  
  if( ok && !measurementId_.isEmpty() ) measurementidTextedit_->setPlainText( measurementId_ );

}



///
/// set the measurementid string
///
void DefoMainWindow::defaultMeasurementId( void ) {
  
  QDateTime datime = QDateTime::currentDateTime();
  measurementId_ = QString( "defomeasurement-%1" ).arg( datime.toString( QString( "ddMMyy-hhmmss" ) ) );
  measurementidTextedit_->setPlainText( measurementId_ );

}



///
///
///
void DefoMainWindow::editBaseFolder( void ) {
  
  QString dirName = QFileDialog::getExistingDirectory( this,
							QString("Base folder"),
							QString("defo"),
							QFileDialog::ShowDirsOnly
						      );

  if( dirName.isEmpty() ) return;

  baseFolderName_ = dirName;
  basefolderTextedit_->setPlainText( baseFolderName_ );
  
}



///
///
///
void DefoMainWindow::receiveArea( DefoArea area ) {

  QString name( QString( "A%1" ).arg( areaComboBox_->count() + 1 ) );
  area.setName( name );

  areas_.push_back( area );

  // increment area
  areaComboBox_->addItem( name );

  // show last entry
  areaComboBox_->setCurrentIndex( areaComboBox_->count() - 1 );

  // we definitely have an item, so allow deleting
  // (may not be necessary if count already > 0)
  areaDeleteButton_->setEnabled( true );

  // FOR THE MOMENT, RESTRICTED TO 1 AREA // @@@@@@@
  areaNewButton_->setEnabled( false );
  
  // refresh display
  emit( imagelabelRefreshAreas( areas_ ) );

  if( debugLevel_ >= 3 ) std::cout << " [DefoMainWindow::receiveArea] =3= Received area: x: " 
				   << area.getRectangle().x() << " y: " << area.getRectangle().y()
				   << " w: " << area.getRectangle().width() << " h: " << area.getRectangle().height() << " (in original image)"
				   << std::endl;

}



///
///
///
void DefoMainWindow::deleteCurrentArea( void ) {

  if( 0 == areaComboBox_->count() ) {
    std::cerr << " [DefoMainWindow::deleteCurrentArea] ** WARNING: called despite area vector is empty, ignoring." << std::endl;
    return;
  }

  int index = areaComboBox_->currentIndex();
  
  // remove area & combobox entry
  areas_.erase( areas_.begin() + index );
  areaComboBox_->removeItem( index );

  // disable the button to block further delete attempts if no more areas
  if( 0 == areaComboBox_->count() ) areaDeleteButton_->setEnabled( false );
  
  // FOR THE MOMENT, RESTRICTED TO 1 AREA // @@@@@@@
  if( 0 == areaComboBox_->count() ) areaNewButton_->setEnabled( true );
  else areaNewButton_->setEnabled( false );

  // redraw remaining areas
  emit( imagelabelRefreshAreas( areas_ ) );

  // we have to disable the refimage
  // because the ref info is bound to the area
  isRefImage_ = false; refCheckBox_->setChecked( false );

}



///
///
///
void DefoMainWindow::toggleAreaDisplay( bool isChecked ) {

  emit( imagelabelDisplayAreas( isChecked ) );

}



///
///
///
void DefoMainWindow::toggleIndicesDisplay( bool isChecked ) {

  emit( imagelabelDisplayIndices( isChecked ) );

}



///
///
///
void DefoMainWindow::togglePointSquareDisplay( bool isChecked ) {

  emit( imagelabelDisplayPointSquares( isChecked ) );

}



///
/// this function triggers threaded file download from the camera;
/// the thread emits actionFinished when ready which is connected to handleCameraAction;
/// the file path has to be put in filepathPlaced_
/// so that handleCameraAction can pick it up there
///
void DefoMainWindow::loadImageFromCamera( void ) {

//   DefoRawImage image( "test/test3/ref_cr.jpg" ); /////////////////////////////////
//   rawimageLabel_->setRotation( true ); /////////////////////////////////
//   rawimageLabel_->displayImageToSize( image.getImage() ); /////////////////////////////////

  // temp display
  QImage tmpImage( "icons/loading.jpg" );
  rawimageLabel_->displayImageToSize( tmpImage );

  // image is stored as a "set-" file
  // above the subfolder structure
  currentFolderName_ = baseFolderName_ + "/" + measurementidTextedit_->toPlainText();
  QDir folderDir( currentFolderName_ );
  
  if( !folderDir.exists() ) QDir( baseFolderName_ ).mkdir( measurementidTextedit_->toPlainText() );

  QDateTime datime = QDateTime::currentDateTime();
  QString filePath = currentFolderName_ + QString( "/" ) // folder 
    + QString( "set-" ) + datime.toString( QString( "ddMMyy-hhmmss" ) ) // filename
    + QString( ".jpg" ); // extension
  
  // tell the handler what to do (and where)
  // and run thread
  camHandler_.setAction( DefoCamHandler::GETIMAGE );
  camHandler_.setFilePath( filePath.toStdString() );
  camHandler_.start();

  filepathPlaced_ = filePath.toStdString();
  
  this->setCursor( Qt::BusyCursor );

}
 
 
 
///
/// we assume that filepathPlaced_ is correctly set;
/// but better check...
///
void DefoMainWindow::handleCameraAction( DefoCamHandler::Action action ) {
  
  
  switch( action ) {

  case DefoCamHandler::GETCFG:
    {
      this->setCursor( Qt::ArrowCursor );
    } break;

  case DefoCamHandler::SETCFG:
    {
      this->setCursor( Qt::ArrowCursor );
    } break;
    

  case DefoCamHandler::GETIMAGE:
    { //  loads image on display

      QFile file( QString( filepathPlaced_.c_str() ) );
      if( !file.exists() ) {
	std::cerr << " [DefoMainWindow::displayImageFromCamera] ** ERROR: file \""
		  << file.fileName().toStdString() << "\" does not exist." << std::endl;
	QMessageBox::critical( this, tr("[DefoMainWindow::handleCameraAction]"),
			       QString("File: %1 does not exist.").arg( QString( filepathPlaced_.c_str() ) ),
			       QMessageBox::Ok );
	return;
      }
      
      DefoRawImage img( filepathPlaced_.c_str() );
      
      if( img.getImage().isNull() ) {
	std::cerr << " [DefoMainWindow::displayImageFromCamera] ** ERROR: file \""
		  << file.fileName().toStdString() << "\" is null." << std::endl;
	QMessageBox::critical( this, tr("[DefoMainWindow::handleCameraAction]"),
			       QString("Retrieved null image from file: %1.").arg( QString( filepathPlaced_.c_str() ) ),
			       QMessageBox::Ok );
	return;
      }
      
      rawimageLabel_->setRotation( true );
      rawimageLabel_->displayImageToSize( img.getImage() );
      this->setCursor( Qt::ArrowCursor );
      areaNewButton_->setEnabled( true );

      // image info;
      // reading cam settings from the comboboxes,
      // should find a way of reading/translating this directly from the camhandler..
      QDateTime datime = QDateTime::currentDateTime();
      imageinfoTextedit_->clear();
      imageinfoTextedit_->appendPlainText( QString( "Raw image size: %1 x %2 pixel" ).arg(img.getImage().width()).arg(img.getImage().height()) );
      imageinfoTextedit_->appendPlainText( QString( "Fetched: %1" ).arg( datime.toString( QString( "dd.MM.yy hh:mm:ss" ) ) ) );
      imageinfoTextedit_->appendPlainText( //&
        QString( "Type: from camera [%1 %2 ISO%3]" ) //&
	  .arg( exptimeComboBox_->currentText() )
   	  .arg( apertureComboBox_->currentText() )
	  .arg( isoComboBox_->currentText() ) );

      // few settings
      if( areas_.empty() ) {
	areaNewButton_->setEnabled( true ); 
	areaDeleteButton_->setEnabled( false );
      } else {
	areaNewButton_->setEnabled( false ); // for the moment, restricted to 1 area // @@@@
	areaDeleteButton_->setEnabled( true );
      }
      
      emit( imagelabelRefreshPointSquares( std::vector<DefoSquare>() ) );
      emit( imagelabelRefreshIndices( std::vector<DefoPoint>() ) );
      
    } break;


  default:
    break;

  }


}



///
///
///
void DefoMainWindow::loadImageFromFile( void ) {
  
  QFileDialog::Options options;

  QString selectedFilter;
  QString fileName = QFileDialog::getOpenFileName( 0,
						   QString("Image"),
						   QString("."),
						   QString("Raw image files (*.jpg *png)"),
						   &selectedFilter,
						   options );

  if( fileName.isEmpty() ) return;

  QImage img( fileName );
  rawimageLabel_->setRotation( true );
  rawimageLabel_->displayImageToSize( img );
  areaNewButton_->setEnabled( true );

  QDateTime datime = QDateTime::currentDateTime();
  imageinfoTextedit_->clear();
  imageinfoTextedit_->appendPlainText( QString( "Raw image size: %1 x %2 pixel" ).arg(img.width()).arg(img.height()) );
  imageinfoTextedit_->appendPlainText( QString( "Fetched: %1" ).arg( datime.toString( QString( "dd.MM.yy hh:mm:ss" ) ) ) );
  imageinfoTextedit_->appendPlainText( QString( "Type: from disk [%1]" ).arg( fileName ) );

  // few settings

  if( areas_.empty() ) {
    areaNewButton_->setEnabled( true ); 
    areaDeleteButton_->setEnabled( false );
  } else {
    areaNewButton_->setEnabled( false ); // for the moment, restricted to 1 rea // @@@@
    areaDeleteButton_->setEnabled( true );
  }

  emit( imagelabelRefreshPointSquares( std::vector<DefoSquare>() ) );
  emit( imagelabelRefreshIndices( std::vector<DefoPoint>() ) );
  
}



///
///
///
void DefoMainWindow::fillComboBoxes( void ) {

  // shutter speed
  int enumIndex = metaObject()->indexOfEnumerator( "ShutterSpeed" );
  QMetaEnum metaEnum = metaObject()->enumerator( enumIndex );
  
  for( int i = 0; i < metaEnum.keyCount(); ++i ) {
    
    std::string shutterSpeedEntry( metaEnum.key( i ) );
    
    // cases
    if( "BULB" == shutterSpeedEntry ) exptimeComboBox_->addItem( "B" );
    else if( "SINVALID" == shutterSpeedEntry ) continue; // dont want this in the list
    else { // now parse the entry
      
      if( "L" == shutterSpeedEntry.substr( 0, 1 ) ) {
	shutterSpeedEntry.erase( 0, 1 ); // remove the "L"
	replace( shutterSpeedEntry.begin(), shutterSpeedEntry.end(), '_', '.');
	shutterSpeedEntry.append( "s" );
      }

      else if( "S" == shutterSpeedEntry.substr( 0, 1 ) ) {
	shutterSpeedEntry.erase( 0, 1 ); // remove the "S"
	shutterSpeedEntry.insert( 0, "1/" );
	shutterSpeedEntry.append( "s" );
      }

      exptimeComboBox_->addItem( shutterSpeedEntry.c_str() );

    }
    
  } // for
  exptimeComboBox_->addItem( "-" ); // when cam is off


  // aperture
  enumIndex = metaObject()->indexOfEnumerator( "Aperture" );
  metaEnum = metaObject()->enumerator( enumIndex );
  
  for( int i = 0; i < metaEnum.keyCount(); ++i ) {
    
    std::string apertureEntry( metaEnum.key( i ) );
    
    // cases
    if( "FINVALID" == apertureEntry ) continue; // dont want this in the list
    else { // now parse the entry
      
      replace( apertureEntry.begin(), apertureEntry.end(), '_', '.');
      apertureComboBox_->addItem( apertureEntry.c_str() );
    }
    
  } // for
  apertureComboBox_->addItem( "-" ); // when cam is off



  // iso
  enumIndex = metaObject()->indexOfEnumerator( "Iso" );
  metaEnum = metaObject()->enumerator( enumIndex );
  
  for( int i = 0; i < metaEnum.keyCount(); ++i ) {
    
    std::string isoEntry( metaEnum.key( i ) );
    
    // cases
    if( "ISOINVALID" == isoEntry ) continue; // dont want this in the list
    else { // now parse the entry
      
      isoEntry.erase( 0, 3 ); // remove the "ISO"
      isoComboBox_->addItem( isoEntry.c_str() );
    }
    
  } // for
  isoComboBox_->addItem( "-" ); // when cam is off



}



///
/// read camera options, set camhandler cfg accordingly
/// (but do not write options to camera)
///
void DefoMainWindow::readCameraParametersFromCfgFile( void ) {
  
  // read options
  DefoConfigReader cfgReader( "defo.cfg" );
  std::string shutterspeed( cfgReader.getValue<std::string>( "CAMERA_SHUTTERSPEED" ) );
  std::string aperture( cfgReader.getValue<std::string>( "CAMERA_APERTURE" ) );
  std::string iso( cfgReader.getValue<std::string>( "CAMERA_ISO" ) );

  // set options

  // shutter speed
  int index = exptimeComboBox_->findText( shutterspeed.c_str(), Qt::MatchExactly | Qt::MatchCaseSensitive );
  if( 0 > index ) {
    std::cerr << " [DefoMainWindow::readCameraOptionsFromCfgFile] ** ERROR: In configuration file: \'"
	      << cfgReader.getFileName() << "\': invalid shutterspeed: \'" << shutterspeed << "\'" << std::endl;
    QMessageBox::critical( this, tr("[DefoMainWindow::readCameraOptionsFromCfgFile]"),
			   QString("In configuration file \'%1\': invalid shutterspeed: \'%2\'").arg(cfgReader.getFileName().c_str() ).arg(shutterspeed.c_str()),
			   QMessageBox::Ok );
    exptimeComboBox_->setCurrentIndex( exptimeComboBox_->count()-1 ); // this is the "-"

    // set camhandler cfg
    camHandler_.setShutterSpeed( EOS550D::SINVALID );
  }
  else {
    exptimeComboBox_->setCurrentIndex( index );
    camHandler_.setShutterSpeed( static_cast<EOS550D::ShutterSpeed>( index ) );
  }


  // aperture
  index = apertureComboBox_->findText( aperture.c_str(), Qt::MatchExactly | Qt::MatchCaseSensitive );
  if( 0 > index ) {
    std::cerr << " [DefoMainWindow::readCameraOptionsFromCfgFile] ** ERROR: In configuration file: \'"
	      << cfgReader.getFileName() << "\': invalid aperture: \'" << aperture << "\'" << std::endl;
    QMessageBox::critical( this, tr("[DefoMainWindow::readCameraOptionsFromCfgFile]"),
			   QString("In configuration file \'%1\': invalid aperture: \'%2\'").arg(cfgReader.getFileName().c_str() ).arg(aperture.c_str()),
			   QMessageBox::Ok );
    apertureComboBox_->setCurrentIndex( apertureComboBox_->count()-1 ); // this is the "-"

    // set camhandler cfg
    camHandler_.setAperture( EOS550D::FINVALID );
  }
  else {
    apertureComboBox_->setCurrentIndex( index );
    camHandler_.setAperture( static_cast<EOS550D::Aperture>( index ) );
  }

  // iso
  index = isoComboBox_->findText( iso.c_str(), Qt::MatchExactly | Qt::MatchCaseSensitive );
  if( 0 > index ) {
    std::cerr << " [DefoMainWindow::readCameraOptionsFromCfgFile] ** ERROR: In configuration file: \'"
	      << cfgReader.getFileName() << "\': invalid iso value: \'" << iso << "\'" << std::endl;
    QMessageBox::critical( this, tr("[DefoMainWindow::readCameraOptionsFromCfgFile]"),
			   QString("In configuration file \'%1\': invalid iso value: \'%2\'").arg(cfgReader.getFileName().c_str() ).arg(iso.c_str()),
			   QMessageBox::Ok );
    isoComboBox_->setCurrentIndex( isoComboBox_->count()-1 ); // this is the "-"

    // set camhandler cfg
    camHandler_.setIso( EOS550D::ISOINVALID );
  }
  else {
    isoComboBox_->setCurrentIndex( index );
    camHandler_.setIso( static_cast<EOS550D::Iso>( index ) );
  }

}



///
/// check if output folder with timestamp exists,
/// otherwise create. "type" is a prefix for the folder name
/// to identify the action
///
QDir const DefoMainWindow::checkAndCreateOutputFolder( char const* type ) {

  QDateTime datime = QDateTime::currentDateTime();
  QDir currentDir( currentFolderName_ );
  QString subdirName = QString( type ) + datime.toString( QString( ".ddMMyy-hhmmss" ) );
  QDir subdir = QDir( currentFolderName_ + "/" + subdirName );
  
  if( subdir.exists() ) return subdir;

  else if( !currentDir.mkpath( subdirName ) ) {
    QMessageBox::critical( this, tr("[DefoMainWindow::checkAndCreateOutputFolder]"),
			   QString("[FILE_SET]: cannot create output dir: \'%1\'").arg(subdirName),
			   QMessageBox::Ok );
    std::cerr << " [DefoMainWindow::checkAndCreateOutputFolder] ** ERROR: cannot create output dir: " 
	      << subdirName.toStdString() << std::endl;
  }

}



///
///
///
void DefoMainWindow::cameraEnabledButtonToggled( bool isChecked ) {

  isCameraEnabled_ = isChecked;

  // upload settings as they're saved in the camHandler cfg member
  if( isCameraEnabled_ ) {
    
    this->setCursor( Qt::BusyCursor );
    
    // tell it what to do & run thread
    camHandler_.setAction( DefoCamHandler::SETCFG );
    camHandler_.start();

    // enable controls
    refreshCameraButton_->setEnabled( true );
    // manualREFButton_->setEnabled( true );
    // manualDEFOButton_->setEnabled( true );
    cameraConnectionTestButton_->setEnabled( true );
    cameraConnectionResetButton_->setEnabled( true );
    apertureComboBox_->setEnabled( true );
    exptimeComboBox_->setEnabled( true );
    isoComboBox_->setEnabled( true );
    wbalanceComboBox_->setEnabled( true );
  }

  else {
    // disable all controls
    refreshCameraButton_->setEnabled( false );
    //    manualREFButton_->setEnabled( false );
    //    manualDEFOButton_->setEnabled( false );
    cameraConnectionTestButton_->setEnabled( false );
    cameraConnectionResetButton_->setEnabled( false );
    apertureComboBox_->setEnabled( false );
    exptimeComboBox_->setEnabled( false );
    isoComboBox_->setEnabled( false );
    wbalanceComboBox_->setEnabled( false );
  }
}



///
/// write all parameters from the advanced tab
///
void DefoMainWindow::writeParameters( void ) {

  // defo classes
  defoRecoImage_.setSeedingThresholds( seedingThresholdsStep1Spinbox_->value(),
				       seedingThresholdsStep2Spinbox_->value(),
				       seedingThresholdsStep3Spinbox_->value()  );
  defoRecoImage_.setHalfSquareWidth( hswSpinBox_->value() );
  defoRecoImage_.setBlueishnessThreshold( blueishnessSpinBox_->value() );
  
  defoRecoSurface_.setSpacingEstimate( surfaceRecoSpacingSpinbox_->value() );
  defoRecoSurface_.setSearchPathHalfWidth( surfaceRecoSearchpathSpinbox_->value() );
  defoRecoSurface_.setNominalGridDistance( geometryLgSpinbox_->value() );
  defoRecoSurface_.setNominalCameraDistance( geometryLcSpinbox_->value() );
  defoRecoSurface_.setNominalViewingAngle( geometryDeltaSpinbox_->value() );
  defoRecoSurface_.setPitchX( geometryPitchSpinbox1_->value() );
  defoRecoSurface_.setPitchY( geometryPitchSpinbox2_->value() );
  defoRecoSurface_.setFocalLength( geometryFSpinbox_->value() );

  // chiller comes later..


  // camera
  if( isCameraEnabled_ ) {

    // load values from advanced tab comboboxes to cfg object
    EOS550D::EOS550DConfig cfg;
    cfg.shutterSpeed_ = static_cast<EOS550D::ShutterSpeed>( exptimeComboBox_->currentIndex() );
    cfg.aperture_ = static_cast<EOS550D::Aperture>( apertureComboBox_->currentIndex() );
    cfg.iso_ = static_cast<EOS550D::Iso>( isoComboBox_->currentIndex() );

    // send it
    camHandler_.setAction( DefoCamHandler::SETCFG );
    camHandler_.setCfg( cfg );
    camHandler_.start();

    this->setCursor( Qt::BusyCursor );

  }

}



///
///
///
void DefoMainWindow::writePointsToFile( std::vector<DefoPoint> const& points, QString const& filepath ) const {

  std::ofstream file( filepath.toStdString().c_str() );
  if( file.bad() ) {
    std::cerr << " [DefoMainWindow::writePointsToFile] ** ERROR: cannot open file: \"" << filepath.toStdString() << "\"." << std::endl;
    return;
  }

  for( DefoPointCollection::const_iterator it = points.begin(); it < points.end(); ++it ) {
    file << "POINT  x: " << std::setw( 8 ) << it->getX() 
	 << " y: " << std::setw( 8 ) << it->getY()
	 << " x-index: " << std::setw( 3 ) << it->getIndex().first
	 << " y-index: " << std::setw( 3 ) << it->getIndex().second
	 << std::setw( 8 ) << (it->isBlue()?" BLUE":" WHITE") << std::endl;
  }

}



///
///
///
void DefoMainWindow::initLightPanelStates( std::string const& stateString ) {

  if( stateString.size() != 5 ) {
    std::cerr << " [DefoMainWindow::initLightPanelStates] ** ERROR: argument to cfg parameter PANEL_STATE_WHEN_START has size: "
	      << stateString.size() << ". All panels off." << std::endl;
    for( unsigned int i = 0; i < 5; ++i ) lightPanelStates_.at( i ) = false;
    return;
  }
  
  for( unsigned int i = 0; i < stateString.size(); ++i ) {
    
    if( stateString.at( i ) == '0' ) lightPanelStates_.at( i ) = false;
    else if( stateString.at( i ) == '1' ) lightPanelStates_.at( i ) = true;
    else {
      std::cerr << " [DefoMainWindow::initLightPanelStates] ** WARNING: bogus character: \"" << stateString.at( i )
		<< "\" as argument to cfg parameter PANEL_STATE_WHEN_START, will interpret this as \"off\"." << std::endl;
      lightPanelStates_.at( i ) = false;
    }
    
  }
  
}



///
///
///
void DefoMainWindow::panelButton1Clicked( void ) {
  
  lightPanelStates_.at( 0 ) = !lightPanelStates_.at( 0 );
  if( lightPanelStates_.at( 0 ) ) lightPanelsButtons_.at( 0 )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
  else lightPanelsButtons_.at( 0 )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
  
  // SWITCH CONRAD (later)

}



///
///
///
void DefoMainWindow::panelButton2Clicked( void ) {

  lightPanelStates_.at( 1 ) = !lightPanelStates_.at( 1 );
  if( lightPanelStates_.at( 1 ) ) lightPanelsButtons_.at( 1 )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
  else lightPanelsButtons_.at( 1 )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
  
  // SWITCH CONRAD (later)

}



///
///
///
void DefoMainWindow::panelButton3Clicked( void ) {

  lightPanelStates_.at( 2 ) = !lightPanelStates_.at( 2 );
  if( lightPanelStates_.at( 2 ) ) lightPanelsButtons_.at( 2 )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
  else lightPanelsButtons_.at( 2 )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
  
  // SWITCH CONRAD (later)

}



///
///
///
void DefoMainWindow::panelButton4Clicked( void ) {

  lightPanelStates_.at( 3 ) = !lightPanelStates_.at( 3 );
  if( lightPanelStates_.at( 3 ) ) lightPanelsButtons_.at( 3 )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
  else lightPanelsButtons_.at( 3 )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
  
  // SWITCH CONRAD (later)

}



///
///
///
void DefoMainWindow::panelButton5Clicked( void ) {

  lightPanelStates_.at( 4 ) = !lightPanelStates_.at( 4 );
  if( lightPanelStates_.at( 4 ) ) lightPanelsButtons_.at( 4 )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
  else lightPanelsButtons_.at( 4 )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
  
  // SWITCH CONRAD (later)

}



///
///
///
void DefoMainWindow::allPanelsOff( void ) {

  for( unsigned int i = 0; i < 5; ++i ) {
    lightPanelStates_.at( i ) = false;
    lightPanelsButtons_.at( i )->setStyleSheet("background-color: rgb(0, 0, 0); color: rgb(255, 255, 255)");
    // SWITCH CONRAD
  }

}



///
///
///
void DefoMainWindow::allPanelsOn( void ) {

    for( unsigned int i = 0; i < 5; ++i ) {
    lightPanelStates_.at( i ) = true;
    lightPanelsButtons_.at( i )->setStyleSheet("background-color: rgb(252, 250, 210); color: rgb(0, 0, 0)");
    // SWITCH CONRAD
  }

}



///
///
///
DefoSleepTimer::DefoSleepTimer() { 

  connect( this, SIGNAL(timeout()), this, SLOT(reset()) );
  isRunning_ = false; 

  DefoConfigReader cfgReader( "defo.cfg" );
  debugLevel_ = cfgReader.getValue<unsigned int>( "DEBUG_LEVEL" ); // for messaging only

}



///
///
///
void DefoSleepTimer::run( void ) {

  setSingleShot( true );
  totalRemainingMSec_ = interval();
  time_.start();
  isRunning_ = true;
  start();

}



///
///
///
void DefoSleepTimer::halt( void ) {

  isRunning_ = false;
  stop();

}



///
///
///
void DefoSleepTimer::pause( void ) {

  totalRemainingMSec_ -= time_.elapsed();
  stop();

}



///
///
///
void DefoSleepTimer::resume( void ) {

  setSingleShot( true );
  setInterval( totalRemainingMSec_ );
  if( debugLevel_ >= 3 ) std::cout << " [DefoSleepTimer::resume] =3= resume with remaining msec: " << totalRemainingMSec_ << std::endl;
  time_.start();
  isRunning_ = true;
  start();

}



///
///
///
void DefoSleepTimer::reset( void ) {

  if( debugLevel_ >= 3 ) std::cout << " [DefoSleepTimer::reset] =3= sleep timer reset." << std::endl;
  isRunning_ = false;

}

