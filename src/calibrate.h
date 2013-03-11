#ifndef _H_PORTAL_CALIBRATION_CALIBRATE_
#define _H_PORTAL_CALIBRATION_CALIBRATE_

// Standard C++ includes
#include <stdio.h>
#include <stdlib.h>

// OpenCV includes
#include <cv.h>
#include <highgui.h>

// Camera Library includes
#include <lens\ICamera.h>

class Calibrate
{

public:
	void calibrateChessboard(shared_ptr<lens::ICamera> capture, int board_w, int board_h, int number_of_views);

private:
		// Returns the number of successful captures
	int grabViews(shared_ptr<lens::ICamera> capture, int board_n, CvSize board_sz, int n_boards, shared_ptr<CvMat> object_points, shared_ptr<CvMat> image_points, shared_ptr<CvMat> point_counts, shared_ptr<CvPoint2D32f> corners );
	void camCalibrate(shared_ptr<lens::ICamera> capture, shared_ptr<CvMat> intrinsic_matrix, shared_ptr<CvMat> distortion_coeffs, int board_n, shared_ptr<CvMat> object_points, shared_ptr<CvMat> image_points, shared_ptr<CvMat> point_counts, int sucesses);
	void unDistort(shared_ptr<lens::ICamera> capture, shared_ptr<CvMat> distortion_coeffs, shared_ptr<CvMat> intrinsic_matrix);
	void saveCalibrationData(shared_ptr<CvMat> distortion_coeffs, shared_ptr<CvMat> intrinsic_matrix); 
};

#endif // _H_PORTAL_CALIBRATION_CALIBRATE_