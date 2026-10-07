#include <librealsense2/rs.hpp>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>

// Function to calculate distance of a point from the plane
float distance_to_plane(float x, float y, float z, const pcl::ModelCoefficients::Ptr& coeffs) {
    float A = coeffs->values[0];
    float B = coeffs->values[1];
    float C = coeffs->values[2];
    float D = coeffs->values[3];
    return std::abs(A * x + B * y + C * z + D) / std::sqrt(A * A + B * B + C * C);
}

int main() {
    float height_threshold = 0.10f;  // Height threshold in meters (10 cm)

    rs2::pipeline pipe;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);
    pipe.start(cfg);

    rs2::pointcloud pc;

    rs2::frameset frames = pipe.wait_for_frames();
    rs2::depth_frame depth = frames.get_depth_frame();
    auto intrin = depth.get_profile().as<rs2::video_stream_profile>().get_intrinsics();

    cv::namedWindow("Ground Plane Deviation", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Red Highlight", cv::WINDOW_AUTOSIZE);

    while (true) {
        frames = pipe.wait_for_frames();
        depth = frames.get_depth_frame();

        rs2::points points = pc.calculate(depth);
        const rs2::vertex* verts = points.get_vertices();
        int width = depth.get_width();
        int height = depth.get_height();

        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
        cloud->points.reserve(points.size());
        for (int i = 0; i < points.size(); ++i) {
            if (std::isnan(verts[i].z) || verts[i].z <= 0.0f)
                continue;
            cloud->points.emplace_back(verts[i].x, verts[i].y, verts[i].z);
        }

        pcl::SACSegmentation<pcl::PointXYZ> seg;
        pcl::ModelCoefficients::Ptr coeffs(new pcl::ModelCoefficients);
        pcl::PointIndices::Ptr inliers(new pcl::PointIndices);

        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_PLANE);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setDistanceThreshold(0.01);
        seg.setInputCloud(cloud);
        seg.segment(*inliers, *coeffs);

        if (inliers->indices.empty()) {
            std::cerr << "No plane found in this frame." << std::endl;
            continue;
        }

        // Create deviation heatmap and highlight image
        cv::Mat deviation_map(height, width, CV_32F, cv::Scalar(0));
        cv::Mat highlight_image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float dist = depth.get_distance(x, y);
                if (dist <= 0.0f || dist > 5.0f) continue;

                float point[3];
                float pixel[2] = { (float)x, (float)y };
                rs2_deproject_pixel_to_point(point, &intrin, pixel, dist);

                float d_plane = distance_to_plane(point[0], point[1], point[2], coeffs);
                deviation_map.at<float>(y, x) = d_plane;

                if (d_plane > height_threshold) {
                    highlight_image.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 255);  // Red
                }
            }
        }

        cv::Mat norm_map, heatmap;
        cv::normalize(deviation_map, norm_map, 0, 1, cv::NORM_MINMAX);
        norm_map.convertTo(norm_map, CV_8U, 255.0);
        cv::applyColorMap(norm_map, heatmap, cv::COLORMAP_JET);

        cv::imshow("Ground Plane Deviation", heatmap);
        cv::imshow("Red Highlight", highlight_image);

        int key = cv::waitKey(1);
        if (key == 27) break;  // ESC to quit
    }

    return 0;
}

