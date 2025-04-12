#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

rclcpp::Node::SharedPtr nodeHandle;


/** @brief Function to get the value of the specified parameter
 * 
 * Function that takes a string as a parameter containing the
 * name of the parameter that is being parsed from the launch
 * file and the initial value of the parameter as inputs, then
 * gets the parameter, casts it as the desired type, displays 
 * the value of the parameter on the command line and the log 
 * file, then returns the parsed value of the parameter.
 * @param parametername String of the name of the parameter
 * @param initialValue Initial value of the parameter
 * @return value Value of the parameter
 * */
template <typename T>
T getParameter(std::string parameterName, int initialValue){
	nodeHandle->declare_parameter<T>(parameterName, initialValue);
	rclcpp::Parameter param = nodeHandle->get_parameter(parameterName);
	T value;
	if(typeid(value).name() == typeid(int).name())
		value = param.as_int();
	if(typeid(value).name() == typeid(double).name())
		value = param.as_double();
	if(typeid(value).name() == typeid(bool).name())
		value = param.as_bool();
	std::cout << parameterName << ": " << value << std::endl;
	std::string output = parameterName + ": " + std::to_string(value);
	RCLCPP_INFO(nodeHandle->get_logger(), output.c_str());
	return value;
}


void image_callback(const sensor_msgs::msg::Image::SharedPtr msg){
    try{
        cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);

        cv::imshow("Image Viewer", cv_ptr->image);
        cv::waitKey(1);
    }
    catch (const cv_bridge::Exception &e)
    {
        RCLCPP_ERROR(this->get_logger(), "CV Bridge exception: %s", e.what());
    }
}

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);

    nodeHandle = rclcpp::Node::make_shared("video_stream");
    auto zedImageSubscriber = nodeHandle->create_subscription<sensor_msgs::msg::Image>("zed_image",1,image_callback);
    
    int window_width = getParameter<int>("window_width", 100);
    int window_height = getParameter<int>("window_height", 100);
    int window_x = getParameter<int>("window_x", 100);
    int window_y = getParameter<int>("window_y", 100);

    cv::namedWindow("Image Display", cv::WINDOW_NORMAL);
    cv::resizeWindow("Image Display", window_width, window_height);
    cv::moveWindow("Image Display", window_x, window_y);
    
    rclcpp::spin(nodeHandle);

    rclcpp::shutdown();
    return 0;
}

