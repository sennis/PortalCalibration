#include "CalibrationEngine.h"

CalibrationEngine::CalibrationEngine(const int horizontalCount, const int verticalCount) :
  m_boardSize( horizontalCount, verticalCount ),
  m_boardMarkerCount( horizontalCount * verticalCount ),
  m_markerDiameter( .5 )
{ }

void CalibrationEngine::CalibrateCamera(shared_ptr<lens::ICamera> capture, int requestedSamples)
{
  // Grab views and place them in the matrixes
  auto objectPoints = CalculateObjectPoints( ); 
  auto imagePoints = GrabCameraImagePoints(capture, requestedSamples);

  // Calibrate for intrinsics
  auto calibrationData = CalibrateView( objectPoints, imagePoints, cv::Size( capture->getWidth( ), capture->getHeight( ) ) );
  
  // Calibrate for extrinsics
  imagePoints = GrabCameraImagePoints(capture, 1); // Only use 1 since we are capturing for 1 view
  CalibrateExtrinsic( objectPoints, imagePoints, calibrationData );
}

void CalibrationEngine::CalibrateProjector(shared_ptr<lens::ICamera> capture, int requestedSamples)
{
  // Grab views for the projector
  auto objectPoints = CalculateObjectPoints( );
  auto imagePoints = GrabProjectorImagePoints( capture, requestedSamples );

  // Calibrate for projector intrinsics
  auto calibrationData = CalibrateView( objectPoints, imagePoints, cv::Size( capture->getWidth(), capture->getHeight( ) ) );

  // Calibrate for extrinsics
  imagePoints = GrabProjectorImagePoints( capture, 1 ); // Only using 1 since we are capturing for 1 view
  CalibrateExtrinsic( objectPoints, imagePoints, calibrationData );
}

vector<vector<cv::Point2f>> CalibrationEngine::GrabCameraImagePoints( shared_ptr<lens::ICamera> capture, int poses2Capture )
{
	int successes = 0;
	bool found = false;
	vector< vector< cv::Point2f > > imagePoints;
	vector< cv::Point2f > pointBuffer;

	// Create a display to give the user some feedback
	Display display("Calibration");
		
	// While we have boards to grab, grab-em
	while ( successes < poses2Capture )
	{
	  // Let the user know how many more images we need and how to capture
	  std::stringstream message;
	  message << "Press <Enter> to capture pose \n";
	  message << successes;
	  message << "/";
	  message << poses2Capture;
	  display.OverlayText( message.str() );

	  while ( m_userWaitKey != cvWaitKey( 15 ) )
	  {
		// Just display to the user. They are setting up the calibration board
		cv::Mat frame( capture->getFrame() );
		cv::drawChessboardCorners( frame, m_boardSize, cv::Mat( pointBuffer ), found );
		display.ShowImage( frame );
	  }

	  // User is ready, try and find the circles
	  pointBuffer.clear();
	  cv::Mat colorFrame( capture->getFrame( ) );
	  cv::Mat gray;
	  cv::cvtColor( colorFrame, gray, CV_BGR2GRAY);
	  found = cv::findCirclesGrid( gray, m_boardSize, pointBuffer, cv::CALIB_CB_ASYMMETRIC_GRID );

	  // Make sure we found it, and that we found all the points
	  if(found && pointBuffer.size() == m_boardMarkerCount)
	  {
		imagePoints.push_back(pointBuffer);
		++successes;
	  }
	} // End collection while loop

	return imagePoints;
}

vector<vector<cv::Point2f>> CalibrationEngine::GrabProjectorImagePoints(shared_ptr<lens::ICamera> capture, int poses2Capture )
{
  	int successes = 0;
	bool found = false;
	vector< vector< cv::Point2f > > imagePoints;
	vector< cv::Point2f > pointBuffer;

	// Create a display to give the user some feedback
	Display display("Calibration");

	// While we have boards to grab, grab-em
	while ( successes < poses2Capture )
	{
	  // Let the user know how many more images we need and how to capture
	  std::stringstream message;
	  message << "Press <Enter> to capture pose \n";
	  message << successes;
	  message << "/";
	  message << poses2Capture;
	  display.OverlayText( message.str() );

	  while ( m_userWaitKey != cvWaitKey( 15 ) )
	  {
		// Just display to the user. They are setting up the calibration board
		cv::Mat frame( capture->getFrame() );
		cv::drawChessboardCorners( frame, m_boardSize, cv::Mat( pointBuffer ), found );
		display.ShowImage( frame );
	  }

	  // User is ready, try and find the circles
	  pointBuffer.clear();

	  // TODO - make the projector project a white image
	  cv::Mat colorFrame( capture->getFrame( ) );
	  cv::Mat gray;
	  cv::cvtColor( colorFrame, gray, CV_BGR2GRAY);
	  found = cv::findCirclesGrid( gray, m_boardSize, pointBuffer, cv::CALIB_CB_ASYMMETRIC_GRID );

	  // Make sure we found it, and that we found all the points
	  if(found && pointBuffer.size() == m_boardMarkerCount)
	  {
		// We found all the markers in the camera view. Now we need to image with the projector
		vector<cv::Mat> wrappedPhase;
		NFringeStructuredLight fringeGenerator(5);
		TwoWavelengthPhaseUnwrapper phaseUnwrapper;

		// Horizontal set --------------------------
		auto smallWavelength = fringeGenerator.GenerateFringe(gray.size(), 70, IStructuredLight::Horizontal);
		wrappedPhase.push_back( ProjectAndCaptureWrappedPhase( capture, smallWavelength ) );
		auto largerWavelength = fringeGenerator.GenerateFringe(gray.size(), 75, IStructuredLight::Horizontal);
		wrappedPhase.push_back( ProjectAndCaptureWrappedPhase( capture, largerWavelength ) );
		auto horizontalUnwrappedPhase = phaseUnwrapper.UnwrapPhase(wrappedPhase);
		
		// Vertical set ----------------------------
		smallWavelength = fringeGenerator.GenerateFringe(gray.size(), 70, IStructuredLight::Vertical);
		wrappedPhase.push_back( ProjectAndCaptureWrappedPhase( capture, smallWavelength ) );
		largerWavelength = fringeGenerator.GenerateFringe(gray.size(), 75, IStructuredLight::Vertical);
		wrappedPhase.push_back( ProjectAndCaptureWrappedPhase( capture, largerWavelength ) );
		auto verticalUnwrappedPhase = phaseUnwrapper.UnwrapPhase(wrappedPhase);

		vector< cv::Point2f > projectorPointBuffer;
		// TODO - interpolate projector pixels from phase

		imagePoints.push_back(projectorPointBuffer);
		++successes;
	  }
	} // End collection while loop

	return imagePoints;
}

cv::Mat CalibrationEngine::ProjectAndCaptureWrappedPhase(shared_ptr<lens::ICamera> capture, vector<cv::Mat> fringeImages)
{
  vector<cv::Mat> capturedFringes;
  cv::Mat gray;

  for(int patternNumber = 0; patternNumber < fringeImages.size(); ++patternNumber)
  {
	// projector.ProjectImage(fringeImages[i]);
	cv::Mat colorFringe( capture->getFrame( ) );
	cv::cvtColor( colorFringe, gray, CV_BGR2GRAY );
	capturedFringes.push_back( gray );
  }

  NFringeStructuredLight phaseWrapper(fringeImages.size()); 
  return phaseWrapper.WrapPhase(capturedFringes);
}

CalibrationData CalibrationEngine::CalibrateView(vector<cv::Point3f> objectPoints, vector<vector<cv::Point2f>> imagePoints, cv::Size viewSize)
{
  // Start with the identity and it will get refined from there
  cv::Mat distortionCoefficients = cv::Mat::zeros(5, 1, CV_64F);
  cv::Mat intrinsicMatrix = cv::Mat::eye(3, 3, CV_64F);
  vector<cv::Mat> rotationVectors;
  vector<cv::Mat> translationVectors;
  
  vector<vector<cv::Point3f>> objectPointList;
  for(int i = 0; i < imagePoints.size(); ++i)
	{ objectPointList.push_back(objectPoints); }

  cv::calibrateCamera(objectPointList, imagePoints, viewSize, intrinsicMatrix, distortionCoefficients, rotationVectors, translationVectors, CV_CALIB_FIX_K4 | CV_CALIB_FIX_K5);

  CalibrationData data;
  data.SetDistortion(distortionCoefficients);
  data.SetIntrinsic(intrinsicMatrix);
  
  return data;
}

void CalibrationEngine::CalibrateExtrinsic(vector<cv::Point3f> objectPoints, vector<vector<cv::Point2f>> imagePoints, CalibrationData& calibrationData)
{
  cv::Mat rotationVector;
  cv::Mat translationVector;

  cv::solvePnP(objectPoints, imagePoints, calibrationData.GetIntrinsic(), calibrationData.GetDistortion(), rotationVector, translationVector);
  calibrationData.SetRotationVector(rotationVector);
}

vector<cv::Point3f> CalibrationEngine::CalculateObjectPoints()
{
  vector<cv::Point3f> objectPoints;

  for( int row = 0; row < m_boardSize.height; ++row )
  {
	for( int col = 0; col < m_boardSize.width; ++col )
	{
	  objectPoints.push_back( cv::Point3f( float(2.0 * col + row % 2) * m_markerDiameter,
										   float(row * m_markerDiameter),
										   0.0f));
	}
  }

  return objectPoints;
}