#include <sl/Camera.hpp>
#include "aruco.hpp"
#include <opencv2/opencv.hpp>
#include <fstream>

using namespace sl;
using namespace std;

bool isTagValidForReset(const vector<cv::Point2f> &corners, const cv::Size &image_size, float ratio = 0.05) {
  float image_area = image_size.width * image_size.height;

  double side1 = cv::norm(corners[1] - corners[0]);
  double side2 = cv::norm(corners[2] -corners[1]);

  float tag_area = side1 * side2;

  auto size_ratio = tag_area / image_area;

  return size_ratio > ratio;
}

int main(int argc, char **argv)
{
  Camera zed;
  InitParameters init_params;
  init_params.camera_resolution = RESOLUTION::HD2K;
  init_params.coordinate_units = UNIT::METER;
  init_params.sensors_required = false;
  init_params.coordinate_system = sl::COORDINATE_SYSTEM::LEFT_HANDED_Y_UP;
  init_params.svo_real_time_mode = false;
  init_params.camera_image_flip = sl::FLIP_MODE::AUTO;
  init_params.depth_mode = sl::DEPTH_MODE::NEURAL;
  ERROR_CODE err = zed.open(init_params);
  if (err != ERROR_CODE::SUCCESS)
  {
    cerr << "Error, unable to open ZED camera: " << sl::toString(err).c_str() << "\n";
    zed.close();
    return EXIT_FAILURE; // Quit if an error occurred
  }

  auto cameraInfo = zed.getCameraInformation();
  Resolution image_size = cameraInfo.camera_configuration.resolution;
  Mat image_zed(image_size, MAT_TYPE::U8_C4);
  cv::Mat image_ocv = cv::Mat(image_zed.getHeight(), image_zed.getWidth(), CV_8UC4, image_zed.getPtr<sl::uchar1>(MEM::CPU));
  cv::Mat image_ocv_rgb;

  auto calibInfo = cameraInfo.camera_configuration.calibration_parameters.left_cam;
  cv::Matx33d camera_matrix = cv::Matx33d::eye();
  camera_matrix(0, 0) = calibInfo.fx;
  camera_matrix(1, 1) = calibInfo.fy;
  camera_matrix(0, 2) = calibInfo.cx;
  camera_matrix(1, 2) = calibInfo.cy;

  cv::Matx<float, 4, 1> dist_coeffs = cv::Vec4f::zeros();

  float actual_marker_size_meters = 0.165; // real marker size in meters -> IT'S IMPORTANT THAT THIS VARIABLE CONTAINS THE CORRECT SIZE OF THE TARGET

  auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_100);

  cout << "Make sure the ArUco marker is a 6x6 (100), measuring " << actual_marker_size_meters * 1000 << " mm" << endl;

  Transform pose;
  Pose zed_pose;
  vector<cv::Vec3d> rvecs, tvecs;
  vector<int> ids;
  vector<vector<cv::Point2f>> corners;
  string position_txt;

  float auto_reset_aruco_screen_ratio = 0.01;
  int last_aruco_reset = -1;

  bool can_reset = false;

  PositionalTrackingParameters positional_tracking_param;  
  positional_tracking_param.enable_imu_fusion = true;
  positional_tracking_param.mode = sl::POSITIONAL_TRACKING_MODE::GEN_2;
  positional_tracking_param.enable_area_memory = true;
  auto returned_state = zed.enablePositionalTracking(positional_tracking_param);
  if (returned_state != ERROR_CODE::SUCCESS) {
      std::cout << "Enabling positional tracking failed: " << returned_state << std::endl;
      zed.close();
      return EXIT_FAILURE;
  }

  bool has_reset = false;

  sl::Transform ARUCO_TO_IMAGE_basis_change;
  ARUCO_TO_IMAGE_basis_change.r00 = -1;
  ARUCO_TO_IMAGE_basis_change.r11 = -1;
  sl::Transform IMAGE_TO_ARUCO_basis_change;
  IMAGE_TO_ARUCO_basis_change = sl::Transform::inverse(ARUCO_TO_IMAGE_basis_change);

  POSITIONAL_TRACKING_STATE tracking_state;


  while (true)
  {
    if (zed.grab() == ERROR_CODE::SUCCESS) {
      zed.retrieveImage(image_zed, VIEW::LEFT, MEM::CPU, image_size);
      cv::cvtColor(image_ocv, image_ocv_rgb, cv::COLOR_BGRA2BGR);
      cv::Mat grayImage;
      cv::cvtColor(image_ocv_rgb, grayImage, cv::COLOR_BGR2GRAY);
      cv::aruco::detectMarkers(image_ocv_rgb, dictionary, corners, ids);

      for (size_t i = 0; i < corners.size(); ++i) {
        cv::cornerSubPix(grayImage, corners[i], cv::Size(5, 5), cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));
      }
      tracking_state = zed.getPosition(zed_pose);
      position_txt = "ZED  x: " + to_string(zed_pose.pose_data.tx + 1.35) +
                     "; y: " + to_string(zed_pose.pose_data.ty) +
                     "; z: " + to_string(zed_pose.pose_data.tz);
      position_txt =
          position_txt + " rx " +
          to_string(zed_pose.pose_data.getEulerAngles(false).x) +
          " ry : " + to_string(zed_pose.pose_data.getEulerAngles(false).y) +
          " rz : " + to_string(zed_pose.pose_data.getEulerAngles(false).z);
      std::cout << position_txt << std::endl;

      // if at least one marker detected
      if (ids.size() > 0)
      {

        cv::aruco::estimatePoseSingleMarkers(corners, actual_marker_size_meters,
                                             camera_matrix, dist_coeffs, rvecs,
                                             tvecs);

        pose.setTranslation(sl::float3(tvecs[0](0), tvecs[0](1), tvecs[0](2)));
        pose.setRotationVector(
            sl::float3(rvecs[0](0), rvecs[0](1), rvecs[0](2)));

        pose = IMAGE_TO_ARUCO_basis_change * pose;

        pose.inverse();
        auto user_coordinate_to_image = sl::getCoordinateTransformConversion4f(
            init_params.coordinate_system, sl::COORDINATE_SYSTEM::IMAGE);
        can_reset = true;

        sl::Transform user_coordinate_to_ARUCO =
            IMAGE_TO_ARUCO_basis_change * user_coordinate_to_image;
        sl::Transform ARUCO_to_user_coordinate =
            sl::Transform::inverse(user_coordinate_to_ARUCO);

        pose = ARUCO_to_user_coordinate * pose * user_coordinate_to_ARUCO;
      }
      else
        can_reset = false;

      if (ids.size() == 0) {
        rvecs.clear();
        tvecs.clear();
        rvecs.resize(1);
        tvecs.resize(1);
      }
      auto transform = pose;

      transform.inverse();

      auto user_coordinate_to_image = sl::getCoordinateTransformConversion4f(
          init_params.coordinate_system, sl::COORDINATE_SYSTEM::IMAGE);
      transform = user_coordinate_to_image * transform;

      sl::float3 rotation = transform.getRotationVector();
      rvecs[0](0) = rotation.x;
      rvecs[0](1) = rotation.y;
      rvecs[0](2) = rotation.z;
      tvecs[0](0) = transform.tx;
      tvecs[0](1) = transform.ty;
      tvecs[0](2) = transform.tz;
      if (!ids.empty() && can_reset && last_aruco_reset != ids[0]) {
        bool resetPose = isTagValidForReset(corners[0], cv::Size(image_zed.getWidth(), image_zed.getHeight()), auto_reset_aruco_screen_ratio);
        if (resetPose) {
          std::cout << "Auto Pose Reset!" << std::endl;
          zed.resetPositionalTracking(pose);
          has_reset = true;
          last_aruco_reset = ids[0];
        }
      }

      auto aruco_t = pose.getTranslation();
      pose.setTranslation(sl::Translation(aruco_t.x, aruco_t.y, aruco_t.z));
    }
  }
  zed.close();
  return 0;
}
