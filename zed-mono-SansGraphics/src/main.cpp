///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2020, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

/***********************************************************************************************
 ** This sample demonstrates how to reloc a ZED camera using an ArUco marker.                  **
 ** Images are captured with the ZED SDK and cameras poses is then computed from ArUco pattern **
 ** to reset ZED tracking with this known position.                                            **
 ***********************************************************************************************/

// ZED includes
#include <sl/Camera.hpp>

// Sample includes
#include "aruco.hpp"

// OCV includes
#include <opencv2/opencv.hpp>

using namespace sl;
using namespace std;

int main(int argc, char **argv) {

    // Create a ZED camera object
    Camera zed;

    // Set configuration parameters
    InitParameters init_params;
//    init_params.camera_resolution = RESOLUTION::HD720;
    init_params.camera_resolution = RESOLUTION::VGA;
    init_params.coordinate_units = UNIT::METER;
    init_params.coordinate_system = COORDINATE_SYSTEM::RIGHT_HANDED_Y_UP;
    init_params.sensors_required = false;

    float deltaX = 0;
    float deltaY = 0;
    float deltaZ = 0;
    float zedX = 0;
    float zedY = 0;
    float zedZ = 0;

    // Open the camera
    ERROR_CODE err = zed.open(init_params);
    if (err != ERROR_CODE::SUCCESS) {
        cout << "Error, unable to open ZED camera: " << err << "\n";
        zed.close();
        return 1; // Quit if an error occurred
    }

    auto cameraInfo = zed.getCameraInformation();
    Resolution image_size = cameraInfo.camera_resolution;
    Mat image_zed(image_size, MAT_TYPE::U8_C4);
    cv::Mat image_ocv = cv::Mat(image_zed.getHeight(), image_zed.getWidth(), CV_8UC4, image_zed.getPtr<sl::uchar1>(MEM::CPU));
    cv::Mat image_ocv_rgb;

    auto calibInfo = cameraInfo.calibration_parameters.left_cam;
    cv::Matx33d camera_matrix = cv::Matx33d::eye();
    camera_matrix(0, 0) = calibInfo.fx;
    camera_matrix(1, 1) = calibInfo.fy;
    camera_matrix(0, 2) = calibInfo.cx;
    camera_matrix(1, 2) = calibInfo.cy;

    cv::Matx<float, 4, 1> dist_coeffs = cv::Vec4f::zeros();

    float actual_marker_size_meters = 0.16f; // real marker size in meters
    auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_100);

    cout << "Make sure the ArUco marker is a 6x6 (100), measuring " << actual_marker_size_meters * 1000 << " mm" << endl;

    Transform pose;
    Pose zed_pose;
    vector<cv::Vec3d> rvecs, tvecs;
    vector<int> ids;
    vector<vector<cv::Point2f> > corners;
    string zed_position_txt;
    string zed_rotation_txt;
    string aruco_position_txt;

    bool can_reset = false;

    PositionalTrackingParameters tracking_params;
    tracking_params.enable_imu_fusion = false; // for this sample, IMU (of ZED-M) is disable, we use the gravity given by the marker.
    tracking_params.enable_area_memory = true;
    auto returned_state = zed.enablePositionalTracking(tracking_params);
    if (returned_state != ERROR_CODE::SUCCESS) {
        zed.close();
        return EXIT_FAILURE;
    }
    POSITIONAL_TRACKING_STATE tracking_state;

    // Loop until 'q' is pressed
    while (true) {
        if (zed.grab() == ERROR_CODE::SUCCESS) {
            // Retrieve the left image
            zed.retrieveImage(image_zed, VIEW::LEFT, MEM::CPU, image_size);

            // convert to RGB
            cv::cvtColor(image_ocv, image_ocv_rgb, cv::COLOR_RGBA2RGB);
            // detect marker
            cv::aruco::detectMarkers(image_ocv_rgb, dictionary, corners, ids);

            // get actual ZED position
            tracking_state = zed.getPosition(zed_pose, REFERENCE_FRAME::WORLD);
	    if (tracking_state == POSITIONAL_TRACKING_STATE::OK) {
	       zedX=zed_pose.pose_data.tx;
               zedY=zed_pose.pose_data.ty;	       
	       zedZ=zed_pose.pose_data.tz;
            // display ZED position
                zed_position_txt = "ZED  x: " + to_string(zed_pose.pose_data.tx) + "; y: " + to_string(zed_pose.pose_data.ty) + "; z: " + to_string(zed_pose.pose_data.tz);
                zed_rotation_txt = "ZED  a: " + to_string(zed_pose.getOrientation().ox) + "; b: " + to_string(zed_pose.getOrientation().oy) + "; c: " + to_string(zed_pose.getOrientation().oz);
	    }else{
		 zed_position_txt = "";
		 zed_rotation_txt = "";
	    }

            // if at least one marker detected
            if (ids.size() > 0) {
                cv::aruco::estimatePoseSingleMarkers(corners, actual_marker_size_meters, camera_matrix, dist_coeffs, rvecs, tvecs);
                pose.setTranslation(sl::float3(tvecs[0](0), tvecs[0](1), tvecs[0](2)));
                pose.setRotationVector(sl::float3(rvecs[0](0), rvecs[0](1), rvecs[0](2)));
                pose.inverse();
                can_reset = true;

		deltaX = pose.tx - zedX;
		deltaY = pose.ty - zedY;
		deltaZ = pose.tz - zedZ;

                aruco_position_txt = "Aruco x: " + to_string(pose.tx) + "; y: " + to_string(pose.ty) + "; z: " + to_string(pose.tz);

            } else{
		aruco_position_txt = "";    
                can_reset = false;
	    }
	    zedX += deltaX;
	    zedY += deltaY;
	    zedZ += deltaZ;

	    std::cout << "ZED: x:" << zedX << " y: " << zedY << " z:" << zedZ << "   " << aruco_position_txt << std::endl;

            //zed.resetPositionalTracking(pose);
        }
    }
    zed.close();
    return 0;

}
