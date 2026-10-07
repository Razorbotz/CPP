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
#include <iomanip>

// Check if one bounding box is inside another
bool is_inside(const cv::Rect& inner, const cv::Rect& outer) {
    return (outer.x <= inner.x && outer.y <= inner.y &&
            outer.x + outer.width >= inner.x + inner.width &&
            outer.y + outer.height >= inner.y + inner.height);
}

// Calculate distance from a point to a plane
float distance_to_plane(float x, float y, float z, const pcl::ModelCoefficients::Ptr& coeffs) {
    float A = coeffs->values[0];
    float B = coeffs->values[1];
    float C = coeffs->values[2];
    float D = coeffs->values[3];
    return std::abs(A * x + B * y + C * z + D) / std::sqrt(A * A + B * B + C * C);
}

int main() {
    float height_threshold = 0.10f;  // Deviation threshold (meters)
    int min_area = 900;              // Minimum area to be considered a valid object
    int kernelSize = 25;              // Morphological kernel size for closing

    rs2::pipeline pipe;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);
    pipe.start(cfg);

    rs2::pointcloud pc;
    rs2::frameset frames = pipe.wait_for_frames();
    rs2::depth_frame depth = frames.get_depth_frame();
    auto intrin = depth.get_profile().as<rs2::video_stream_profile>().get_intrinsics();

    cv::namedWindow("Deviation Highlight", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Detected Objects", cv::WINDOW_AUTOSIZE);

    while (true) {
        frames = pipe.wait_for_frames();
        depth = frames.get_depth_frame();
        rs2::points points = pc.calculate(depth);
        const rs2::vertex* verts = points.get_vertices();
        int width = depth.get_width();
        int height = depth.get_height();

        // Convert to PCL cloud
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
        cloud->points.reserve(points.size());
        for (int i = 0; i < points.size(); ++i) {
            if (std::isnan(verts[i].z) || verts[i].z <= 0.0f)
                continue;
            cloud->points.emplace_back(verts[i].x, verts[i].y, verts[i].z);
        }

        // RANSAC Plane Segmentation
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

        cv::Mat deviation_mask(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::Mat object_view(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::Mat binary_mask(height, width, CV_8U, cv::Scalar(0));

        // Compute binary deviation mask
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float dist = depth.get_distance(x, y);
                if (dist <= 0.0f || dist > 5.0f) continue;

                float pixel[2] = { (float)x, (float)y };
                float point[3];
                rs2_deproject_pixel_to_point(point, &intrin, pixel, dist);

                float d_plane = distance_to_plane(point[0], point[1], point[2], coeffs);
                if (d_plane > height_threshold) {
                    deviation_mask.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 255); // Red
                    binary_mask.at<uchar>(y, x) = 255;
                }
            }
        }

        // Apply morphological closing to merge close regions
        cv::Mat closed_mask;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                   cv::Size(kernelSize, kernelSize));
        cv::morphologyEx(binary_mask, closed_mask, cv::MORPH_CLOSE, kernel);

        // Find contours in closed binary mask
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(closed_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::vector<cv::Rect> boxes;
        for (const auto& contour : contours) {
            if (cv::contourArea(contour) < min_area) continue;
            boxes.push_back(cv::boundingRect(contour));
        }

        // Filter out nested boxes
        std::vector<cv::Rect> filtered_boxes;
        for (size_t i = 0; i < boxes.size(); ++i) {
            bool is_nested = false;
            for (size_t j = 0; j < boxes.size(); ++j) {
                if (i != j && is_inside(boxes[i], boxes[j])) {
                    is_nested = true;
                    break;
                }
            }
            if (!is_nested) filtered_boxes.push_back(boxes[i]);
        }

        // Draw and annotate boxes
        for (const auto& bbox : filtered_boxes) {
            cv::rectangle(object_view, bbox, cv::Scalar(0, 255, 0), 2);

            int cx = bbox.x + bbox.width / 2;
            int cy = bbox.y + bbox.height / 2;

            float dist = depth.get_distance(cx, cy);
            if (dist > 0.0f && dist < 5.0f) {
                float pixel[2] = { (float)cx, (float)cy };
                float point[3];
                rs2_deproject_pixel_to_point(point, &intrin, pixel, dist);

                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2)
                   << "x: " << point[0] << " y: " << point[1] << "\nz: " << point[2];

                cv::putText(object_view, ss.str(), cv::Point(bbox.x, bbox.y - 10),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
            }
        }

        cv::imshow("Deviation Highlight", deviation_mask);
        cv::imshow("Detected Objects", object_view);

        if (cv::waitKey(1) == 27) break; // ESC to exit
    }

    return 0;
}

