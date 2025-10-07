#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <string> 
#include <vector>
#include <sys/types.h>
#include <sys/socket.h> 
//#include <cstdlib>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iostream>
//#include <fstream>
#include <fcntl.h>
#include <thread>
#include <list>
#include <chrono>
#include <cmath>

#include <glibmm/ustring.h>
#include <SDL2/SDL.h>
#include <gtkmm.h>
#include <gdkmm.h>
#include <gtkmm/window.h>
#include <webkit2/webkit2.h>
#include <cairomm/context.h>
#include <pangomm.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <unordered_set>
#include <algorithm>
#include <iomanip>
#include <fstream>

#include <cstdlib>
#include <opencv2/opencv.hpp>
#include <map>
#include <sstream>
#include <curl/curl.h>
#include <variant>
#include <regex>
#include <mutex>
#include <atomic>

#include "InfoFrame.hpp"
#include "BinaryMessage.hpp"

/*
TODO: 
Fix crash on video start
Fix random seg faults
Map Issues:
Cosmic map isn't drawing robot in correct location
Robot isn't drawing in correct location, need to offset for camera position
Random segfaults when connected to the robot
Random segfaults when robot starts ROS2

*/

#define PORT 31337 
#define VIDEO_PORT 31338
#define ORIN_IP "192.168.1.6"
#define NANO_IP "192.168.1.5"
bool useOrin = true;

float parseFloat(const uint8_t* array){
    uint32_t axisYInteger=0;
    axisYInteger|=uint32_t(array[0])<<24;
    axisYInteger|=uint32_t(array[1])<<16;
    axisYInteger|=uint32_t(array[2])<<8;
    axisYInteger|=uint32_t(array[3])<<0;
    float value=(float)*(static_cast<float*>(static_cast<void*>(&axisYInteger)));

    return value;
}

int parseInt(const uint8_t* array){
    uint32_t axisYInteger=0;
    axisYInteger|=uint32_t(array[0])<<24;
    axisYInteger|=uint32_t(array[1])<<16;
    axisYInteger|=uint32_t(array[2])<<8;
    axisYInteger|=uint32_t(array[3])<<0;
    int value=(int)*(static_cast<int*>(static_cast<void*>(&axisYInteger)));

    return value;
}

 
void insert(float value,uint8_t* array){
    array[0]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>24) & 0xff);
    array[1]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>16) & 0xff);
    array[2]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>8) & 0xff);
    array[3]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>0) & 0xff);
}


void insert(int value,uint8_t* array){
    array[0]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>24) & 0xff);
    array[1]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>16) & 0xff);
    array[2]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>8) & 0xff);
    array[3]=uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value))))>>0) & 0xff);
}


bool quit(GdkEventAny* event){
    exit(0);
}

Gtk::ListBox* addressListBox;
Gtk::Entry* ipAddressEntry;
Gtk::Label* connectionStatusLabel;
  
Gtk::Button* silentRunButton;
Gtk::Button* connectButton;
Gtk::Button* toggleModeButton;
Gtk::Button* settingsButton;

Gtk::ListBox* videoAddressListBox;
Gtk::Entry* videoIPAddressEntry;
Gtk::Label* videoConnectionStatusLabel;
  
Gtk::Button* videoStreamButton;
Gtk::Button* videoConnectButton;
bool isStreamingActive = false;
bool isGray = true;
std::mutex frameMutex;
cv::Mat latestFrame;
bool newFrameAvailable;
Glib::Dispatcher videoDisconnectDispatcher;
std::atomic<bool> shouldVideoDisconnect = false;
  
Gtk::FlowBox* sensorBox;
Gtk::Box* innerLeftBox;
Gtk::Box* innerRightBox;
Gtk::Box* bottomLowerBox;

Gtk::Window* window;
int sock = 0; 
bool connected=false;
bool silentRunning=true;
bool initialized = false;

bool initVals = false;
bool threeMonitors = false;
bool smallLaptop = false;
bool noVideo = false;
bool noArena = false;
std::string mapUsed = "NASA";
bool testInput = false;
bool useAltLayout = false;
bool isController = false;
bool twoJoysticks = false;

int videoSock = 0; 
bool videoConnected=false;

Gtk::Window* arenaWindow;
Gtk::Window* sensorsWindow;
Gtk::Window* configWindow;
Gtk::Window* motorWindow;
int monitor_count = 0;

std::string configFile = "config.txt";

enum class ElementType {
    UInt8, UInt16, Int8, Int32, Float32, Boolean, String
};

struct ElementInfo {
    ElementType type;
    std::string name;
};

std::string darkBackgroundColor = "#0b1a21";
std::string lightBackgroundColor = "#f0faf2";
bool isLightMode = true;


// To add a new key, add it to the vector that the key belongs to and 
// add it to the element_definitions below with the type of the element

// To add a new type, create a new vector of strings below. Initialize it
// in the initialize map function. Add it to the key_vectors. Create a 
// local keys and copy the values to it.  Create various boxes and add them
// to frame_entries. Create a new widget and add it to the options frame. 
// Save the new values to the vector from the local copy.
std::set<std::string> speedometer_keys = {
    "DISPLAY_SPEED",
    "NUMBERS_INSIDE",
    "NUMBER_TICKS"
};

std::vector<std::string> talon_keys = {
"Device ID", "Bus Voltage", "Output Current", "Output Percent",
"Temperature", "Sensor Position", "Sensor Velocity", "Max Current"
};
std::vector<std::string> reset_talon_keys = {
 "Device ID", "Bus Voltage", "Output Current", "Output Percent",
"Temperature", "Sensor Position", "Sensor Velocity", "Max Current"
};   

std::map<std::string, bool> talon_values;

std::vector<std::string> falcon_keys = talon_keys;
std::map<std::string, bool> falcon_values;
std::vector<std::string> reset_falcon_keys = reset_talon_keys;

std::vector<std::string> linear_keys = {
    "Motor Number", "Speed", "Potentiometer", "Time Without Change",
    "Max", "Min", "Error", "At Min", "At Max", "Distance", "Sensorless"
};
std::map<std::string, bool> linear_values;
std::vector<std::string> reset_linear_keys = {
    "Motor Number", "Speed", "Potentiometer", "Time Without Change",
    "Max", "Min", "Error", "At Min", "At Max", "Distance", "Sensorless"
};

std::vector<std::string> power_keys = {
    "Voltage", "Temp", "Current 0", "Current 1", "Current 2",
    "Current 3", "Current 4", "Current 5", "Current 6"
};
std::map<std::string, bool> power_values;
std::vector<std::string> reset_power_keys = {
    "Voltage", "Temp", "Current 0", "Current 1", "Current 2",
    "Current 3", "Current 4", "Current 5", "Current 6"
};

std::vector<std::string> power2_keys = {
    "Current 7", "Current 8", "Current 9", "Current 10", "Current 11",
    "Current 12", "Current 13", "Current 14", "Current 15"
};
std::map<std::string, bool> power2_values;
std::vector<std::string> reset_power2_keys = {
    "Current 7", "Current 8", "Current 9", "Current 10", "Current 11",
    "Current 12", "Current 13", "Current 14", "Current 15"
};

std::vector<std::string> autonomy_keys = {
    "Robot State", "Excavation State", "Error State", "Diagnostics State", 
    "Tilt State", "Dump State", "Level Bucket", "Level Arms", "Dest X", "Dest Z"
};
std::map<std::string, bool> autonomy_values;
std::vector<std::string> reset_autonomy_keys = {
    "Robot State", "Excavation State", "Error State", "Diagnostics State", 
    "Tilt State", "Dump State", "Level Bucket", "Level Arms", "Dest X", "Dest Z"
};

std::vector<std::string> zed_keys = {
    "X", "Y", "Z", "roll", "pitch", "yaw", "aruco"
};
std::map<std::string, bool> zed_values;
std::vector<std::string> reset_zed_keys = {
    "X", "Y", "Z", "roll", "pitch", "yaw", "aruco"
};

std::vector<std::string> communication_keys = {
    "RSSI", "Wi-Fi", "CAN Bus", "Using CAN1", "RX packets", "TX packets", "CAN Bus2", "RX2 packets", "TX2 packets", "Status"
};
std::map<std::string, bool> communication_values;
std::vector<std::string> reset_communication_keys = {
    "RSSI", "Wi-Fi", "CAN Bus", "Using CAN1", "RX packets", "TX packets", "CAN Bus2", "RX2 packets", "TX2 packets", "Status"
};

void initialize_bool_map(std::map<std::string, bool>& map, const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        map[key] = true;
    }
}

void initialize_maps(){
    initialize_bool_map(talon_values, talon_keys);
    initialize_bool_map(falcon_values, falcon_keys);
    initialize_bool_map(linear_values, linear_keys);
    initialize_bool_map(power_values, power_keys);
    initialize_bool_map(power2_values, power2_keys);
    initialize_bool_map(autonomy_values, autonomy_keys);
    initialize_bool_map(zed_values, zed_keys);
    initialize_bool_map(communication_values, communication_keys);
}

double roll_rotation_angle = 0.0;
Glib::RefPtr<Gdk::Pixbuf> roll_pixbuf;
Gtk::Image* roll_image;

double pitch_rotation_angle = 0.0;
Glib::RefPtr<Gdk::Pixbuf> pitch_pixbuf;
Glib::RefPtr<Gdk::Pixbuf> lvl_pixbuf;
Gtk::Image* pitch_image;
Gtk::Image* lvl_image;


double MULTIPLIER_X = 1100.0 / 6.88;
double MULTIPLIER_Y = 800.0 / 5.0;

double ARENA_WIDTH_M = 6.88, ARENA_HEIGHT_M = 5.0;
double ARENA_WIDTH_P = 1100.0, ARENA_HEIGHT_P = 800.0;

double UCF_WIDTH_M = 8.14, UCF_HEIGHT_M = 4.57;
double UCF_WIDTH_P = 1300.0, UCF_HEIGHT_P = 730;

double COSMIC_WIDTH_M = 5.48, COSMIC_HEIGHT_M = 4.87;
double COMSIC_WIDTH_P = 877, COSMIC_HEIGHT_P = 780;

double LAB_WIDTH_M = 5.0, LAB_HEIGHT_M = 4.0;
double LAB_WIDTH_P = 800, LAB_HEIGHT_P = 640;


std::vector<InfoFrame*> infoFrameList;

struct AxisEvent{
    bool isSet=false;
    uint8_t which;
    uint8_t axis;  //0-roll 1-pitch 2-throttle 3-yaw
    int value;
};
std::vector<std::vector<AxisEvent*>*>* axisEventList;


class DrawingArea : public Gtk::DrawingArea {
    public:
        DrawingArea() : top_color_("#D3D3D3"), bottom_color_("#A9A9A9"), ratio_(2.0 / 3.0) {}
        
        void set_height_ratio(double ratio){
            ratio_ = ratio;
            queue_draw();
        }
    
    protected:
        bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
            int width = get_allocated_width();
            int height = get_allocated_height();
            cr->set_source_rgb(top_color_.get_red(), top_color_.get_green(), top_color_.get_blue());
            cr->rectangle(0, 0, width, height * ratio_);
            cr->fill();
            
            cr->set_source_rgb(bottom_color_.get_red(), bottom_color_.get_green(), bottom_color_.get_blue());
            cr->rectangle(0, height * ratio_, width, height * (1 - ratio_));
            cr->fill();
            
            return true;
        }
    
    private:
        Gdk::RGBA top_color_;
        Gdk::RGBA bottom_color_;
        double ratio_;
    };

DrawingArea* right_arm;
DrawingArea* left_arm;
DrawingArea* right_bucket;
DrawingArea* left_bucket;
Gtk::Box* armBox;
Gtk::Box* bucketBox;
bool arm_init = false, bucket_init = false, roll_init = false, pitch_init = false, bucketLevel_init = false;

int right_arm_pos = 0, left_arm_pos = 0, right_bucket_pos = 0, left_bucket_pos = 0;

class ImageOverlay : public Gtk::DrawingArea {
    public:
        ImageOverlay() :
            img_x(100), img_y(50), rotation_angle(0.0), dest_x(-1), dest_y(-1) {
                load_images();
            }
    
        bool update_image_position(double x, double y){
            img_x = x;
            img_y = y;
            queue_draw();
            return true;
        }

        bool update_image_rotation(double rotation){
            rotation_angle = ((rotation * M_PI) / 180);
            queue_draw();
            return true;
        }

        bool update_image_x(double x){
            img_x = x;
            queue_draw();
            return true;
        }

        bool update_image_y(double y){
            img_y = y;
            queue_draw();
            return true;
        }

        // Scale factor of map means 1m = 160px, so scale multiplier sets
        // the size of the rock and hole to scale multiplier meters in radius
        void add_rock_image(int x, int y, double scale_multiplier) {
            rock_data.emplace_back(x, y, scale_multiplier);
            queue_draw();
        }

        void add_hole_image(int x, int y, double scale_multiplier) {
            hole_data.emplace_back(x, y, scale_multiplier);
            queue_draw();
        }

        void add_dest_loc(int x, int y){
            dest_x = x;
            dest_y = y;
            queue_draw();
        }

    protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        if (!background || !overlay) return false;

        int height = 0;
        if(mapUsed == "NASA"){
            height = ARENA_HEIGHT_P;
        }
        else if(mapUsed == "UCF"){
            height = UCF_HEIGHT_P;
        }
        else if(mapUsed == "Cosmic"){
            height = COSMIC_HEIGHT_P;
        }
        else if(mapUsed == "Lab"){
            height = LAB_HEIGHT_P;
        }
        else{
            height = ARENA_HEIGHT_P;
        }
    
        // Get widget and image sizes to scale the images correctly
        int widget_width = get_allocation().get_width();
        int widget_height = get_allocation().get_height();
    
        int img_width = background->get_width();
        int img_height = background->get_height();
    
        double scale_x = static_cast<double>(widget_width) / img_width;
        double scale_y = static_cast<double>(widget_height) / img_height;
        double scale = std::min(scale_x, scale_y);
    
        double scaled_width = img_width * scale;
        double scaled_height = img_height * scale;
        double offset_x = (widget_width - scaled_width) / 2.0;
        double offset_y = (widget_height - scaled_height) / 2.0;
    
        // Apply transformations for both background and overlay
        cr->save();
        cr->translate(offset_x, offset_y);
        cr->scale(scale, scale);
    
        cr->save();
        Gdk::Cairo::set_source_pixbuf(cr, background, 0, 0);
        cr->paint();
        cr->restore();

        cr->save();
        
        double cam_offset_x = 20.0; // meters * 160
        double cam_offset_y = 60.0;

        double cos_theta = std::cos(rotation_angle);
        double sin_theta = std::sin(rotation_angle);
        double rotated_offset_x = cam_offset_x * cos_theta - cam_offset_y * sin_theta;
        double rotated_offset_y = cam_offset_x * sin_theta + cam_offset_y * cos_theta;

        cr->translate(img_x + rotated_offset_x + overlay->get_width() / 2,
                    height - (img_y + rotated_offset_y + overlay->get_height() / 2));
        cr->rotate(rotation_angle);
        cr->translate(-overlay->get_width() / 2, -overlay->get_height() / 2);

        Gdk::Cairo::set_source_pixbuf(cr, overlay, 0, 0);
        cr->paint();
        cr->restore();

        // Draw rocks
        for (const auto& data : rock_data) {
            int new_width = rock->get_width() * data.scale_multiplier;
            int new_height = rock->get_height() * data.scale_multiplier;
            auto scaled_pixbuf = rock->scale_simple(new_width, new_height, Gdk::INTERP_BILINEAR);
            int draw_x = data.x - (new_width / 2);
            int draw_y = height - (data.y + new_height / 2);
            Gdk::Cairo::set_source_pixbuf(cr, scaled_pixbuf, draw_x, draw_y);
            cr->paint();
        }
    
        // Draw holes
        for (const auto& data : hole_data) {
            int new_width = hole->get_width() * data.scale_multiplier;
            int new_height = hole->get_height() * data.scale_multiplier;
            auto scaled_pixbuf = hole->scale_simple(new_width, new_height, Gdk::INTERP_BILINEAR);
            int draw_x = data.x - (new_width / 2);
            int draw_y = height - (data.y + new_height / 2);
            Gdk::Cairo::set_source_pixbuf(cr, scaled_pixbuf, draw_x, draw_y);
            cr->paint();
        }

        if(dest_x != -1 && dest_y != -1){
            int dest_img_w = dest_image->get_width();
            int dest_img_h = dest_image->get_height();

            int draw_x = dest_x - dest_img_w / 2;
            int draw_y = height - (dest_y + dest_img_h / 2);

            Gdk::Cairo::set_source_pixbuf(cr, dest_image, draw_x, draw_y);
            cr->paint();
        }

        cr->restore();
        cr->reset_clip();
    
        return true;
    }
    
    private:
        Glib::RefPtr<Gdk::Pixbuf> background, overlay, rock, hole, dest_image;
        double img_x, img_y;
        double rotation_angle;

        int dest_x, dest_y;

        struct ImageData {
            int x, y;
            double scale_multiplier;
            ImageData(int x, int y, double scale) : x(x), y(y), scale_multiplier(scale) {}
        };
        std::vector<ImageData> rock_data;
        std::vector<ImageData> hole_data;
        double m_scale_multiplier;
    
        void load_images(){
            try{
                if(mapUsed == "NASA"){
                    background = Gdk::Pixbuf::create_from_file("../resources/Arena.png");
                }
                else if(mapUsed == "UCF"){
                    background = Gdk::Pixbuf::create_from_file("../resources/UCFArena.png");
                }
                else if(mapUsed == "Cosmic"){
                    background = Gdk::Pixbuf::create_from_file("../resources/CosmicArena.png");
                }
                else if(mapUsed == "Lab"){
                    background = Gdk::Pixbuf::create_from_file("../resources/LabArena.png");
                }
                else{
                    background = Gdk::Pixbuf::create_from_file("../resources/Arena.png");
                }
                overlay = Gdk::Pixbuf::create_from_file("../resources/RobotTop.png");
                rock = Gdk::Pixbuf::create_from_file("../resources/Rock.png");
                hole = Gdk::Pixbuf::create_from_file("../resources/Hole.png");
                dest_image = Gdk::Pixbuf::create_from_file("../resources/X.png");
            }
            catch(const Glib::Exception& ex){
                g_warning("Failed to load images: %s", ex.what().c_str());
            }
        }
};

ImageOverlay* overlay_area;

bool set_source_hex_color(const Cairo::RefPtr<Cairo::Context>& cr, const std::string& color_string) {
    if (color_string.empty()) return false;

    if (color_string[0] == '#' && color_string.length() == 7) {
        try {
            int r = std::stoi(color_string.substr(1, 2), nullptr, 16);
            int g = std::stoi(color_string.substr(3, 2), nullptr, 16);
            int b = std::stoi(color_string.substr(5, 2), nullptr, 16);
            float R = r / 255.0;
            float G = g / 255.0;
            float B = b / 255.0;
            cr->set_source_rgb(R, G, B);
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Invalid hex color: " << color_string << std::endl;
            return false;
        }
    }

    std::regex rgb_regex(R"(rgb\((\d+),\s*(\d+),\s*(\d+)\))");
    std::smatch match;
    if (std::regex_match(color_string, match, rgb_regex)) {
        try {
            int r = std::stoi(match[1]);
            int g = std::stoi(match[2]);
            int b = std::stoi(match[3]);
            float R = r / 255.0;
            float G = g / 255.0;
            float B = b / 255.0;
            // Not entirely sure why it needs to be BGR instead of RGB, but it does
            cr->set_source_rgb(B, G, R);
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Invalid rgb() values: " << color_string << std::endl;
            return false;
        }
    }

    std::cerr << "Unsupported color format: " << color_string << std::endl;
    return true;
}

Glib::RefPtr<Gdk::Pixbuf> rotate_image(Glib::RefPtr<Gdk::Pixbuf> pixbuf, double angle_deg, int target_width, int target_height) {
    double angle_rad = angle_deg * M_PI / 180.0;

    int width = pixbuf->get_width();
    int height = pixbuf->get_height();

    int new_width = static_cast<int>(std::abs(width * std::cos(angle_rad)) + std::abs(height * std::sin(angle_rad)));
    int new_height = static_cast<int>(std::abs(width * std::sin(angle_rad)) + std::abs(height * std::cos(angle_rad)));

    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, new_width, new_height);
    auto cr = Cairo::Context::create(surface);

    // Fill background
    if (angle_deg > 30 || angle_deg < -30) {
        cr->set_source_rgb(1.0, 0.0, 0.0); // Red
    } 
    else {
        if(isLightMode){
            if(!set_source_hex_color(cr, lightBackgroundColor)){
                cr->set_source_rgb(1.0, 1.0, 1.0); // White
            }
        }
        else{
            if(!set_source_hex_color(cr, darkBackgroundColor)){
                cr->set_source_rgb(1.0, 1.0, 1.0); // White
            }
        }
            
    }
    cr->paint();

    // Move to center and rotate
    cr->translate(new_width / 2.0, new_height / 2.0);
    cr->rotate(angle_rad);
    cr->translate(-width / 2.0, -height / 2.0);

    // Draw original pixbuf
    Gdk::Cairo::set_source_pixbuf(cr, pixbuf, 0, 0);
    cr->paint();

    // Copy Cairo surface into a new Pixbuf
    Glib::RefPtr<Gdk::Pixbuf> rotated_pixbuf = Gdk::Pixbuf::create(
        Gdk::COLORSPACE_RGB, true, 8, new_width, new_height
    );

    unsigned char* dest_pixels = rotated_pixbuf->get_pixels();
    int dest_stride = rotated_pixbuf->get_rowstride();
    const unsigned char* src_pixels = surface->get_data();
    int src_stride = surface->get_stride();

    for (int y = 0; y < new_height; ++y) {
        memcpy(dest_pixels + y * dest_stride, src_pixels + y * src_stride, new_width * 4);
    }

    // Crop to target size
    int crop_x = std::max(0, (new_width - target_width) / 2);
    int crop_y = std::max(0, (new_height - target_height) / 2);

    Glib::RefPtr<Gdk::Pixbuf> resized_pixbuf = rotated_pixbuf->create_subpixbuf(rotated_pixbuf,
        crop_x, crop_y, target_width, target_height
    );

    // Draw black markers
    unsigned char* new_pixels = resized_pixbuf->get_pixels();
    int new_rowstride = resized_pixbuf->get_rowstride();
    int new_channels = resized_pixbuf->get_n_channels();

    for (int y = 98; y <= 101; ++y) {
        unsigned char* row_start = new_pixels + y * new_rowstride;
        
        for (int x = 0; x <= 15; ++x) {
            unsigned char* new_pixel = row_start + x * new_channels;
            new_pixel[0] = 0;
            new_pixel[1] = 0;
            new_pixel[2] = 0;
            if (new_channels == 4) {
                new_pixel[3] = 255;
            }
        }
    
        for (int x = 185; x <= 199; ++x) {
            unsigned char* new_pixel = row_start + x * new_channels;
            new_pixel[0] = 0;
            new_pixel[1] = 0;
            new_pixel[2] = 0;
            if (new_channels == 4) {
                new_pixel[3] = 255;
            }
        }
    }

    return resized_pixbuf;
}

class BorderedBox : public Gtk::Box {
    public:
    BorderedBox(Gtk::Orientation orientation, int spacing)
    : Gtk::Box(orientation, spacing) {}
    
    protected:
        bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
            Gtk::Box::on_draw(cr); 
    
            auto allocation = get_allocation();
            double width = allocation.get_width();
            double height = allocation.get_height();
    
            cr->set_line_width(1.0);
            cr->set_source_rgb(0, 0, 0);
    
            cr->rectangle(1, 1, width - 2, height - 2);
            cr->stroke();
    
            return true;
        }
    };

class CircleDrawingArea : public Gtk::DrawingArea{
    public:
        CircleDrawingArea()
        {
            color_.set_rgba(0.0, 0.0, 0.0, 1.0);
            background_color_.set_rgba(1.0, 1.0, 1.0, 1.0);
        }
    
        void set_color(const Gdk::RGBA& color)
        {
            color_ = color;
            queue_draw();
        }

        void set_background_color(const Gdk::RGBA& color){
            background_color_ = color;
            queue_draw();
        }
    
    protected:
        bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override
        {
            cr->set_source_rgba(background_color_.get_red(), background_color_.get_green(), background_color_.get_blue(), background_color_.get_alpha());
            cr->paint();
    
            cr->set_source_rgba(color_.get_red(), color_.get_green(), color_.get_blue(), color_.get_alpha());
    
            double width = get_width();
            double height = get_height();
            double radius = std::min(width, height) / 4;
    
            cr->arc(width/2, height/2, radius, 0, 2*M_PI);
            cr->fill();
    
            return true;
        }
    private:
        Gdk::RGBA color_;
        Gdk::RGBA background_color_;
    };

CircleDrawingArea* talon1Circle;
CircleDrawingArea* talon3Circle;
CircleDrawingArea* falcon1Circle;
CircleDrawingArea* falcon2Circle;
CircleDrawingArea* falcon3Circle;
CircleDrawingArea* falcon4Circle;
CircleDrawingArea* lowerFalcon1Circle;
CircleDrawingArea* lowerFalcon2Circle;
CircleDrawingArea* lowerFalcon3Circle;
CircleDrawingArea* lowerFalcon4Circle;

// TODO: Modify this to be more descriptive and make the graphs better
// Not entirely sure what all that will entail
// TODO: Fix potentiometer not displaying correctly
class MultiMotorGraph : public Gtk::Box {
    public:
        enum GraphType {
            VOLTAGE,
            CURRENT,
            POSITION,
            OUTPUT_PERCENT,
            SPEED,
            POTENTIOMETER
        };
    
        MultiMotorGraph(const std::string& title, GraphType type, const std::vector<std::string>& motorNames)
            : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5),
              title(title),
              graphType(type),
              motorNames(motorNames) {
            
            // Setup colors and ranges based on graph type
            colors = {
                {1.0, 0.0, 0.0}, // Red - Motor 1
                {0.0, 0.5, 0.0}, // Green - Motor 2
                {0.0, 0.0, 1.0}, // Blue - Motor 3
                {1.0, 0.0, 1.0}, // Magenta - Motor 4
                {1.0, 0.5, 0.0}, // Orange - Motor 5
                {0.0, 0.5, 0.5}  // Teal - Motor 6
            };
    
            // Set ranges based on graph type
            switch(graphType) {
                case VOLTAGE:
                    minVal = 14.5f;
                    maxVal = 17.0f; // 14-17V for Talon voltage
                    yLabel = "Voltage (V)";
                    break;
                case CURRENT:
                    minVal = 0.0f;
                    maxVal = 50.0f; // 0-50A for current (adjust as needed)
                    yLabel = "Current (A)";
                    break;
                case POSITION:
                    minVal = 0.0f;
                    maxVal = 1024.0f; // 0-1024 for position (adjust based on your sensor)
                    yLabel = "Position (units)";
                    break;
                case OUTPUT_PERCENT:
                    minVal = 0.0f;
                    maxVal = 1.0f; // -100% to 100% output
                    yLabel = "Output (%)";
                    break;
                case SPEED:
                //TODO: SPEED RANGE 
                minVal = 0.0f;
                maxVal = 1.0f; // -100% to 100% speed
                yLabel = "Speed (normalized)";
                break;
                case POTENTIOMETER:
                //TODO: POTENTIOMETER RANGE 
                    minVal = 0.0f;
                    maxVal = 1024.0f; // 0-1024 typical for potentiometers
                    yLabel = "Potentiometer";
                    break;
            }
    
            // Create legend
            setup_legend();
            
            // Create drawing area
            setup_graph_area();
        }
    
        void update_data(const std::string& motorName, float value) {
            // For output percentage and speed, clamp values to [-1, 1] range
            if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                value = std::max(-1.0f, std::min(1.0f, value));
            }
            // For potentiometer, clamp to [0, 5] range
            else if (graphType == POTENTIOMETER) {
                value = std::max(0.0f, std::min(1024.0f, value));
            }
            
            data[motorName].push_back(value);
            
            if (data[motorName].size() > 100) {
                data[motorName].pop_front();
            }
            graphArea->queue_draw();
        }
    
    private:
        void setup_legend() {
            Gtk::Box* legendBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
            legendBox->property_margin().set_value(5);
            
            // Title with units
            Gtk::Label* titleLabel = Gtk::manage(new Gtk::Label(title + " (" + yLabel + ")"));
            titleLabel->set_halign(Gtk::ALIGN_START);
            legendBox->add(*titleLabel);
            
            // Color indicators
            for (size_t i = 0; i < motorNames.size(); i++) {
                Gtk::DrawingArea* colorSwatch = Gtk::manage(new Gtk::DrawingArea());
                colorSwatch->set_size_request(15, 15);
                colorSwatch->signal_draw().connect(
                    sigc::bind(sigc::mem_fun(*this, &MultiMotorGraph::draw_color_swatch), i));
                
                Gtk::Label* motorLabel = Gtk::manage(new Gtk::Label(motorNames[i]));
                motorLabel->set_margin_start(5);
                
                Gtk::Box* legendItem = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
                legendItem->add(*colorSwatch);
                legendItem->add(*motorLabel);
                legendItem->set_margin_end(15);
                
                legendBox->add(*legendItem);
            }
            
            this->add(*legendBox);
        }
    
        void setup_graph_area() {
            graphArea = Gtk::manage(new Gtk::DrawingArea());
            graphArea->set_hexpand(true);
            graphArea->set_vexpand(true);
            graphArea->signal_draw().connect(
                sigc::mem_fun(*this, &MultiMotorGraph::draw_graph));
            this->add(*graphArea);
        }
    
        bool draw_color_swatch(const Cairo::RefPtr<Cairo::Context>& cr, int colorIndex) {
            const auto& color = colors[colorIndex % colors.size()];
            cr->set_source_rgb(color[0], color[1], color[2]);
            cr->rectangle(0, 0, 15, 15);
            cr->fill();
            return true;
        }
    
        bool draw_graph(const Cairo::RefPtr<Cairo::Context>& cr) {
            Gtk::Allocation alloc = graphArea->get_allocation();
            const int width = alloc.get_width();
            const int height = alloc.get_height();
    
            // Clear background
            cr->set_source_rgb(1, 1, 1);
            cr->paint();
    
            // Draw border
            cr->set_source_rgb(0.7, 0.7, 0.7);
            cr->rectangle(0, 0, width, height);
            cr->stroke();
    
            // Calculate grid steps based on range
            float range = maxVal - minVal;
            float step;
            
            if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                step = 0.1f; // 25% increments for output and speed
            } else if (graphType == POTENTIOMETER) {
                step = 100.0f; // 1V increments for potentiometer
            } else {
                step = (range > 1000) ? 100.0f :
                      (range > 20) ? 5.0f : 
                      (range > 10) ? 1.0f : 
                      (range > 5) ? 1.0f : 0.5f;
            }
    
            // Draw grid and labels
            cr->set_source_rgb(0.9, 0.9, 0.9);
            cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
            cr->set_font_size(10);
            
            // Special case for output percentage and speed to show 0 line
            if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                float zeroY = height - ((0 - minVal) / range) * (height - 20);
                cr->set_source_rgb(0.7, 0.7, 0.7);
                cr->move_to(0, zeroY);
                cr->line_to(width, zeroY);
                cr->stroke();
                
                cr->set_source_rgb(0, 0, 0);
                cr->move_to(5, zeroY - 5);
                cr->show_text("0");
            }
            
            for (float v = minVal; v <= maxVal; v += step) {
                // Skip 0 if we already drew it specially
                if ((graphType == OUTPUT_PERCENT || graphType == SPEED) && v == 0) {
                    continue;
                }
                
                float y = height - ((v - minVal) / range) * (height - 20);
                cr->set_source_rgb(0.9, 0.9, 0.9);
                cr->move_to(0, y);
                cr->line_to(width, y);
                cr->stroke();
                
                cr->set_source_rgb(0, 0, 0);
                cr->move_to(5, y - 5);
                
                // Format label based on value size and type
                if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(0), v * 100) + "%");
                } else if (graphType == POTENTIOMETER) {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(1), v) + "V");
                } else if (maxVal > 100) {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(0), v));
                } else {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(1), v));
                }
            }
    
            // Draw each motor's data
            for (size_t i = 0; i < motorNames.size(); i++) {
                const auto& name = motorNames[i];
                if (data[name].empty()) continue;
    
                const auto& color = colors[i % colors.size()];
                cr->set_source_rgb(color[0], color[1], color[2]);
                cr->set_line_width(1.5);
    
                bool first = true;
                for (size_t j = 0; j < data[name].size(); j++) {
                    float x = (j / 100.0) * (width - 20) + 10;
                    float y = height - ((data[name][j] - minVal) / range) * (height - 20);
                    
                    if (first) {
                        cr->move_to(x, y);
                        first = false;
                    } else {
                        cr->line_to(x, y);
                    }
                }
                cr->stroke();
            }
    
            return true;
        }
    
        std::string title;
        std::string yLabel;
        GraphType graphType;
        std::vector<std::string> motorNames;
        std::map<std::string, std::deque<float>> data;
        std::vector<std::array<double, 3>> colors;
        float minVal;
        float maxVal;
        Gtk::DrawingArea* graphArea;
};

MultiMotorGraph* talonVoltageGraph;
MultiMotorGraph* talonCurrentGraph;
MultiMotorGraph* talonPositionGraph;
MultiMotorGraph* talonOutputGraph;


MultiMotorGraph* falconVoltageGraph;
MultiMotorGraph* falconCurrentGraph;
MultiMotorGraph* falconPositionGraph;
MultiMotorGraph* falconOutputGraph;

MultiMotorGraph* linearSpeedGraph;
MultiMotorGraph* linearPotentiometerGraph;


class Speedometer : public Gtk::DrawingArea {
public:
    Speedometer(const std::string& label)
        : label_(label), // Label for the Widget, will be displayed below
          speed_(0.0), 
          reverse_(false), // Should the value be displayed in red
          min_speed_(0.0),
          max_speed_(100.0),
          num_major_divisions_(10), // e.g., 0, 10, 20 ... 100 (11 ticks)
          num_minor_ticks_per_segment_(4), // 4 minor ticks = 5 small intervals
          display_speed_(true),  // Display speed value at bottom of speedo
          numbers_inside_(true), // Display numbers inside outer circle on speedo
          numbers_on_ticks_(true), // Numbers displayed by ticks
          angle_for_zero_(135.0), // Where should the zero value be 
          angle_for_sweep_(270.0), // Number of degrees the speedo should travel
          low_warning_(false), // Should there be a red warning band on the low side
          low_warning_thresh_(0.2), // Where should the low warning band start
          high_warning_(false), // Should there be a red warning band on the high side
          high_warning_thresh_(0.2), // Where should the high warning bnd start
          use_text_label_(false), // Should the speed label be text instead
          text_label_("Label") // Text value for the label
    {
        // To change Speedometer sizes, need to change this value
        // Set a minimum size for the widget
        set_size_request(250, 250);
    }

    void set_speed(double speed) {
        if(speed < min_speed_){
            set_reverse(true);
            speed_ = std::clamp(-speed, min_speed_, max_speed_); // Assuming min_speed_ is typically 0 for magnitude

        }
        else{
            speed_ = std::clamp(speed, min_speed_, max_speed_); // Assuming min_speed_ is typically 0 for magnitude
            set_reverse(false);
        }
        queue_draw();
    }

    void set_reverse(bool reverse) {
        reverse_ = reverse;
        queue_draw();
    }

    // Call this if you want the gauge to represent a range other than 0-max_speed
    // Note: The current drawing logic primarily uses 0 as the start of the scale.
    // Modifying this to a dynamic min_speed_ on the dial requires adjusting tick/needle logic.
    void set_min_speed(double speed) {
        min_speed_ = speed;
        // Potentially adjust speed_ if it's now out of new bounds
        speed_ = std::clamp(speed_, min_speed_, max_speed_);
        queue_draw();
    }

    void set_max_speed(double speed) {
        if (speed < min_speed_) { // Ensure max_speed is not less than min_speed
            max_speed_ = min_speed_;
        } else {
            max_speed_ = speed;
        }
        // Potentially adjust speed_ if it's now out of new bounds
        speed_ = std::clamp(speed_, min_speed_, max_speed_);
        queue_draw();
    }

    void set_num_major_divisions(int divisions) {
        if (divisions > 0) {
            num_major_divisions_ = divisions;
            queue_draw();
        }
    }

    void set_num_minor_ticks_per_segment(int minor_ticks) {
        if (minor_ticks >= 0) {
            num_minor_ticks_per_segment_ = minor_ticks;
            queue_draw();
        }
    }

    void set_display_speed(bool display_speed){
        display_speed_ = display_speed;
    }

    void set_numbers_inside(bool numbers_inside){
        numbers_inside_ = numbers_inside;
    }

    void set_numbers_on_ticks(bool numbers_on_ticks){
        numbers_on_ticks_ = numbers_on_ticks;
    }

    void set_angle_for_zero(double angle_for_zero){
        angle_for_zero_ = angle_for_zero;
    }
    
    void set_angle_for_sweep(double angle_for_sweep){
        angle_for_sweep_ = angle_for_sweep;
    }

    void set_low_warning(bool warning){
        low_warning_ = warning;
    }

    void set_low_warning_thresh(double thresh){
        low_warning_thresh_ = thresh;
    }

    void set_high_warning(bool warning){
        high_warning_ = warning;
    }

    void set_high_warning_thresh(double thresh){
        high_warning_thresh_ = thresh;
    }

    void set_use_text_label(bool text_label){
        use_text_label_ = text_label;
    }

    void set_text_label(std::string label){
        text_label_ = label;
    }


protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        Gtk::Allocation alloc = get_allocation();
        const int w = alloc.get_width() - 15;
        const int h = alloc.get_height() - 15;

        const double smallest_dim = std::min(w, h);
        const double radius = smallest_dim / 2.5; // Main radius for ticks
        const double cx = w / 2.0;
        const double cy = h / 2.0; // Center of the gauge

        const double angle_for_zero_value_rad = angle_for_zero_ * M_PI / 180.0;
        const double total_sweep_angle_rad = angle_for_sweep_ * M_PI / 180.0;

        // Colors
        Gdk::RGBA color_dial_bg;
        color_dial_bg.set_rgba(0.1, 0.1, 0.1, 1.0); // Dark grey
        Gdk::RGBA color_bezel;
        color_bezel.set_rgba(0.2, 0.2, 0.2, 1.0);
        Gdk::RGBA color_tick_mark;
        color_tick_mark.set_rgba(0.9, 0.9, 0.9, 1.0); // Light grey/white
        Gdk::RGBA color_text;
        color_text.set_rgba(0.9, 0.9, 0.9, 1.0);
        Gdk::RGBA color_needle;
        color_needle.set_rgba(1.0, 0.2, 0.2, 1.0); // Reddish
        Gdk::RGBA color_needle_pivot;
        color_needle_pivot.set_rgba(0.7, 0.7, 0.7, 1.0);
        Gdk::RGBA color_speed_text_normal;
        color_speed_text_normal.set_rgba(0.8, 0.8, 1.0, 1.0); // Light blueish
        Gdk::RGBA color_speed_text_reverse;
        color_speed_text_reverse.set_rgba(1.0, 0.8, 0.8, 1.0); // Light reddish
        Gdk::RGBA label_text;
        label_text.set_rgba(0.1, 0.1, 0.1, 1.0);


        // 1. Bezel
        cr->set_source_rgba(color_bezel.get_red(), color_bezel.get_green(), color_bezel.get_blue(), color_bezel.get_alpha());
        cr->arc(cx, cy, radius + 10, 0, 2 * M_PI);
        cr->fill();

        // 2. Dial background
        cr->set_source_rgba(color_dial_bg.get_red(), color_dial_bg.get_green(), color_dial_bg.get_blue(), color_dial_bg.get_alpha());
        cr->arc(cx, cy, radius + 5, 0, 2 * M_PI);
        cr->fill_preserve();
        cr->set_source_rgba(0.3, 0.3, 0.3, 1.0); // Outline for the dial face
        cr->set_line_width(1.0);
        cr->stroke();

        // 3. Ticks and Labels
        const double major_tick_len = 10.0;
        const double minor_tick_len = 5.0;
        const double text_radius_offset = 20.0; // How far from ticks to place text

        // Draw red arc for warning zone
        auto draw_warning_arc = [&](double danger_speed_start, double danger_speed_end) {
            if (danger_speed_start < danger_speed_end && max_speed_ > min_speed_) {
                double ratio_start = (danger_speed_start - min_speed_) / (max_speed_ - min_speed_);
                double ratio_end = (danger_speed_end - min_speed_) / (max_speed_ - min_speed_);

                double angle_start = angle_for_zero_value_rad + ratio_start * total_sweep_angle_rad;
                double angle_end = angle_for_zero_value_rad + ratio_end * total_sweep_angle_rad;

                cr->set_line_width(major_tick_len * 1.5);
                cr->set_source_rgb(1.0, 0.0, 0.0);
                cr->arc(cx, cy, radius - major_tick_len / 2.0, angle_start, angle_end);
                cr->stroke();
            }
        };

        if (low_warning_) {
            draw_warning_arc(min_speed_, min_speed_ + low_warning_thresh_ * (max_speed_ - min_speed_));
        }
        if (high_warning_) {
            draw_warning_arc(max_speed_ - high_warning_thresh_ * (max_speed_ - min_speed_), max_speed_);
        }


        cr->set_source_rgba(color_tick_mark.get_red(), color_tick_mark.get_green(), color_tick_mark.get_blue(), color_tick_mark.get_alpha());
        for (int i = 0; i <= num_major_divisions_; ++i) {
            double tick_ratio = static_cast<double>(i) / num_major_divisions_;
            double angle = angle_for_zero_value_rad + tick_ratio * total_sweep_angle_rad;

            // Major tick
            double x1 = cx + radius * cos(angle);
            double y1 = cy + radius * sin(angle);
            double x2 = cx + (radius - major_tick_len) * cos(angle);
            double y2 = cy + (radius - major_tick_len) * sin(angle);

            cr->set_line_width(2.0);
            cr->move_to(x1, y1);
            cr->line_to(x2, y2);
            cr->stroke();

            // Number label for major tick
            // Ensure max_speed_ is not zero to avoid issues, though labels can be 0
            double value = tick_ratio * (max_speed_ - min_speed_) + min_speed_;
            std::string tick_text = std::to_string(static_cast<int>(round(value)));

            Cairo::TextExtents extents;
            cr->set_font_size(std::max(10.0, smallest_dim / 20.0)); // Responsive font size
            cr->get_text_extents(tick_text, extents);

            // Adjust text position to be centered and outside ticks
            double label_distance = numbers_inside_
                ? (radius - major_tick_len - text_radius_offset)
                : (radius + text_radius_offset);

            double tx = cx + label_distance * cos(angle) - (extents.width / 2.0 + extents.x_bearing);
            double ty = cy + label_distance * sin(angle) - (extents.height / 2.0 + extents.y_bearing);
            
            if (numbers_inside_) {
                cr->set_source_rgba(color_text.get_red(), color_text.get_green(), color_text.get_blue(), color_text.get_alpha());
            }
            else {
                cr->set_source_rgb(0.0, 0.0, 0.0);
            }
            cr->move_to(tx, ty);
            if(numbers_on_ticks_)
                cr->show_text(tick_text);

            // Reset color after drawing text
            cr->set_source_rgba(color_text.get_red(), color_text.get_green(), color_text.get_blue(), color_text.get_alpha());

            // Minor ticks (except after the last major tick)
            if (i < num_major_divisions_) {
                for (int j = 1; j <= num_minor_ticks_per_segment_; ++j) {
                    double minor_tick_ratio = tick_ratio + (static_cast<double>(j) / num_major_divisions_ / (num_minor_ticks_per_segment_ + 1));
                    // Ensure minor ticks don't overshoot total_sweep_angle_rad
                    if (minor_tick_ratio * total_sweep_angle_rad > total_sweep_angle_rad + 1e-6) continue; 

                    double minor_angle = angle_for_zero_value_rad + minor_tick_ratio * total_sweep_angle_rad;
                    double mx1 = cx + radius * cos(minor_angle);
                    double my1 = cy + radius * sin(minor_angle);
                    double mx2 = cx + (radius - minor_tick_len) * cos(minor_angle);
                    double my2 = cy + (radius - minor_tick_len) * sin(minor_angle);

                    cr->set_line_width(1.0);
                    cr->move_to(mx1, my1);
                    cr->line_to(mx2, my2);
                    cr->stroke();
                }
            }
        }

        // 4. Needle
        double current_speed_ratio = 0.0;
        if (max_speed_ > min_speed_) { // Avoid division by zero or undefined behavior
             current_speed_ratio = (speed_ - min_speed_) / (max_speed_ - min_speed_);
        }
        else if (max_speed_ == min_speed_ && speed_ == min_speed_){
             current_speed_ratio = 0.0; // Or 0.5 if middle, but for 0-max this is fine
        }


        double needle_angle = angle_for_zero_value_rad + current_speed_ratio * total_sweep_angle_rad;
        cr->set_source_rgba(color_needle.get_red(), color_needle.get_green(), color_needle.get_blue(), color_needle.get_alpha());
        cr->set_line_width(std::max(2.0, smallest_dim / 80.0)); // Responsive needle width
        cr->move_to(cx, cy);
        cr->line_to(cx + (radius - major_tick_len/2) * cos(needle_angle), cy + (radius - major_tick_len/2) * sin(needle_angle));
        cr->stroke();

        // 5. Needle Pivot
        cr->set_source_rgba(color_needle_pivot.get_red(), color_needle_pivot.get_green(), color_needle_pivot.get_blue(), color_needle_pivot.get_alpha());
        cr->arc(cx, cy, std::max(4.0, smallest_dim / 40.0), 0, 2 * M_PI);
        cr->fill();
        cr->set_source_rgba(0.1,0.1,0.1,1); // Pivot outline
        cr->set_line_width(0.5);
        cr->arc(cx, cy, std::max(4.0, smallest_dim / 40.0), 0, 2 * M_PI);
        cr->stroke();


        // 6. Speed Text Display
        if(!use_text_label_){
            std::ostringstream speed_stream;
            speed_stream << std::fixed << std::setprecision(1) << speed_;
            std::string speed_str = speed_stream.str();
            if (reverse_) {
                speed_str += " R";
                cr->set_source_rgba(color_speed_text_reverse.get_red(), color_speed_text_reverse.get_green(), color_speed_text_reverse.get_blue(), color_speed_text_reverse.get_alpha());
            } else {
                cr->set_source_rgba(color_speed_text_normal.get_red(), color_speed_text_normal.get_green(), color_speed_text_normal.get_blue(), color_speed_text_normal.get_alpha());
            }

            cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
            cr->set_font_size(std::max(14.0, smallest_dim / 12.0));

            Cairo::TextExtents speed_extents;
            cr->get_text_extents(speed_str, speed_extents);
            cr->move_to(cx - (speed_extents.width / 2.0 + speed_extents.x_bearing), cy + radius * 0.5); // Position below center
            if(display_speed_)
                cr->show_text(speed_str);
        }
        else{
            cr->set_source_rgba(color_speed_text_normal.get_red(), color_speed_text_normal.get_green(), color_speed_text_normal.get_blue(), color_speed_text_normal.get_alpha());

            cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
            cr->set_font_size(std::max(14.0, smallest_dim / 12.0));

            Cairo::TextExtents speed_extents;
            cr->get_text_extents(text_label_, speed_extents);
            cr->move_to(cx - (speed_extents.width / 2.0 + speed_extents.x_bearing), cy + radius * 0.5); // Position below center
            if(display_speed_)
                cr->show_text(text_label_);
        }

        // 7. Main Label (e.g., "Left Speed")
        cr->set_source_rgba(label_text.get_red(), label_text.get_green(), label_text.get_blue(), label_text.get_alpha());
        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
        cr->set_font_size(std::max(16.0, smallest_dim / 15.0));
        Cairo::TextExtents label_extents;
        cr->get_text_extents(label_, label_extents);
        cr->move_to(cx - (label_extents.width / 2.0 + label_extents.x_bearing), cy + radius + 15 + label_extents.height); // Position below gauge
        cr->show_text(label_);

        return true;
    }

private:
    std::string label_;
    double speed_;
    bool reverse_;
    double min_speed_;
    double max_speed_;
    int num_major_divisions_;
    int num_minor_ticks_per_segment_;
    bool display_speed_;
    bool numbers_inside_;
    bool numbers_on_ticks_;
    double angle_for_zero_;
    double angle_for_sweep_;
    bool low_warning_;
    double low_warning_thresh_;
    bool high_warning_;
    double high_warning_thresh_;
    bool use_text_label_;
    std::string text_label_;
};    

Speedometer* leftSpeedometer;
Speedometer* rightSpeedometer;
bool displaySpeed = true;
bool numbersInside = true;
bool numberTicks = true;

std::string motorDisplayed = "Talon 1";
Speedometer* voltageDial;
Speedometer* temperatureDial;
DrawingArea* positionDial;
Speedometer* percentDial;
Speedometer* velocityDial;
Speedometer* currentDial;
bool displayMotor = false;


extern "C" void destroy_pixbuf_data(const guint8* data) {
    delete[] data;
}

class VideoWidget : public Gtk::DrawingArea {
public:
    VideoWidget() {}

    void setFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(frameMutex);
        if (frame.empty()) {
            latestFrame.release();
            currentPixbuf.reset();
        } else {
            latestFrame = frame.clone();

            // Convert to RGB if needed
            cv::Mat frameToDisplay_CV = latestFrame;
            if (frameToDisplay_CV.channels() == 1) {
                cv::cvtColor(frameToDisplay_CV, frameToDisplay_CV, cv::COLOR_GRAY2RGB);
            } else if (frameToDisplay_CV.channels() == 4) {
                cv::cvtColor(frameToDisplay_CV, frameToDisplay_CV, cv::COLOR_BGRA2RGB);
            } else if (frameToDisplay_CV.channels() == 3) {
                cv::cvtColor(frameToDisplay_CV, frameToDisplay_CV, cv::COLOR_BGR2RGB);
            }

            int width = frameToDisplay_CV.cols;
            int height = frameToDisplay_CV.rows;
            int cv_channels = frameToDisplay_CV.channels();
            int pixbuf_rowstride = width * cv_channels;
            size_t data_size = static_cast<size_t>(height) * pixbuf_rowstride;

            // Allocate buffer for Gdk::Pixbuf data.
            guchar* copiedData = new guchar[data_size];
            if (frameToDisplay_CV.isContinuous()) {
                std::memcpy(copiedData, frameToDisplay_CV.data, data_size);
            } else {
                for (int r = 0; r < height; ++r) {
                    std::memcpy(copiedData + r * pixbuf_rowstride,
                                frameToDisplay_CV.data + r * frameToDisplay_CV.step,
                                static_cast<size_t>(width) * cv_channels);
                }
            }

            // Create pixbuf and let it manage the buffer
            currentPixbuf = Gdk::Pixbuf::create_from_data(
                static_cast<const guint8*>(copiedData),
                Gdk::COLORSPACE_RGB,
                false,
                8,
                width,
                height,
                pixbuf_rowstride,
                [](const guint8* data){
                    delete[] data;
                }
            );
        }
        queue_draw();
    }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        std::lock_guard<std::mutex> lock(frameMutex);
        Gtk::Allocation allocation = get_allocation();

        if (!currentPixbuf) {
            cr->set_source_rgb(0.1, 0.1, 0.1);
            cr->rectangle(0, 0, allocation.get_width(), allocation.get_height());
            cr->fill();
            return true;
        }

        int width = currentPixbuf->get_width();
        int height = currentPixbuf->get_height();
        int widget_width = allocation.get_width();
        int widget_height = allocation.get_height();

        double scale_ratio_x = (width > 0) ? static_cast<double>(widget_width) / width : 1.0;
        double scale_ratio_y = (height > 0) ? static_cast<double>(widget_height) / height : 1.0;
        double actual_scale_ratio = std::min(scale_ratio_x, scale_ratio_y);

        int scaled_width = static_cast<int>(width * actual_scale_ratio);
        int scaled_height = static_cast<int>(height * actual_scale_ratio);

        double draw_x = (widget_width - scaled_width) / 2.0;
        double draw_y = (widget_height - scaled_height) / 2.0;

        Glib::RefPtr<Gdk::Pixbuf> scaled_pixbuf = currentPixbuf;
        if (scaled_width > 0 && scaled_height > 0 &&
            (scaled_width != width || scaled_height != height)) {
            scaled_pixbuf = currentPixbuf->scale_simple(
                scaled_width, scaled_height, Gdk::INTERP_BILINEAR);
        }

        if (scaled_pixbuf) {
            Gdk::Cairo::set_source_pixbuf(cr, scaled_pixbuf, draw_x, draw_y);
            cr->paint();
        }
        return true;
    }

private:
    cv::Mat latestFrame;
    std::mutex frameMutex;
    Glib::RefPtr<Gdk::Pixbuf> currentPixbuf;
};

VideoWidget* videoArea;


void initRoll(){
    if(!roll_init){
        roll_image = Gtk::manage(new Gtk::Image());
        
        if(noVideo)
            sensorBox->add(*roll_image);
        else
            bottomLowerBox->add(*roll_image);
        
        try{
            roll_pixbuf = Gdk::Pixbuf::create_from_file("../resources/RobotSide.png");
        }
        catch(const Glib::FileError& e){
            g_print("Failed to load image: %s\n", e.what().c_str());
            return;
        }

        if(!noVideo){
            Gtk::Box* padding = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
            padding->set_size_request(100, 100);
            bottomLowerBox->add(*padding);
        }
        
        Glib::RefPtr<Gdk::Pixbuf> newrollpixbuf = rotate_image(roll_pixbuf, roll_rotation_angle, 200, 200);
        roll_image->set(newrollpixbuf);
        roll_init = true;
        window->show_all();
    }
}


void initPitch(){
    if(!pitch_init){
        if(!noVideo){
            Gtk::Box* padding = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
            padding->set_size_request(100, 100);
            bottomLowerBox->add(*padding);
        }
        pitch_image = Gtk::manage(new Gtk::Image());
        if(noVideo)
            sensorBox->add(*pitch_image);
        else
            bottomLowerBox->add(*pitch_image);
        
        try{
            pitch_pixbuf = Gdk::Pixbuf::create_from_file("../resources/RobotBack.png");
        }
        catch(const Glib::FileError& e){
            g_print("Failed to load image: %s\n", e.what().c_str());
            return;
        }
        
        Glib::RefPtr<Gdk::Pixbuf> newpitchpixbuf = rotate_image(pitch_pixbuf, pitch_rotation_angle, 200, 200);
        pitch_image->set(newpitchpixbuf);
        pitch_init = true;
        window->show_all();
    }
}

void initBucketLvl(){
    if(!bucketLevel_init){
        Gtk::Box* padding = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
        padding->set_size_request(100, 100);
        bottomLowerBox->add(*padding);
        lvl_image = Gtk::manage(new Gtk::Image());
        try{
            lvl_pixbuf = Gdk::Pixbuf::create_from_file("../resources/bucket.png");
        }
        catch(const Glib::FileError& e){
            g_print("Failed to load image: %s\n", e.what().c_str());
            return;
        }

        if(noVideo)
            sensorBox->add(*lvl_image);
        else
            bottomLowerBox->add(*lvl_image);


        Glib::RefPtr<Gdk::Pixbuf> newlvlpixbuf = rotate_image(lvl_pixbuf, 0, 200, 200);
        lvl_image->set(newlvlpixbuf);
        bucketLevel_init = true;
        window->show_all();
    }
}

void initArmPos(){
    if(!arm_init){
        Gtk::Box* armTextBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,2));
        armBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));
        armBox->set_size_request(110, -1);
        
        left_arm = Gtk::manage(new DrawingArea());
        left_arm->set_size_request(40, 180);
        left_arm->set_hexpand(true);
        left_arm->set_halign(Gtk::ALIGN_CENTER);
        armBox->add(*left_arm);
        left_arm->show();
        
        right_arm = Gtk::manage(new DrawingArea());
        right_arm->set_size_request(40, 180);
        right_arm->set_hexpand(true);
        right_arm->set_halign(Gtk::ALIGN_CENTER);
        armBox->add(*right_arm);
        right_arm->show();
        right_arm->set_height_ratio(0.5);
        
        armBox->set_halign(Gtk::ALIGN_CENTER);
        armBox->set_valign(Gtk::ALIGN_CENTER);
        
        armTextBox->add(*armBox);
        armTextBox->set_halign(Gtk::ALIGN_CENTER);
        
        Gtk::Label* armPosLabel = Gtk::manage(new Gtk::Label("L 		R"));
        Gtk::Label* armLabel = Gtk::manage(new Gtk::Label("Arm Positions"));
        
        armPosLabel->set_halign(Gtk::ALIGN_CENTER);    
        armLabel->set_halign(Gtk::ALIGN_CENTER);
        
        armTextBox->add(*armPosLabel);
        armTextBox->add(*armLabel);
        
        if(noVideo)
            sensorBox->add(*armTextBox);
        else
            innerLeftBox->add(*armTextBox);

        arm_init = true;
        window->show_all();
    }
}


void initBucketPos(){
    if(!bucket_init){
        Gtk::Box* bucketTextBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,3));
        bucketBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,20));
        bucketBox->set_size_request(110, -1);
        
        left_bucket = Gtk::manage(new DrawingArea());
        left_bucket->set_size_request(40, 180);
        left_bucket->set_hexpand(true);
        left_bucket->set_halign(Gtk::ALIGN_CENTER);
        bucketBox->add(*left_bucket);
        left_bucket->show();
        
        right_bucket = Gtk::manage(new DrawingArea());
        right_bucket->set_size_request(40, 180);
        right_bucket->set_hexpand(true);
        right_bucket->set_halign(Gtk::ALIGN_CENTER);
        bucketBox->add(*right_bucket);
        right_bucket->show();
        right_bucket->set_height_ratio(0.5);
        
        bucketBox->set_halign(Gtk::ALIGN_CENTER);
        bucketBox->set_valign(Gtk::ALIGN_CENTER);
        
        bucketTextBox->add(*bucketBox);
        Gtk::Label* bucketPosLabel = Gtk::manage(new Gtk::Label("L 		R"));
        Gtk::Label* bucketLabel = Gtk::manage(new Gtk::Label("Bucket Positions"));
        bucketTextBox->add(*bucketPosLabel);
        bucketTextBox->add(*bucketLabel);
        
        if(noVideo)
            sensorBox->add(*bucketTextBox);
        else
            innerRightBox->add(*bucketTextBox);
        bucket_init = true;
        window->show_all();
    }
}

void setBackgroundColors(Gdk::RGBA color){
    if(talon1Circle)
        talon1Circle->set_background_color(color);
    if(talon3Circle)
        talon3Circle->set_background_color(color);

    if(falcon1Circle)
        falcon1Circle->set_background_color(color);
    if(falcon2Circle)
        falcon2Circle->set_background_color(color);
    if(falcon3Circle)
        falcon3Circle->set_background_color(color);
    if(falcon4Circle)
        falcon4Circle->set_background_color(color);
    if(lowerFalcon1Circle)
        lowerFalcon1Circle->set_background_color(color);
    if(lowerFalcon2Circle)
        lowerFalcon2Circle->set_background_color(color);
    if(lowerFalcon3Circle)
        lowerFalcon3Circle->set_background_color(color);
    if(lowerFalcon4Circle)
        lowerFalcon4Circle->set_background_color(color);
}

InfoFrame* getInfoFrame(std::string label){
    for (InfoFrame* frame : infoFrameList) {
        if (frame->get_label() != label) continue;
        return frame;
    }
    return nullptr;
}


Gtk::Widget* get_flowbox_child_for(Gtk::FlowBox& flowbox, Gtk::Widget* target_widget) {
    for (auto* child : flowbox.get_children()) {
        auto* flowbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
        if (!flowbox_child) continue;

        if (flowbox_child->get_child() == target_widget)
            return flowbox_child;
    }
    return nullptr;
}

// Dark mode
std::string darkMode =
    "* { font-family: 'Proxima Nova'; font-weight: bold; }\n"
    "window { background-color: " + darkBackgroundColor + "; }\n"
    "#dark_text, #dark_text label { color: #000000; }\n"
    "label, button, entry { color: #edf6fa; }\n"
    "button { border: 1px solid #edf6fa; background-color: transparent; }\n";

std::string lightMode = 
    "* { font-family: 'Proxima Nova'; font-weight: bold }\n"
    "window { background-color: " + lightBackgroundColor + "; }\n"
    "label, button, entry { color: #000000; }\n"
    "button {  border: 1px solid #000000; background-color: #f0f0f0; }\n";


std::string generateDarkModeString(const std::string& color) {
    return
        "* { font-family: 'Proxima Nova'; font-weight: bold; }\n"
    "window { background-color: " + color + "; }\n"
    "#dark_text, #dark_text label { color: #000000; }\n"
    "label, button, entry { color: #edf6fa; }\n"
    "button { border: 1px solid #edf6fa; background-color: transparent; }\n";
}

// Light mode
std::string generateLightModeString(const std::string& color) {
    return
        "* { font-family: 'Proxima Nova'; font-weight: bold }\n"
    "window { background-color: " + color + "; }\n"
    "label, button, entry { color: #000000; }\n"
    "button {  border: 1px solid #000000; background-color: #f0f0f0; }\n";
}


void toggleMode() {
    auto css_provider = Gtk::CssProvider::create();
    Gdk::RGBA background;

    if (isLightMode) {
        css_provider->load_from_data(generateDarkModeString(darkBackgroundColor));
        isLightMode = false;
        background.set(darkBackgroundColor);
    } 
    else {
        css_provider->load_from_data(generateLightModeString(lightBackgroundColor));
        isLightMode = true;
        background.set(lightBackgroundColor);
    }
    if(!noVideo)
        setBackgroundColors(background);

    auto screen = Gdk::Screen::get_default();
    Gtk::StyleContext::add_provider_for_screen(
        screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
} 


void updateBackgroundColor(InfoFrame* infoFrame, std::string label){
    if(isLightMode){
        infoFrame->setBackground(label, lightBackgroundColor);
        infoFrame->setTextColor(label, "#000000", false);
    }
    else{
        infoFrame->setBackground(label, darkBackgroundColor);
        infoFrame->setTextColor(label, "white", false);
    }
}


void updateBackgroundColor(Gtk::Box* box, bool synced){
    if(synced){
        Gdk::RGBA red;
        red.set_rgba(1.0,0,0,1.0);
        box->override_background_color(red);
    }
    else{
        Gdk::RGBA white;
        white.set_rgba(1.0,1.0,1.0,1.0);
        box->override_background_color(white);
    }
}

const std::set<std::string> talonLabels = {"Talon 1", "Talon 3"};
const std::set<std::string> falconLabels = {"Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4"};


CircleDrawingArea* getTalonCircle(const std::string& label) {
    if (label == "Talon 1") return talon1Circle;
    if (label == "Talon 3") return talon3Circle;
    return nullptr;
}


CircleDrawingArea* getFalconCircle(const std::string& label) {
    if (label == "Falcon 1") return falcon1Circle;
    if (label == "Falcon 2") return falcon2Circle;
    if (label == "Falcon 3") return falcon3Circle;
    if (label == "Falcon 4") return falcon4Circle;
    return nullptr;
}

CircleDrawingArea* getLowerFalconCircle(const std::string& label) {
    if (label == "Falcon 1") return lowerFalcon1Circle;
    if (label == "Falcon 2") return lowerFalcon2Circle;
    if (label == "Falcon 3") return lowerFalcon3Circle;
    if (label == "Falcon 4") return lowerFalcon4Circle;
    return nullptr;
}


void updateCircleColor(CircleDrawingArea* circle, bool lowVoltage) {
    if (!circle || noVideo) return;

    Gdk::RGBA color;
    if (lowVoltage)
        color.set_rgba(1.0, 0.0, 0.0, 1.0); // Red
    else
        color.set_rgba(0.0, 1.0, 0.0, 1.0); // Green

    circle->set_color(color);
}


void updateCircleColor(CircleDrawingArea* circle, Gdk::RGBA color) {
    if (!circle || noVideo) return;
    circle->set_color(color);
}


void handleZedElements(const std::vector<Element>& elements) {
    for (const auto& element : elements) {
        if (element.type != TYPE::FLOAT32) continue;
        float value = element.data.front().float32;

        if (element.label == "roll") {
            roll_rotation_angle = std::round(value);
            roll_image->set(rotate_image(roll_pixbuf, -roll_rotation_angle, 200, 200));
        }
        else if (element.label == "yaw") {
            pitch_rotation_angle = std::round(value);
            pitch_image->set(rotate_image(pitch_pixbuf, pitch_rotation_angle, 200, 200));
        }
        else if (element.label == "pitch" && !noArena) {
            overlay_area->update_image_rotation(value - 90);
        }
        else if (element.label == "Z" && !noArena) {
            overlay_area->update_image_y(value * MULTIPLIER_Y);
        }
        else if (element.label == "X" && !noArena) {
            overlay_area->update_image_x(value * MULTIPLIER_X);
        }
    }
}

void handleTalonElements(const std::string& label, const std::vector<Element>& elements) {
    for (const auto& element : elements) {
        if (element.label == "Sensor Position") {
            int pos = element.data.front().uint16;
            if (label == "Talon 1") {
                left_arm_pos = pos;
                left_arm->set_height_ratio((920 - pos) / 920.0);
            }
            else if (label == "Talon 3") {
                left_bucket_pos = pos;
                left_bucket->set_height_ratio((700 - pos) / 700.0);
            }

            bool synced = std::abs(left_arm_pos - right_arm_pos) > 50;
            if (label == "Talon 1" || label == "Talon 2")
                updateBackgroundColor(armBox, synced);
            else
                updateBackgroundColor(bucketBox, synced);

            if (!noVideo) talonPositionGraph->update_data(label, pos);
        }
        else if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            if (!noVideo) talonVoltageGraph->update_data(label, voltage);
            updateCircleColor(getTalonCircle(label), voltage < 15.0f);
        }
        else if (element.label == "Output Current") {
            float current = element.data.front().uint16 / 100.0f;
            if (!noVideo) talonCurrentGraph->update_data(label, current);
        }
        else if (element.label == "Output Percent") {
            float percent = element.data.front().float32;
            if (!noVideo) talonOutputGraph->update_data(label, percent);
        }
    }
}

void handleFalconElements(const std::string& label, const std::vector<Element>& elements) {
    for (const auto& element : elements) {
        if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            if (!noVideo) falconVoltageGraph->update_data(label, voltage);
            updateCircleColor(getFalconCircle(label), voltage < 15.0f);
        }
        else if (element.label == "Output Current") {
            float current = element.data.front().uint16 / 100.0f;
            if (!noVideo) falconCurrentGraph->update_data(label, current);
        }
        else if (element.label == "Output Percent") {
            float percent = element.data.front().float32;
            if (!noVideo) falconOutputGraph->update_data(label, percent);
            if(label == "Falcon 2" || label == "Falcon 4"){
                leftSpeedometer->set_speed(percent * 100.0);
            }
            if(label == "Falcon 1" || label == "Falcon 3"){
                rightSpeedometer->set_speed(percent * 100.0);
            }
        }
        else if (element.label == "Error"){
            bool error = element.data.front().boolean;
            updateCircleColor(getLowerFalconCircle(label), error);
        }
    }
}

void handleCommunicationElements(InfoFrame* frame, const std::vector<Element>& elements) {
    for (const auto& element : elements) {
        if (element.label != "Wi-Fi" && element.label != "CAN Bus") continue;

        std::string text;
        for (const auto& c : element.data) text += c.character;

        if (text == "NON-FUNCTIONAL" || text == "INTERFERENCE" || text == "DOWN") {
            frame->setBackground(element.label, "#FF0000");
            frame->setTextColor(element.label, "white", true);
        }
        else {
            updateBackgroundColor(frame, element.label);
        }
    }
}


void handleAutonomyElements(const std::string& label, const std::vector<Element>& elements) {
    int destX = -1;
    int destY = -1;
    for (const auto& element : elements) {
        if (element.label == "Dest X") {
            destX = element.data.front().float32 * MULTIPLIER_X;
            if(destY != -1){
                overlay_area->add_dest_loc(destX, destY);
                break;
            }
        }
        else if(element.label == "Dest Z"){
            destY = element.data.front().float32 * MULTIPLIER_Y;
            if(destX != -1){
                overlay_area->add_dest_loc(destX, destY);
                break;
            }
        }
    }
}


std::map<std::string, bool>& getMap(std::string label){
    if(label.rfind("Talon", 0) == 0){
        return talon_values;
    }
    if(label.rfind("Falcon", 0) == 0){
        return falcon_values;
    }
    if(label.rfind("Linear", 0) == 0){
        return linear_values;
    }
    if(label.rfind("Autonomy", 0) == 0){
        return autonomy_values;
    }
    if(label.rfind("Communication", 0) == 0){
        return communication_values;
    }
    if(label.rfind("Power2", 0) == 0){
        return power2_values;
    }
    else if(label.rfind("Power", 0) == 0){
        return power_values;
    }
    if(label.rfind("Zed", 0) == 0){
        return zed_values;
    }
    return talon_values;
}

std::map<std::string, std::vector<std::string>*> key_vectors = {
    {"Talon", &talon_keys},
    {"Falcon", &falcon_keys},
    {"Linear", &linear_keys},
    {"Autonomy", &autonomy_keys},
    {"Communication", &communication_keys},
    {"Power2", &power2_keys},
    {"Power", &power_keys},
    {"Zed", &zed_keys}
};

std::vector<std::string> getKeys(const std::string& label) {
    if(label == "Power2"){
        return power2_keys;
    }
    for (const auto& [prefix, keys_ptr] : key_vectors) {
        if (label.rfind(prefix, 0) == 0) {
            return *keys_ptr;
        }
    }
    return talon_keys;
}

bool updateMotorDetails = false;

void updateMotor(std::string label, const std::vector<Element>& elements) {
    if(label != motorDisplayed)
        return;
    
    for (const auto& element : elements) {
        if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            voltageDial->set_speed((double)voltage);
        }
        else if(element.label == "Output Current"){
            float current = element.data.front().uint16 / 100.0f;
            currentDial->set_speed((double)current);
        }
        else if(element.label == "Output Percent"){
            float percent = element.data.front().float32;
            percentDial->set_speed((double)percent * 100);
        }
        else if(element.label == "Temperature"){
            int temperature = element.data.front().uint16;
            temperatureDial->set_speed((double)temperature);
        }
        else if(element.label == "Sensor Position"){
            int pos = element.data.front().uint16;
            if(label == "Talon 1" || label == "Talon 3")
                positionDial->set_height_ratio((920 - pos) / 920.0);
        }
        else if(element.label == "Sensor Velocity"){
            int pos = element.data.front().uint16;
            velocityDial->set_speed((double)pos);
        }
    }
}


void handleGenericElements(std::string label, InfoFrame* frame, const std::vector<Element>& elements) {
    std::map<std::string, bool>& values = getMap(label);
    for (const auto& element : elements) {
        auto it = values.find(element.label);
        if(it == values.end() || !it->second)
            continue;
        const auto& value = element.data.front();

        if (element.type == TYPE::BOOLEAN)       frame->setItem(element.label, value.boolean);
        else if (element.type == TYPE::UINT8)     frame->setItem(element.label, value.uint8);
        else if (element.type == TYPE::INT8)      frame->setItem(element.label, value.int8);
        else if (element.type == TYPE::UINT16) {
            if(element.label == "Bus Voltage" || element.label == "Output Current"){
                float val = value.uint16 / 100.0f;
                if (element.label == "Bus Voltage" && val < 15.0f) {
                    frame->setBackground(element.label, "#FF0000");
                    frame->setTextColor(element.label, "white", true);
                }
                else updateBackgroundColor(frame, element.label);
                frame->setItem(element.label, val);
            }
            else{
                frame->setItem(element.label, value.uint16);
            }

        }
        else if (element.type == TYPE::INT16)     frame->setItem(element.label, value.int16);
        else if (element.type == TYPE::UINT32)    frame->setItem(element.label, value.uint32);
        else if (element.type == TYPE::INT32)     frame->setItem(element.label, value.int32);
        else if (element.type == TYPE::UINT64)    frame->setItem(element.label, value.uint64);
        else if (element.type == TYPE::INT64)     frame->setItem(element.label, value.int64);
        else if (element.type == TYPE::FLOAT32)   frame->setItem(element.label, value.float32);
        else if (element.type == TYPE::FLOAT64)   frame->setItem(element.label, value.float64);
        else if (element.type == TYPE::STRING) {
            std::string text;
            for (const auto& c : element.data) text += c.character;
            frame->setItem(element.label, text);
        }
    }
    frame->show_all();
}

const std::unordered_set<std::string> validLabels = {
    "Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4",
    "Talon 1", "Talon 2", "Talon 3", "Talon 4",
    "Linear 1", "Linear 2", "Linear 3", "Linear 4",
    "Zed", "Autonomy", "Communication", "Power", "Power2"
};

void addElementToInfoFrame(InfoFrame* frame, const Element& element) {
    frame->addItem(element.label);
    const auto& data = element.data.front();
    switch (element.type) {
        case TYPE::BOOLEAN:   frame->setItem(element.label, data.boolean); break;
        case TYPE::INT8:      frame->setItem(element.label, data.int8); break;
        case TYPE::UINT8:     frame->setItem(element.label, data.uint8); break;
        case TYPE::INT16:     frame->setItem(element.label, data.int16); break;
        case TYPE::UINT16:
            if (element.label == "Bus Voltage" || element.label == "Output Current")
                frame->setItem(element.label, data.uint16 / 100.0f);
            else
                frame->setItem(element.label, data.uint16);
            break;
        case TYPE::INT32:     frame->setItem(element.label, data.int32); break;
        case TYPE::UINT32:    frame->setItem(element.label, data.uint32); break;
        case TYPE::INT64:     frame->setItem(element.label, data.int64); break;
        case TYPE::UINT64:    frame->setItem(element.label, data.uint64); break;
        case TYPE::FLOAT32:   frame->setItem(element.label, data.float32); break;
        case TYPE::FLOAT64:   frame->setItem(element.label, data.float64); break;
        case TYPE::STRING: {
            std::string text;
            for (const auto& c : element.data) text += c.character;
            frame->setItem(element.label, text);
            break;
        }
        default: break;
    }
}


void addElementToInfoFrame(std::string label, InfoFrame* frame, const Element& element) {
    std::map<std::string, bool>& values = getMap(label);
    auto it = values.find(element.label);
    bool end = it == values.end();
    if(it == values.end() || !it->second){
        return;
    }

    addElementToInfoFrame(frame, element);
}

bool allowMotorsDoubleClick = true;


Speedometer* createDial(std::string label, double min_speed, double max_speed, 
                        int major_divisions, int minor_ticks, double zero_angle, double sweep){
    auto speedometer = Gtk::manage(new Speedometer(label));
    speedometer->set_size_request(300, 300);
    speedometer->set_display_speed(displaySpeed);
    speedometer->set_numbers_inside(numbersInside);
    speedometer->set_numbers_on_ticks(numberTicks);
    speedometer->set_min_speed(min_speed);
    speedometer->set_max_speed(max_speed);
    speedometer->set_num_major_divisions(major_divisions);
    speedometer->set_num_minor_ticks_per_segment(minor_ticks);
    speedometer->set_angle_for_zero(zero_angle);
    speedometer->set_angle_for_sweep(sweep);
    speedometer->set_hexpand(false);
    return speedometer;
}

// TODO: Figure out what information should be displayed here and 
// how it should be displayed
void create_motor_detail_window(const std::string& label){
    motorDisplayed = label;
    motorWindow = new Gtk::Window();
    std::string title = label + " Details";
    motorWindow->set_title(title);
    motorWindow->set_default_size(900, 900);
    auto outerBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    auto upperBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    auto lowerBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));

    voltageDial = createDial("Voltage", 14.0, 17.0, 3, 4, 210.0, 120.0);
    voltageDial->set_speed(16.0);
    voltageDial->set_low_warning(true);
    voltageDial->set_low_warning_thresh(0.333);
    voltageDial->set_use_text_label(true);
    voltageDial->set_text_label("Volts DC");
    upperBox->add(*voltageDial);

    temperatureDial = createDial("Temperature", 20.0, 100.0, 8, 4, 180.0, 180.0);
    temperatureDial->set_speed(45.0);
    temperatureDial->set_high_warning(true);
    temperatureDial->set_high_warning_thresh(0.25);
    temperatureDial->set_use_text_label(true);
    temperatureDial->set_text_label("* C");
    upperBox->add(*temperatureDial);

    if(label == "Talon 1" || label == "Talon 3"){
        auto positionBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        positionDial = Gtk::manage(new DrawingArea());
        positionDial->set_size_request(40, 250);
        positionDial->set_hexpand(true);
        positionDial->set_halign(Gtk::ALIGN_CENTER);
        positionDial->show();
        positionDial->set_height_ratio(0.5);
        positionBox->add(*positionDial);
        auto positionLabel = Gtk::manage(new Gtk::Label("Position"));
        positionBox->add(*positionLabel);
        upperBox->add(*positionBox);
    }
    

    percentDial = createDial("Output Percent", 0.0, 100.0, 10, 4, 135.0, 270.0);
    percentDial->set_speed(45.0);
    percentDial->set_high_warning(true);
    percentDial->set_high_warning_thresh(0.1);
    percentDial->set_use_text_label(true);
    percentDial->set_text_label("% Power");
    lowerBox->add(*percentDial);

    velocityDial = createDial("Velocity", 0.0, 10.0, 10, 4, 135.0, 270.0);
    velocityDial->set_speed(5.0);
    lowerBox->add(*velocityDial);

    currentDial = createDial("Output Current", 0.0, 100.0, 10, 4, 135.0, 270.0);
    currentDial->set_speed(45.0);
    currentDial->set_high_warning(true);
    currentDial->set_high_warning_thresh(0.25);
    currentDial->set_use_text_label(true);
    currentDial->set_text_label("Amps");
    lowerBox->add(*currentDial);

    outerBox->add(*upperBox);
    outerBox->add(*lowerBox);

    motorWindow->add(*outerBox);

    motorWindow->signal_hide().connect([]() {
        allowMotorsDoubleClick = true;
        updateMotorDetails = false;
    });

    motorWindow->show_all_children();
    motorWindow->show_all();
}

bool onMotorClick(GdkEventButton* event, const std::string& label){
    if(!allowMotorsDoubleClick)
        return false;
    if (event->type == GDK_2BUTTON_PRESS) {
        allowMotorsDoubleClick = false;
        updateMotorDetails = true;
        create_motor_detail_window(label);
        return true;
    }
    return false;
}

void updateGUI(BinaryMessage& message) {
    std::string label = message.getLabel();

    for (InfoFrame* frame : infoFrameList) {
        if (frame->get_label() != label) continue;

        const auto& elements = message.getObject().elementList;

        if (label == "Zed") {
            handleZedElements(elements);
        }
        else if (label == "Communication") {
            handleCommunicationElements(frame, elements);
        }
        else if (talonLabels.count(label)) {
            handleTalonElements(label, elements);
        }
        else if (falconLabels.count(label)) {
            handleFalconElements(label, elements);
        }
        else if(label == "Autonomy"){
            handleAutonomyElements(label, elements);
        }
        if(updateMotorDetails){
            updateMotor(label, elements);
        }

        handleGenericElements(label, frame, elements);
        return;
    }
    if (!validLabels.count(label)) return;

    if ((label == "Talon 1" || label == "Talon 2") && !arm_init) 
        initArmPos();
    if ((label == "Talon 3" || label == "Talon 4") && !bucket_init) 
        initBucketPos();
    if (label == "Zed" && !roll_init) 
        initRoll();
    if(label == "Zed" && !pitch_init)
        initPitch();

    InfoFrame* infoFrame = Gtk::manage(new InfoFrame(label));
    infoFrameList.push_back(infoFrame);

    for (const Element& element : message.getObject().elementList) {
        addElementToInfoFrame(infoFrame, element);
    }

    Gtk::EventBox* frameBox = Gtk::manage(new Gtk::EventBox());
    frameBox->add(*infoFrame);
    frameBox->show_all();
    if(label == "Talon 1" || label == "Talon 3" ||
       label == "Falcon 1" || label == "Falcon 2" || label == "Falcon 3" || label == "Falcon 4"){
        frameBox->signal_button_press_event().connect(
            [label](GdkEventButton* event) -> bool {
                return onMotorClick(event, label);
            },
            false
        );
    }

    sensorBox->add(*frameBox);
    infoFrame->show_all();
}


// Define element-adding lambdas keyed by prefix
// This creates the binary messages associated with the string
std::map<std::string, std::vector<ElementInfo>> element_definitions = {
    {"TALON", {
        {ElementType::UInt8, "Device ID"},
        {ElementType::UInt16, "Bus Voltage"},
        {ElementType::UInt16, "Output Current"},
        {ElementType::Float32, "Output Percent"},
        {ElementType::Int8, "Sensor Velocity"},
        {ElementType::UInt8, "Temperature"},
        {ElementType::UInt16, "Sensor Position"},
        {ElementType::Float32, "Max Current"}
    }},
    {"FALCON", {
        {ElementType::UInt8, "Device ID"},
        {ElementType::UInt16, "Bus Voltage"},
        {ElementType::UInt16, "Output Current"},
        {ElementType::Float32, "Output Percent"},
        {ElementType::UInt8, "Temperature"},
        {ElementType::UInt16, "Sensor Position"},
        {ElementType::Int8, "Sensor Velocity"},
        {ElementType::Float32, "Max Current"}
    }},
    {"LINEAR", {
        {ElementType::UInt8, "Motor Number"},
        {ElementType::Float32, "Speed"},
        {ElementType::UInt16, "Potentiometer"},
        {ElementType::UInt8, "Time Without Change"},
        {ElementType::UInt16, "Max"},
        {ElementType::UInt16, "Min"},
        {ElementType::String, "Error"},
        {ElementType::Boolean, "At Min"},
        {ElementType::Boolean, "At Max"},
        {ElementType::Float32, "Distance"},
        {ElementType::Boolean, "Sensorless"}
    }},
    {"AUTONOMY", {
        {ElementType::String, "Robot State"},
        {ElementType::String, "Excavation State"},
        {ElementType::String, "Error State"},
        {ElementType::String, "Diagnostics State"},
        {ElementType::String, "Tilt State"},
        {ElementType::String, "Dump State"},
        {ElementType::String, "Level Bucket"},
        {ElementType::String, "Level Arms"},
        {ElementType::Float32, "Dest X"},
        {ElementType::Float32, "Dest Z"}
    }},
    {"ZED", {
        {ElementType::Float32, "X"},
        {ElementType::Float32, "Y"},
        {ElementType::Float32, "Z"},
        {ElementType::Float32, "roll"},
        {ElementType::Float32, "pitch"},
        {ElementType::Float32, "yaw"},
        {ElementType::Boolean, "aruco"}
    }},
    {"COMMUNICATION", {
        {ElementType::Int32, "RSSI"},
        {ElementType::String, "Wi-Fi"},
        {ElementType::String, "CAN Bus"},
        {ElementType::Boolean, "Using CAN1"},
        {ElementType::Int32, "RX packets"},
        {ElementType::Int32, "TX packets"},
        {ElementType::String, "CAN Bus2"},
        {ElementType::Int32, "RX2 packets"},
        {ElementType::Int32, "TX2 packets"},
        {ElementType::String, "Status"}
    }},
    {"POWER", {
        {ElementType::Float32, "Voltage"},
        {ElementType::Float32, "Temp"},
        {ElementType::Float32, "Current 0"},
        {ElementType::Float32, "Current 1"},
        {ElementType::Float32, "Current 2"},
        {ElementType::Float32, "Current 3"},
        {ElementType::Float32, "Current 4"},
        {ElementType::Float32, "Current 5"},
        {ElementType::Float32, "Current 6"}
    }},
    {"POWER2", {
        {ElementType::Float32, "Current 7"},
        {ElementType::Float32, "Current 8"},
        {ElementType::Float32, "Current 9"},
        {ElementType::Float32, "Current 10"},
        {ElementType::Float32, "Current 11"},
        {ElementType::Float32, "Current 12"},
        {ElementType::Float32, "Current 13"},
        {ElementType::Float32, "Current 14"},
        {ElementType::Float32, "Current 15"}
    }}
};


std::string getNameFromPrefix(std::string label){
    if(label.rfind("TALON", 0) == 0){
        return "Talon";
    }
    if(label.rfind("FALCON", 0) == 0){
        return "Falcon";
    }
    if(label.rfind("LINEAR", 0) == 0){
        return "Linear";
    }
    if(label.rfind("AUTONOMY", 0) == 0){
        return "Autonomy";
    }
    if(label.rfind("COMMUNICATION", 0) == 0){
        return "Communication";
    }
    if(label.rfind("POWER2", 0) == 0){
        return "Power2";
    }
    if(label.rfind("POWER", 0) == 0){
        return "Power";
    }
    if(label.rfind("ZED", 0) == 0){
        return "Zed";
    }
    if(label.rfind("TEST", 0) == 0){
        return "Test";
    }
    return "Talon";
}


void populateBinaryMessage(const std::string& name, const std::string& prefix, BinaryMessage& message) {
    std::string vector_name = getNameFromPrefix(prefix);
    auto keys_it = key_vectors.find(vector_name);
    auto defs_it = element_definitions.find(prefix);
    if (keys_it == key_vectors.end() || defs_it == element_definitions.end()) {
        std::cerr << "Warning: Missing keys or definitions for prefix " << prefix << std::endl;
        return;
    }
    const auto& keys = *keys_it->second;
    const auto& defs = defs_it->second;
    std::map<std::string, ElementType> type_map;
    for (const auto& def : defs) {
        type_map[def.name] = def.type;
    }
    for (const std::string& key : keys) {
        auto type_it = type_map.find(key);
        if (type_it == type_map.end()) continue;

        ElementType type = type_it->second;

        if      (type == ElementType::UInt8)   message.addElementUInt8(key, 0);
        else if (type == ElementType::UInt16)  message.addElementUInt16(key, 0);
        else if (type == ElementType::Int8)    message.addElementInt8(key, 0);
        else if (type == ElementType::Int32)   message.addElementInt32(key, 0);
        else if (type == ElementType::Float32) message.addElementFloat32(key, 0.0f);
        else if (type == ElementType::Boolean) message.addElementBoolean(key, false);
        else if (type == ElementType::String)  message.addElementString(key, "");
    }
}


void createMessage(std::string name, std::string prefix){
    BinaryMessage message(name);
    populateBinaryMessage(name, prefix, message);
    updateGUI(message);
}


void initGUI() {
    if(initVals){
        createMessage("Talon 1", "TALON");
        createMessage("Talon 3", "TALON");
        createMessage("Falcon 1", "FALCON");
        createMessage("Falcon 2", "FALCON");
        createMessage("Falcon 3", "FALCON");
        createMessage("Falcon 4", "FALCON");
        
        createMessage("Linear 1", "LINEAR");
        createMessage("Linear 3", "LINEAR");
        
        initRoll();
        initPitch();
        initArmPos();
        initBucketPos();
        
        createMessage("Communication", "COMMUNICATION");
        createMessage("Autonomy", "AUTONOMY");
        createMessage("Zed", "ZED");
        createMessage("Power", "POWER");
        createMessage("Power2", "POWER2");
    }
    
    // Ensure proper initial display
    window->set_default_size(1200, 900);
    window->show_all();
}


void updateGUI(){
    for (InfoFrame* frame : infoFrameList) {
        std::string label = frame->get_label();
        frame->removeAllItems();
        std::map<std::string, bool>& values = getMap(label);
        std::vector<std::string> keys = getKeys(label);
        for (const std::string& key : keys) {
            auto it = values.find(key);
            if (it != values.end() && it->second) {
                frame->addItem(key);
            }
        }
    }

    initGUI();
}


void setDisconnectedState(){
    connectButton->set_label("Connect");
    connectionStatusLabel->set_text("Not Connected");
    silentRunButton->set_label("Silent Running");
    Gdk::RGBA red;
    red.set_rgba(1.0,0,0,1.0);
    connectionStatusLabel->override_background_color(red);
    ipAddressEntry->set_can_focus(true);
    ipAddressEntry->set_editable(true);
    connected=false;
    silentRunning = true;
    initialized = false;

    if (connected) {
        if (sock > 0) {
            if (close(sock) != 0) {
                perror("Failed to close socket");
            }
            sock = 0;
        }
    }

    arm_init = false;
    bucket_init = false;
    roll_init = false;

    if(!noVideo){
        Gdk::RGBA black;
        black.set_rgba(0.0, 0.0, 0.0, 1.0);
        updateCircleColor(talon1Circle, black);
        updateCircleColor(talon3Circle, black);
        updateCircleColor(falcon1Circle, black);
        updateCircleColor(falcon2Circle, black);
        updateCircleColor(falcon3Circle, black);
        updateCircleColor(falcon4Circle, black);
        updateCircleColor(lowerFalcon1Circle, black);
        updateCircleColor(lowerFalcon2Circle, black);
        updateCircleColor(lowerFalcon3Circle, black);
        updateCircleColor(lowerFalcon4Circle, black);
    }
}


void setConnectedState(){
    connectButton->set_label("Disconnect");
    connectionStatusLabel->set_text("Connected");
    Gdk::RGBA green;
    green.set_rgba(0,1.0,0,1.0);
    connectionStatusLabel->override_background_color(green);
    ipAddressEntry->set_can_focus(false);
    ipAddressEntry->set_editable(false);
    connected=true;
}


void setVideoDisconnectedState(){
    videoConnectButton->set_label("Connect");
    videoConnectionStatusLabel->set_text("Not Connected");
    videoStreamButton->set_label("Not Video Streaming");
    Gdk::RGBA red;
    red.set_rgba(1.0,0,0,1.0);
    videoConnectionStatusLabel->override_background_color(red);
    videoIPAddressEntry->set_can_focus(true);
    videoIPAddressEntry->set_editable(true);
    videoConnected=false;

}


void setVideoConnectedState(){
    videoConnectButton->set_label("Disconnect");
    videoConnectionStatusLabel->set_text("Connected");
    Gdk::RGBA green;
    green.set_rgba(0,1.0,0,1.0);
    videoConnectionStatusLabel->override_background_color(green);
    videoIPAddressEntry->set_can_focus(false);
    videoIPAddressEntry->set_editable(false);
    videoConnected=true;
}


void connectToVideoServer(){
    if(videoConnected==true)return;
    struct sockaddr_in address; 
    int bytesRead; 
    struct sockaddr_in serv_addr; 
    std::string hello("Hello Robot"); 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(VIDEO_PORT);

    char buffer[1024] = {0}; 
    if ((videoSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 

        printf("\n Socket creation error \n");

        setVideoDisconnectedState();
        return; 
    } 
    if(inet_pton(AF_INET, videoIPAddressEntry->get_text().c_str(), &serv_addr.sin_addr)<=0)  { 

        printf("\nInvalid address/ Address not supported \n");

        Gtk::MessageDialog dialog(*window,"Invalid Address",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK);
        int result=dialog.run();

        setVideoDisconnectedState();
        return;
    } 
    if(connect(videoSock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");

        Gtk::MessageDialog dialog(*window,"Connection Failed",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK);
        int result=dialog.run();

        setVideoDisconnectedState();
    }
    else{
        send(videoSock , hello.c_str() , strlen(hello.c_str()) , 0 );
        bytesRead = read( videoSock , buffer, 1024);
        fcntl(videoSock,F_SETFL, O_NONBLOCK);

        setVideoConnectedState();
    }
}


void disconnectFromVideoServer(){
    Gtk::MessageDialog dialog(*window,"Disconnect now?",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK_CANCEL);
    //dialog.set_secondary_text("Do you want to shutdown now?");
    int result=dialog.run();

    switch(result) {
        case (Gtk::RESPONSE_OK): 
            if(shutdown(videoSock,SHUT_RDWR)==-1){
                Gtk::MessageDialog dialog(*window,"Failed Shutdown",false,Gtk::MESSAGE_ERROR,Gtk::BUTTONS_OK);
                int result=dialog.run();
            }
            if(close(videoSock)==0){
                setVideoDisconnectedState();
            }
            else{
                Gtk::MessageDialog dialog(*window,"Failed Close",false,Gtk::MESSAGE_ERROR,Gtk::BUTTONS_OK);
                int result=dialog.run();
            }
            break;
        case (Gtk::RESPONSE_CANCEL):
        case (Gtk::RESPONSE_NONE):
        default:
            break;
    }
}


void videoConnectOrDisconnect(){
    Glib::ustring string=videoConnectButton->get_label();
    //std::cout << "connect" << string << std::endl;
    if(string=="Connect"){
        connectToVideoServer();
    }
    else{
        disconnectFromVideoServer();
    }
}


void videoStream(){
    if(!videoConnected)return;
    std::string currentButtonState=videoStreamButton->get_label();
    if(currentButtonState=="Not Video Streaming"){
        int messageSize=3;
        uint8_t command=1;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=1;
        send(videoSock, message, messageSize, 0); 

        videoStreamButton->set_label("Video Streaming");
        isStreamingActive = true;
    }
    else{
        int messageSize=3;
        uint8_t command=1;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=0;
        send(videoSock, message, messageSize, 0); 

        videoStreamButton->set_label("Not Video Streaming");
        isStreamingActive = true;
    }
}


void videoRowActivated(Gtk::ListBoxRow* listBoxRow){
    Gtk::Label* label=static_cast<Gtk::Label*>(listBoxRow->get_child());
    Glib::ustring connectionString(label->get_text());
    int index=connectionString.rfind('@');
    if(index==-1)return;
    ++index;
    Glib::ustring addressString=connectionString.substr(index,connectionString.length()-index);
    videoIPAddressEntry->set_text(addressString);
}



Gtk::ScrolledWindow* create_gear_dial(const std::vector<std::string>& gears,
                                      std::map<std::string, Gtk::Label*>& gear_labels,
                                      Gtk::Box*& label_container)
{
    auto* scroll = new Gtk::ScrolledWindow();
    scroll->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scroll->set_propagate_natural_height(true);
    scroll->set_size_request(80, 120); // Dial size

    label_container = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
    scroll->add(*label_container);

    for (const auto& gear : gears) {
        auto* label = Gtk::manage(new Gtk::Label(gear));
        label->set_margin_top(8);
        label->set_margin_bottom(8);
        label->set_alignment(0.5, 0.5);
        label->set_markup("<span size='8000' foreground='gray'>" + gear + "</span>");

        gear_labels[gear] = label;
        label_container->pack_start(*label, Gtk::PACK_SHRINK);
    }

    return scroll;
}


void highlight_gear_and_scroll(const std::string& current_gear,
                               const std::vector<std::string>& gears,
                               const std::map<std::string, Gtk::Label*>& gear_labels,
                               Gtk::ScrolledWindow* scroll,
                               Gtk::Box* label_container)
{
    int gear_index = 0;
    for (size_t i = 0; i < gears.size(); ++i) {
        const auto& gear = gears[i];
        auto* label = gear_labels.at(gear);

        if (gear == current_gear) {
            label->set_markup("<span size='12000' weight='bold' background='red' foreground='white'>" + gear + "</span>");
            gear_index = i;
        } else {
            label->set_markup("<span size='8000' foreground='gray'>" + gear + "</span>");
        }
    }

    // Scroll so current gear is in the middle
    auto adj = scroll->get_vadjustment();
    double row_height = 30.0; // Approximate
    double new_value = std::max(0.0, gear_index * row_height - scroll->get_height() / 2);
    adj->set_value(new_value);
}

std::vector<std::string> gears = {"M", "5", "4", "3", "2", "1"};
std::map<std::string, Gtk::Label*> gear_labels;
Gtk::Box* gear_label_box = nullptr;
Gtk::ScrolledWindow* gear_dial = nullptr;
std::string currentGear = "3";

void increaseGear(){
    auto it = std::find(gears.begin(), gears.end(), currentGear);
    if (it != gears.begin()) {
        std::string nextGear = *std::prev(it);  // Increase gear
        currentGear = nextGear;
        highlight_gear_and_scroll(currentGear, gears, gear_labels, gear_dial, gear_label_box);
    } else {
        std::cout << "Already at highest gear." << std::endl;
    }
}

void decreaseGear(){
    auto it = std::find(gears.begin(), gears.end(), currentGear);
    if (it != gears.end() && std::next(it) != gears.end()) {
        std::string nextGear = *std::next(it);  // Decrease gear
        currentGear = nextGear;
        highlight_gear_and_scroll(currentGear, gears, gear_labels, gear_dial, gear_label_box);
    } else {
        std::cout << "Already at lowest gear." << std::endl;
    }
}


// Server address
struct sockaddr_in serv_addr; 
socklen_t addr_len = sizeof(serv_addr);
std::chrono::high_resolution_clock::time_point lastHeartbeatTime;

//UDP Version
void connectToServer(){
    if(connected==true) return;

    std::string hello("Hello Robot"); 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT);

    if(useOrin) {
        if(inet_pton(AF_INET, ORIN_IP, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid ORIN_IP" << std::endl;
            return;
        }
    }
    else {
        if(inet_pton(AF_INET, NANO_IP, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid NANO_IP" << std::endl;
            return;
        }
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        perror("Socket creation error");
        setDisconnectedState();
        return; 
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sendto(sock, hello.c_str(), hello.length(), 0, (struct sockaddr *)&serv_addr, addr_len);
    std::cout << "Hello sent to server." << std::endl;

    auto startTime = std::chrono::steady_clock::now();
    bool replyReceived = false;
    char buffer[2048] = {0};
    int bytesRead = 0;

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count() < 2) {
        bytesRead = recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&serv_addr, &addr_len);
        if (bytesRead > 0) {
            replyReceived = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    if (replyReceived) {
        std::cout << "Received reply from server. Connection established." << std::endl;
        setConnectedState();
        ipAddressEntry->set_text(inet_ntoa(serv_addr.sin_addr));
        initialized = true;
        lastHeartbeatTime = std::chrono::high_resolution_clock::now();
    }
    else {
        std::cout << "Did not receive reply from server (timeout). Connection failed." << std::endl;
        setDisconnectedState();
        close(sock);
        sock = 0;
    }
}


void disconnectFromServer(){
    Gtk::MessageDialog dialog(*window,"Disconnect now?",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK_CANCEL);
    //dialog.set_secondary_text("Do you want to shutdown now?");
    int result=dialog.run();

    switch(result) {
        case (Gtk::RESPONSE_OK): 
            if(close(sock)==0){
                setDisconnectedState();
            }
            else{
                Gtk::MessageDialog dialog(*window,"Failed Close",false,Gtk::MESSAGE_ERROR,Gtk::BUTTONS_OK);
                int result=dialog.run();
            }
            break;
        case (Gtk::RESPONSE_CANCEL):
        case (Gtk::RESPONSE_NONE):
        default:
            break;
    }
}


void connectOrDisconnect(){
    Glib::ustring string=connectButton->get_label();
    //std::cout << "connect" << string << std::endl;
    if(string=="Connect"){
        connectToServer();
    }
    else{
        disconnectFromServer();
    }
}


void silentRun(){
    if(!connected)return;
    std::string currentButtonState=silentRunButton->get_label();
    if(currentButtonState=="Silent Running"){
        int messageSize=3;
        uint8_t command=7;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=0;
        // send(sock, message, messageSize, 0);
        sendto(sock , message , messageSize , 0 ,(struct sockaddr *)&serv_addr, addr_len);


        silentRunButton->set_label("Not Silent Running");
        silentRunning = false;
    }
    else{
        int messageSize=3;
        uint8_t command=7;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=1;
        // send(sock, message, messageSize, 0); 
        sendto(sock , message , messageSize , 0 ,(struct sockaddr *)&serv_addr, addr_len);

        silentRunButton->set_label("Silent Running");
        silentRunning = true;
    }
}


void rowActivated(Gtk::ListBoxRow* listBoxRow){
    Gtk::Label* label=static_cast<Gtk::Label*>(listBoxRow->get_child());
    Glib::ustring connectionString(label->get_text());
    int index=connectionString.rfind('@');
    if(index==-1)return;
    ++index;
    Glib::ustring addressString=connectionString.substr(index,connectionString.length()-index);
    ipAddressEntry->set_text(addressString);
}


void shutdownRobot(){
    int messageSize=2;
    uint8_t command=8;// shutdown
    uint8_t message[messageSize];
    message[0]=messageSize;
    message[1]=command;
    // send(sock, message, messageSize, 0);
    sendto(sock , message , messageSize , 0 ,(struct sockaddr *)&serv_addr, addr_len);
}


void shutdownDialog(Gtk::Window* parentWindow){
    Gtk::MessageDialog dialog(*parentWindow,"Shutdown now?",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK_CANCEL);
    int result=dialog.run();

    switch(result) {
        case (Gtk::RESPONSE_OK):
            shutdownRobot();
            break;
        case (Gtk::RESPONSE_CANCEL):
        case (Gtk::RESPONSE_NONE):
        default:
            break;
    }
}

std::string current_ip = "http://192.168.1.8";

void send_servo_command(const std::string& direction) {
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string url = current_ip + "/action?go=" + direction;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);  // Short timeout
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
    }
}


bool on_key_release_event(GdkEventKey* key_event){
    switch (key_event->keyval) {
        case GDK_KEY_u:
        case GDK_KEY_i:
        case GDK_KEY_o:
        case GDK_KEY_p:
            send_servo_command("stop");
            return false;
            break;
    }
    int messageSize=5;
    uint8_t command=2;// keyboard
    uint8_t message[messageSize];
    message[0]=messageSize;
    message[1]=command;
    message[2]=(uint8_t)(((key_event->keyval)>>8)& 0xff);
    message[3]=(uint8_t)(((key_event->keyval)>>0)& 0xff);
    message[4]=0;
    // send(sock, message, messageSize, 0);
    sendto(sock , message , messageSize , 0 ,(struct sockaddr *)&serv_addr, addr_len);


    return false;
}


bool on_key_press_event(GdkEventKey* key_event){
    switch (key_event->keyval) {
        case GDK_KEY_u:
            send_servo_command("left");
            return false;
            break;
        case GDK_KEY_i:
            send_servo_command("right");
            return false;
            break;
        case GDK_KEY_o:
            send_servo_command("up");
            return false;
            break;
        case GDK_KEY_p:
            send_servo_command("down");
            return false;
            break;
        case GDK_KEY_1:
            current_ip = "http://192.168.1.8";
            std::cout << "Switched to IP 1: " << current_ip << std::endl;
            return false;
            break;
        case GDK_KEY_2:
            current_ip = "http://192.168.1.9";
            std::cout << "Switched to IP 2: " << current_ip << std::endl;
            return false;
            break;
        case GDK_KEY_minus:
            decreaseGear();
            break;
        case GDK_KEY_plus:
            if(key_event->state & GDK_SHIFT_MASK)
                increaseGear();
            break;
    }

    int messageSize=5;
    uint8_t command=2;// keyboard
    uint8_t message[messageSize];
    message[0]=messageSize;
    message[1]=command;
    message[2]=(uint8_t)(((key_event->keyval)>>8)& 0xff);
    message[3]=(uint8_t)(((key_event->keyval)>>0)& 0xff);
    message[4]=1;
    // send(sock, message, messageSize, 0);
    sendto(sock , message , messageSize , 0 ,(struct sockaddr *)&serv_addr, addr_len);


    return false;
}


Gtk::EventBox* create_labeled_box(const Glib::ustring& label_text, CircleDrawingArea*& out_circle, bool right = false) {
    auto event_box = Gtk::manage(new Gtk::EventBox());

    auto box = Gtk::manage(new BorderedBox(Gtk::ORIENTATION_HORIZONTAL, 5));
    box->set_size_request(300, 75);

    auto label = Gtk::manage(new Gtk::Label(label_text));
    label->set_hexpand(true);

    Pango::FontDescription font;
    font.set_size(20 * Pango::SCALE);
    label->override_font(font);

    out_circle = Gtk::manage(new CircleDrawingArea());
    out_circle->set_size_request(75, 75);
    out_circle->set_hexpand(false);
    out_circle->set_halign(Gtk::ALIGN_CENTER);

    if(right){
        box->add(*label);
        box->add(*out_circle);
    }
    else{
        box->add(*out_circle);
        box->add(*label);
    }   

    event_box->add(*box);
    event_box->add_events(Gdk::BUTTON_PRESS_MASK);
    event_box->set_visible_window(false);

    return event_box;
}



Gtk::EventBox* create_box(const Glib::ustring& label_text, CircleDrawingArea*& out_circle, bool right = false) {
    auto event_box = Gtk::manage(new Gtk::EventBox());

    auto box = Gtk::manage(new BorderedBox(Gtk::ORIENTATION_HORIZONTAL, 5));
    box->set_size_request(200, 75);

    auto label = Gtk::manage(new Gtk::Label(label_text));
    label->set_hexpand(true);

    Pango::FontDescription font;
    font.set_size(20 * Pango::SCALE);
    label->override_font(font);

    out_circle = Gtk::manage(new CircleDrawingArea());
    out_circle->set_size_request(75, 75);
    out_circle->set_hexpand(false);
    out_circle->set_halign(Gtk::ALIGN_CENTER);

    if(right){
        box->add(*label);
        box->add(*out_circle);
    }
    else{
        box->add(*out_circle);
        box->add(*label);
    }   

    event_box->add(*box);
    event_box->add_events(Gdk::BUTTON_PRESS_MASK);
    event_box->set_visible_window(false);

    return event_box;
}


bool onClickEvent(GdkEventButton* event, const std::string& id) {
    if (event->type == GDK_2BUTTON_PRESS) {
        auto target_infoframe = getInfoFrame(id);
        Gtk::FlowBoxChild* flowbox_child = dynamic_cast<Gtk::FlowBoxChild*>(get_flowbox_child_for(*sensorBox, target_infoframe));
        if (flowbox_child) {
            sensorBox->select_child(*flowbox_child);
        }
        return true;
    }
    return false;
}


Gtk::Box* create_motor_column(std::vector<std::pair<Glib::ustring, CircleDrawingArea**>> items, void (*init_hook)(), std::vector<std::string> labels, bool right = false) {
    auto column = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    column->set_size_request(300, 300);
    column->set_hexpand(false);
    column->set_vexpand(false);

    for (size_t i = 0; i < items.size(); ++i) {
        if (i == 2 && init_hook) init_hook();
        auto box = create_labeled_box(items[i].first, *items[i].second, right);
        std::string id = labels[i];
        box->signal_button_press_event().connect(
            [id](GdkEventButton* event) -> bool {
                return onClickEvent(event, id);
            },
            false
        );
        column->add(*box);
    }

    return column;
}


// To change Speedometer sizes, need to change this value
Gtk::Box* create_lower_motor_column(std::vector<std::pair<Glib::ustring, CircleDrawingArea**>> items, std::vector<std::string> labels, bool right = false) {
    auto column = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    column->set_size_request(200, 300);
    column->set_hexpand(false);
    column->set_vexpand(false);

    for (size_t i = 0; i < items.size(); ++i) {
        auto box = create_labeled_box(items[i].first, *items[i].second, right);
        std::string id = labels[i];
        box->signal_button_press_event().connect(
            [id](GdkEventButton* event) -> bool {
                return onClickEvent(event, id);
            },
            false
        );
        column->add(*box);
    }

    return column;
}


Gdk::RGBA parse_color(const std::string& color_str) {
    Gdk::RGBA color;
    color.set(color_str);
    return color;
}


std::string to_color_string(const Gdk::RGBA& color) {
    return color.to_string();
}


std::map<std::string, std::string> tooltip_map = {
    {"DISPLAY_SPEED", "Show or hide the speedometer."},
    {"NUMBERS_INSIDE", "Display numbers inside the speedometer ring."},
    {"NUMBER_TICKS", "Align numbers with speedometer tick marks."},
    {"SHOW_FALCON_Device ID", "Show Falcon CAN ID in the telemetry frame."}
};


void add_tooltip(Gtk::CheckButton* check, const std::string& key) {
    auto it = tooltip_map.find(key);
    if (it != tooltip_map.end()) {
        check->set_tooltip_text(it->second);
    }
}


void save_value(std::map<std::string, bool>& values, const std::string& value, bool active) {
    values[value] = active;
}


InfoFrame *talonFrame = nullptr, *falconFrame = nullptr, *linearFrame = nullptr,
    *autonomyFrame = nullptr, *zedFrame = nullptr, *communicationFrame = nullptr,
    *powerFrame = nullptr, *power2Frame = nullptr;

std::unordered_map<std::string, InfoFrame*> frame_map;
void setup_frame_map() {
    frame_map = {
        {"TALON",         talonFrame},
        {"FALCON",        falconFrame},
        {"LINEAR",        linearFrame},
        {"AUTONOMY",      autonomyFrame},
        {"ZED",           zedFrame},
        {"COMMUNICATION", communicationFrame},
        {"POWER",         powerFrame},
        {"POWER2",        power2Frame},
    };
}

class ListColumns : public Gtk::TreeModel::ColumnRecord {
public:
    ListColumns() {
        add(col_active);
        add(col_text);
        add(col_key);
    }
    Gtk::TreeModelColumn<bool> col_active;
    Gtk::TreeModelColumn<Glib::ustring> col_text;
    Gtk::TreeModelColumn<Glib::ustring> col_key;
};

ListColumns columns;

std::map<std::string, Gtk::CheckButton*> bool_buttons;
std::vector<std::string> local_talon_keys = talon_keys;
std::vector<std::string> local_falcon_keys = falcon_keys;
std::vector<std::string> local_linear_keys = linear_keys;
std::vector<std::string> local_autonomy_keys = autonomy_keys;
std::vector<std::string> local_communication_keys = communication_keys;
std::vector<std::string> local_power2_keys = power2_keys;
std::vector<std::string> local_power_keys = power_keys;
std::vector<std::string> local_zed_keys = zed_keys;

std::map<std::string, std::vector<std::string>*> local_key_vectors = {
    {"Talon", &local_talon_keys},
    {"Falcon", &local_falcon_keys},
    {"Linear", &local_linear_keys},
    {"Autonomy", &local_autonomy_keys},
    {"Communication", &local_communication_keys},
    {"Power2", &local_power2_keys},
    {"Power", &local_power_keys},
    {"Zed", &local_zed_keys}
};


bool allowConfig = true;

void create_config_editor_window(const std::string& config_file) {
    allowConfig = false;
    configWindow = new Gtk::Window();
    configWindow->set_title("Configuration Editor");
    configWindow->set_default_size(1000, 600);
    auto scrolledWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolledWindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    std::map<std::string, Glib::RefPtr<Gtk::ListStore>> list_stores;

    auto main_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    auto grid = Gtk::make_managed<Gtk::Grid>();
    auto save_button = Gtk::make_managed<Gtk::Button>("Save");
    save_button->set_name("dark_text");
    auto reset_button = Gtk::make_managed<Gtk::Button>("Reset");
    reset_button->set_name("dark_text");
    auto light_color_button = Gtk::make_managed<Gtk::ColorButton>();
    auto dark_color_button = Gtk::make_managed<Gtk::ColorButton>();
    auto file_entry = Gtk::make_managed<Gtk::Entry>();
    file_entry->set_text(config_file);

    std::string lightBackground;
    std::string darkBackground;
    Speedometer* testSpeedometer = new Speedometer("Test Speedometer");
    testSpeedometer->set_size_request(200, 75);
    testSpeedometer->set_display_speed(displaySpeed);
    testSpeedometer->set_numbers_inside(numbersInside);
    testSpeedometer->set_numbers_on_ticks(numberTicks);
    auto outer_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    auto speed_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
    auto speed_options_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    speed_box->set_size_request(250, 250);
    speed_box->add(*testSpeedometer);
    speed_box->add(*speed_options_box);

    grid->attach(*Gtk::make_managed<Gtk::Label>("Config File:"), 0, 0, 1, 1);
    grid->attach(*file_entry, 1, 0, 1, 1);

    // Load config
    std::ifstream file("../resources/" + config_file);
    std::string line;
    int row = 2;

    std::map<std::string, bool> config_values;

    Gtk::FlowBox* sensorsBox = Gtk::manage(new Gtk::FlowBox());
    sensorsBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    sensorsBox->set_size_request(1000, -1);
    

    auto populateBinaryMessage = [&](const std::string& prefix, BinaryMessage& message) {
        std::string name = getNameFromPrefix(prefix);
        auto keys_it = local_key_vectors.find(name);
        auto defs_it = element_definitions.find(prefix);
        if (keys_it == local_key_vectors.end() || defs_it == element_definitions.end()) {
            std::cerr << "Warning: Missing keys or definitions for prefix " << prefix << std::endl;
            return;
        }
        const auto& keys = *keys_it->second;
        const auto& defs = defs_it->second;
        std::map<std::string, ElementType> type_map;
        for (const auto& def : defs) {
            type_map[def.name] = def.type;
        }
        for (const std::string& key : keys) {
            auto type_it = type_map.find(key);
            if (type_it == type_map.end()) continue;

            ElementType type = type_it->second;

            if      (type == ElementType::UInt8)   message.addElementUInt8(key, 0);
            else if (type == ElementType::UInt16)  message.addElementUInt16(key, 0);
            else if (type == ElementType::Int8)    message.addElementInt8(key, 0);
            else if (type == ElementType::Int32)   message.addElementInt32(key, 0);
            else if (type == ElementType::Float32) message.addElementFloat32(key, 0.0f);
            else if (type == ElementType::Boolean) message.addElementBoolean(key, false);
            else if (type == ElementType::String)  message.addElementString(key, "");
        }
    };

    // Lambda to get the bool values of the map given by the prefix, then adds the element to the InfoFrame
    auto addConditionalElements = [&](const std::string& prefix, BinaryMessage& msg, InfoFrame* frame) {
        std::string label = getNameFromPrefix(prefix);
        std::map<std::string, bool>& values = getMap(label);
        for (const Element& el : msg.getObject().elementList) {
            std::string key = "SHOW_" + prefix + "_" + el.label;
            auto it = values.find(el.label);
            if(it == values.end() || !it->second)
                continue;

            addElementToInfoFrame(frame, el);
        }
    };

    // Single generic frame creation function
    // Given a name, prefix creates frame, then adds the elements to the 
    auto createFrame = [&](const std::string& name, const std::string& prefix, InfoFrame*& frameRef, Gtk::Box* box) {
        BinaryMessage message(name);
        populateBinaryMessage(prefix, message);

        frameRef = Gtk::manage(new InfoFrame(name));
        addConditionalElements(prefix, message, frameRef);

        box->add(*frameRef);
        frameRef->show_all();
    };

    // Lambda to get the InfoFrame associated with the passed prefix
    auto get_info_frame_for_prefix = [&](const std::string& prefix) -> InfoFrame* {
        auto it = frame_map.find(prefix);
        return (it != frame_map.end()) ? it->second : talonFrame;
    };

    // Lambda to reset the order of the items in the frame
    // TODO: Update draggable items and checkboxes
    auto reset_frame = [&](std::string prefix){
        InfoFrame* frameRef = get_info_frame_for_prefix(prefix);
        frameRef->removeAllItems();
        std::string messageName = getNameFromPrefix(prefix);
        BinaryMessage message(messageName);    
        populateBinaryMessage(prefix, message);
        std::map<std::string, bool>& values = getMap(messageName);
        for (const Element& el : message.getObject().elementList) {
            std::string key = "SHOW_" + prefix + "_" + el.label;
            auto it = values.find(el.label);
            if(it == values.end())
                continue;
            it->second = true;
        }
        addConditionalElements(prefix, message, frameRef);
        frameRef->show_all();
    };

    // Define frames and boxes
    Gtk::Box *talonBox = nullptr, *falconBox = nullptr, *linearBox = nullptr,
            *autonomyBox = nullptr, *zedBox = nullptr, *communicationBox = nullptr,
            *powerBox = nullptr, *power2Box = nullptr;

    InfoFrame *optionsTalonFrame = nullptr, *optionsFalconFrame = nullptr, *optionsLinearFrame = nullptr,
            *optionsAutonomyFrame = nullptr, *optionsZedFrame = nullptr, *optionsCommunicationFrame = nullptr,
            *optionsPowerFrame = nullptr, *optionsPower2Frame = nullptr;

    // Frame entry struct
    struct FrameEntry {
        std::string label; // Label to put on the Infoframe
        std::string prefix; // Prefix to prepend to all of the options to differentiate
        InfoFrame** frame_ptr; // Frame to display the items in the InfoFrame
        Gtk::Box** box_ptr; // Box to hold both the display frame and the options frame
        InfoFrame** options_frame_ptr; // Options InfoFrame
    };

    std::vector<FrameEntry> frame_entries = {
        {"Talon",         "TALON",         &talonFrame,         &talonBox,         &optionsTalonFrame},
        {"Falcon",        "FALCON",        &falconFrame,        &falconBox,        &optionsFalconFrame},
        {"Linear",        "LINEAR",        &linearFrame,        &linearBox,        &optionsLinearFrame},
        {"Autonomy",      "AUTONOMY",      &autonomyFrame,      &autonomyBox,      &optionsAutonomyFrame},
        {"Zed",           "ZED",           &zedFrame,           &zedBox,           &optionsZedFrame},
        {"Communication", "COMMUNICATION", &communicationFrame, &communicationBox, &optionsCommunicationFrame},
        {"Power",         "POWER",         &powerFrame,         &powerBox,         &optionsPowerFrame},
        {"Power2",        "POWER2",        &power2Frame,        &power2Box,        &optionsPower2Frame},
    };

    // Create all frames & boxes in a loop
    for (auto& entry : frame_entries) {
        *(entry.box_ptr) = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
        createFrame(entry.label, entry.prefix, *(entry.frame_ptr), *(entry.box_ptr));

        *(entry.options_frame_ptr) = Gtk::manage(new InfoFrame(entry.prefix.substr(0, 1) + entry.prefix.substr(1) + " Options"));
        (*(entry.box_ptr))->add(*(*(entry.options_frame_ptr)));

        sensorsBox->add(*(*(entry.box_ptr)));
    }

    setup_frame_map();

    auto create_reorderable_checkbox_list = [&](const std::string& prefix, std::vector<std::string> keys, std::map<std::string, bool>& items_map,
                                            Glib::RefPtr<Gtk::ListStore>& list_store_out) -> Gtk::Widget* {
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
        auto scrolled_window = Gtk::make_managed<Gtk::ScrolledWindow>();
        auto tree_view = Gtk::make_managed<Gtk::TreeView>();

        auto list_store = Gtk::ListStore::create(columns);
        list_store_out = list_store;

        tree_view->set_model(list_store);
        tree_view->set_reorderable(true);

        tree_view->enable_model_drag_source();
        tree_view->enable_model_drag_dest();


        // Checkbox column
        auto cell_toggle = Gtk::make_managed<Gtk::CellRendererToggle>();
        cell_toggle->property_activatable() = true;
        int col_index_toggle = tree_view->append_column("Active", *cell_toggle);
        if (auto col_toggle = tree_view->get_column(col_index_toggle - 1)) {
            col_toggle->add_attribute(cell_toggle->property_active(), columns.col_active);
        }

        // Text column with autosizing
        int col_index_text = tree_view->append_column("Item", columns.col_text);
        if (auto col_text = tree_view->get_column(col_index_text - 1)) {
            col_text->set_resizable(true);
            col_text->set_expand(true);
            col_text->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
        }

        // Scrolling and expansion
        scrolled_window->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        scrolled_window->set_min_content_height(300);
        scrolled_window->set_hexpand(true);
        scrolled_window->set_vexpand(true);
        tree_view->set_hexpand(true);

        // Fill list store from map
        for(const auto& key: keys){
            auto row = *(list_store->append());
            row[columns.col_text] = key;
            auto it = items_map.find(key);
            if (it != items_map.end()) {
                row[columns.col_active] = it->second;
            }
            row[columns.col_key] = prefix + "_" + key;
        }

        // Sync checkbox toggle with map
        cell_toggle->signal_toggled().connect([list_store, prefix, &items_map, &get_info_frame_for_prefix, &addConditionalElements, &populateBinaryMessage, &reset_frame](const Glib::ustring& path) {
            if (auto iter = list_store->get_iter(path)) {
                bool active = !(*iter)[columns.col_active];
                (*iter)[columns.col_active] = active;
                std::string key = Glib::ustring((*iter)[columns.col_text]).raw();
                items_map[key] = active;
                InfoFrame* frameRef = get_info_frame_for_prefix(prefix);
                if (active) {
                    InfoFrame* frameRef = get_info_frame_for_prefix(prefix);
                    frameRef->removeAllItems();
                    std::string messageName = getNameFromPrefix(prefix);
                    BinaryMessage message(messageName);    
                    populateBinaryMessage(prefix, message);
                    addConditionalElements(prefix, message, frameRef);
                    frameRef->show_all();
                }
                else {
                    frameRef->removeItem(key);
                    frameRef->show_all();
                }
            }
        });
        
        tree_view->signal_drag_end().connect([prefix, list_store, &get_info_frame_for_prefix, &addConditionalElements, &populateBinaryMessage, &reset_frame](const Glib::RefPtr<Gdk::DragContext>& context) {
            // Create a new vector to store the new order
            std::vector<std::string> new_order;

            // Iterate over list_store rows in visual order
            for (auto iter = list_store->children().begin(); iter != list_store->children().end(); ++iter) {
                auto row = *iter;
                std::string key = Glib::ustring(row[columns.col_text]).raw();
                new_order.push_back(key);
            }
            std::string name = getNameFromPrefix(prefix);
            auto it = local_key_vectors.find(name);
            if (it != local_key_vectors.end() && it->second) {
                *(it->second) = new_order;  // Replace contents with new order
            }
            else {
                std::cerr << "Warning: prefix '" << prefix << "' not found in local_key_vectors." << std::endl;
            }
            InfoFrame* frameRef = get_info_frame_for_prefix(prefix);
            frameRef->removeAllItems();
            std::string messageName = getNameFromPrefix(prefix);
            BinaryMessage message(messageName);    
            populateBinaryMessage(prefix, message);
            addConditionalElements(prefix, message, frameRef);
            frameRef->show_all();
        });

        scrolled_window->add(*tree_view);
        box->pack_start(*scrolled_window, Gtk::PACK_EXPAND_WIDGET);

        return box;
    };


    Gtk::Widget* talonWidget = create_reorderable_checkbox_list("TALON", talon_keys, talon_values, list_stores["TALON"]);
    optionsTalonFrame->addWidget(*talonWidget);
    Gtk::Widget* falconWidget = create_reorderable_checkbox_list("FALCON", falcon_keys, falcon_values, list_stores["FALCON"]);
    optionsFalconFrame->addWidget(*falconWidget);
    Gtk::Widget* linearWidget = create_reorderable_checkbox_list("LINEAR", linear_keys, linear_values, list_stores["LINEAR"]);
    optionsLinearFrame->addWidget(*linearWidget);
    Gtk::Widget* autonomyWidget = create_reorderable_checkbox_list("AUTONOMY", autonomy_keys, autonomy_values, list_stores["AUTONOMY"]);
    optionsAutonomyFrame->addWidget(*autonomyWidget);
    Gtk::Widget* zedWidget = create_reorderable_checkbox_list("ZED", zed_keys, zed_values, list_stores["ZED"]);
    optionsZedFrame->addWidget(*zedWidget);
    Gtk::Widget* communicationWidget = create_reorderable_checkbox_list("COMMUNICATION", communication_keys, communication_values, list_stores["COMMUNICATION"]);
    optionsCommunicationFrame->addWidget(*communicationWidget);
    Gtk::Widget* powerWidget = create_reorderable_checkbox_list("POWER", power_keys, power_values, list_stores["POWER"]);
    optionsPowerFrame->addWidget(*powerWidget);
    Gtk::Widget* power2Widget = create_reorderable_checkbox_list("POWER2", power2_keys, power2_values, list_stores["POWER2"]);
    optionsPower2Frame->addWidget(*power2Widget);

    // Lambda to create the color option picker and add it to the grid
    auto add_color_setting = [&](const std::string& label_text, const std::string& color_value, Gtk::ColorButton* color_button) {
        auto label = Gtk::make_managed<Gtk::Label>(label_text + ":");
        color_button->set_rgba(parse_color(color_value));
        grid->attach(*label, 0, row, 1, 1);
        grid->attach(*color_button, 1, row, 1, 1);
        row++;
    };

    // Lambda to add the on click functionality to the speedometer options
    auto connect_speedometer_toggle = [&](Gtk::CheckButton* check, const std::string& key) {
        check->signal_toggled().connect([=]() mutable {
            bool active = check->get_active();
            if (key == "DISPLAY_SPEED") {
                displaySpeed = active;
                testSpeedometer->set_display_speed(active);
            }
            else if (key == "NUMBERS_INSIDE") {
                numbersInside = active;
                testSpeedometer->set_numbers_inside(active);
            }
            else if (key == "NUMBER_TICKS") {
                numberTicks = active;
                testSpeedometer->set_numbers_on_ticks(active);
            }
            if(testSpeedometer)testSpeedometer->queue_draw();

        });
    };


    auto update_value = [&](const std::string& prefix, const std::string& key, bool active) {
        std::string full_key = "SHOW_" + prefix + "_" + key;
        std::string label = getNameFromPrefix(prefix);
        std::map<std::string, bool>& values = getMap(label);
        auto it = values.find(key);
        if(it != values.end()){
            it->second = active;
        }

        std::string col_key = prefix + "_" + key;
        auto it_store = list_stores.find(prefix);
        if (it_store != list_stores.end()) {
            auto store = it_store->second;
            for (auto iter = store->children().begin(); iter != store->children().end(); ++iter) {
                if ((*iter)[columns.col_key] == col_key) {
                    (*iter)[columns.col_active] = active;
                    auto row = *iter;
                    Glib::ustring text = row[columns.col_text];
                    bool active_val = row[columns.col_active];
                    Glib::ustring key_val = row[columns.col_key];
                    store->erase(iter);
                    auto new_iter = store->append();
                    (*new_iter)[columns.col_text] = text;
                    (*new_iter)[columns.col_active] = active_val;
                    (*new_iter)[columns.col_key] = key_val;
                    break;
                }
            }
        }
        std::string name = getNameFromPrefix(prefix);
        auto it_vec = local_key_vectors.find(name);
        if (it_vec != local_key_vectors.end() && it_vec->second) {
            auto& vec = *(it_vec->second);
            auto pos = std::find(vec.begin(), vec.end(), key);
            if (pos != vec.end()) {
                vec.erase(pos);       // Remove from old position
            }
            vec.push_back(key);       // Add to end
        }
        InfoFrame* frameRef = get_info_frame_for_prefix(prefix);
        frameRef->removeAllItems();
        std::string messageName = getNameFromPrefix(prefix);
        BinaryMessage message(messageName);    
        populateBinaryMessage(prefix, message);
        addConditionalElements(prefix, message, frameRef);
        frameRef->show_all();
    };

    // Lambda that finds the prefix and then creates a checkbox witht that prefix and
    // option
    auto setup_info_toggle_if_needed = [&](const std::string& key, bool active) {
        if (key.rfind("SHOW_TALON_", 0) == 0) {
            update_value("TALON", key.substr(11), active);
        }
        else if (key.rfind("SHOW_FALCON_", 0) == 0) {
            update_value("FALCON", key.substr(12), active);
        }
        else if (key.rfind("SHOW_LINEAR_", 0) == 0) {
            update_value("LINEAR", key.substr(12), active);
        }
        else if (key.rfind("SHOW_AUTONOMY_", 0) == 0) {
            update_value("AUTONOMY", key.substr(14), active);
        }
        else if (key.rfind("SHOW_ZED_", 0) == 0) {
            update_value("ZED", key.substr(9), active);
        }
        else if (key.rfind("SHOW_COMMUNICATION_", 0) == 0) {
            update_value("COMMUNICATION", key.substr(19), active);
        }
        else if (key.rfind("SHOW_POWER_", 0) == 0) {
            update_value("POWER", key.substr(11), active);
        }
        else if (key.rfind("SHOW_POWER2_", 0) == 0) {
            update_value("POWER2", key.substr(12), active);
        }
    };

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string key, value;
        if (!(std::getline(ss, key, '=') && std::getline(ss, value))) continue;
        if (key == "LIGHT_BACKGROUND") {
            lightBackground = value;
            add_color_setting("Light Background Color", value, light_color_button);
        }
        else if (key == "DARK_BACKGROUND") {
            darkBackground = value;
            add_color_setting("Dark Background Color", value, dark_color_button);
        }
        else {
            if (key == "DISPLAY_SPEED" || key == "NUMBERS_INSIDE" || key == "NUMBER_TICKS") {
                auto check = Gtk::make_managed<Gtk::CheckButton>(key);
                check->set_active(value == "true");
                bool_buttons[key] = check;
                speed_options_box->add(*check);
                add_tooltip(check, key);
                connect_speedometer_toggle(check, key);
            }
            else {
                setup_info_toggle_if_needed(key, value == "true");
            }
        }
    }

    auto it = bool_buttons.find("DISPLAY_SPEED");
    if(it == bool_buttons.end()){
        lightBackground = "#FFFFFF";
        add_color_setting("Light Background Color", "#FFFFFF", light_color_button);
        darkBackground = "#000000";
        add_color_setting("Dark Background Color", "#000000", dark_color_button);
        
        auto check = Gtk::make_managed<Gtk::CheckButton>("DISPLAY_SPEED");
        check->set_active(true);
        bool_buttons["DISPLAY_SPEED"] = check;
        speed_options_box->add(*check);
        add_tooltip(check, "DISPLAY_SPEED");
        connect_speedometer_toggle(check, "DISPLAY_SPEED");
        
        check = Gtk::make_managed<Gtk::CheckButton>("NUMBERS_INSIDE");
        check->set_active(true);
        bool_buttons["NUMBERS_INSIDE"] = check;
        speed_options_box->add(*check);
        add_tooltip(check, "NUMBERS_INSIDE");
        connect_speedometer_toggle(check, "NUMBERS_INSIDE");
        
        check = Gtk::make_managed<Gtk::CheckButton>("NUMBER_TICKS");
        check->set_active(true);
        bool_buttons["NUMBER_TICKS"] = check;
        speed_options_box->add(*check);
        add_tooltip(check, "NUMBER_TICKS");
        connect_speedometer_toggle(check, "NUMBER_TICKS");
    }

    if (bool_buttons.count("DISPLAY_SPEED")) {
        displaySpeed = bool_buttons["DISPLAY_SPEED"]->get_active();
        testSpeedometer->set_display_speed(displaySpeed);
    }
    if (bool_buttons.count("NUMBERS_INSIDE")) {
        numbersInside = bool_buttons["NUMBERS_INSIDE"]->get_active();
        testSpeedometer->set_numbers_inside(numbersInside);
    }
    if (bool_buttons.count("NUMBER_TICKS")) {
        numberTicks = bool_buttons["NUMBER_TICKS"]->get_active();
        testSpeedometer->set_numbers_on_ticks(numberTicks);
    }

    auto save_values = [](const std::string& key, bool active){
        if (key.rfind("SHOW_TALON_", 0) == 0) {
            save_value(talon_values, key.substr(11), active);
        }
        else if (key.rfind("SHOW_FALCON_", 0) == 0) {
            save_value(falcon_values, key.substr(12), active);
        }
        else if (key.rfind("SHOW_LINEAR_", 0) == 0) {
            save_value(linear_values, key.substr(12), active);
        }
        else if (key.rfind("SHOW_AUTONOMY_", 0) == 0) {
            save_value(autonomy_values, key.substr(14), active);
        }
        else if (key.rfind("SHOW_ZED_", 0) == 0) {
            save_value(zed_values, key.substr(9), active);
        }
        else if (key.rfind("SHOW_COMMUNICATION_", 0) == 0) {
            save_value(communication_values, key.substr(19), active);
        }
        else if (key.rfind("SHOW_POWER_", 0) == 0) {
            save_value(power_values, key.substr(11), active);
        }
        else if (key.rfind("SHOW_POWER2_", 0) == 0) {
            save_value(power2_values, key.substr(12), active);
        }
    };

    auto write_values = [](std::ofstream& outfile, const std::string& label, const std::string& prefix){
        std::map<std::string, bool>& values = getMap(label);
        std::vector<std::string> keys = getKeys(label);
        for (const std::string& key : keys) {
            auto it = values.find(key);
            if (it != values.end()) {
                bool active = it->second;
                outfile << "SHOW_" << prefix << "_" << key << "=" << (active ? "true" : "false") << "\n";
            }
        }
        
    };

    reset_button->signal_clicked().connect([=]() mutable{
        local_talon_keys = reset_talon_keys;
        reset_frame("TALON");

        local_falcon_keys = reset_falcon_keys;
        reset_frame("FALCON");

        local_linear_keys = reset_linear_keys;
        reset_frame("LINEAR");

        local_autonomy_keys = reset_autonomy_keys;
        reset_frame("AUTONOMY");

        local_power_keys = reset_power_keys;
        reset_frame("POWER");

        local_power2_keys = reset_power2_keys;
        reset_frame("POWER2");

        local_zed_keys = reset_zed_keys;
        reset_frame("ZED");

        local_communication_keys = reset_communication_keys;
        reset_frame("COMMUNICATION");
    });

    save_button->signal_clicked().connect([=]() mutable{
        std::ofstream outfile("../resources/" + file_entry->get_text());
        
        for (const auto& [key, button] : bool_buttons) {
            bool active = button->get_active();
            save_values(key, active);
        }

        talon_keys = local_talon_keys;
        falcon_keys = local_falcon_keys;
        linear_keys = local_linear_keys;
        autonomy_keys = local_autonomy_keys;
        power_keys = local_power_keys;
        power2_keys = local_power2_keys;
        zed_keys = local_zed_keys;
        communication_keys = local_communication_keys;

        write_values(outfile, "Talon", "TALON");
        write_values(outfile, "Falcon", "FALCON");
        write_values(outfile, "Linear", "LINEAR");
        write_values(outfile, "Autonomy", "AUTONOMY");
        write_values(outfile, "Communication", "COMMUNICATION");
        write_values(outfile, "Power2", "POWER2");
        write_values(outfile, "Power", "POWER");
        write_values(outfile, "Zed", "ZED");

        const auto lightColorStr = to_color_string(light_color_button->get_rgba());
        const auto darkColorStr = to_color_string(dark_color_button->get_rgba());
        outfile << "LIGHT_BACKGROUND=" << lightColorStr << "\n";
        outfile << "DARK_BACKGROUND=" << darkColorStr << "\n";
        outfile << "DISPLAY_SPEED=" << (displaySpeed ? "true" : "false") << "\n";
        outfile << "NUMBERS_INSIDE=" << (numbersInside ? "true" : "false") << "\n";
        outfile << "NUMBER_TICKS=" << (numberTicks ? "true" : "false") << "\n";

        if (!lightBackground.empty() && !darkBackground.empty()) {
            std::cout << lightBackground << "\n" << darkBackground << std::endl;

            Gdk::RGBA selectedColor = isLightMode ? light_color_button->get_rgba() : dark_color_button->get_rgba();
            lightBackgroundColor = lightColorStr;
            darkBackgroundColor = darkColorStr;

            std::vector<CircleDrawingArea*> circles = {
                talon1Circle, talon3Circle,
                falcon1Circle, falcon2Circle, falcon3Circle, falcon4Circle,
                lowerFalcon1Circle, lowerFalcon2Circle, lowerFalcon3Circle, lowerFalcon4Circle
            };
            for (auto* circle : circles) {
                if (circle) circle->set_background_color(selectedColor);
            }

            auto css_provider = Gtk::CssProvider::create();
            const auto& css = isLightMode ? generateLightModeString(lightBackgroundColor)
                                        : generateDarkModeString(darkBackgroundColor);
            css_provider->load_from_data(css);
            Gtk::StyleContext::add_provider_for_screen(
                Gdk::Screen::get_default(), css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }

        displaySpeed   = bool_buttons["DISPLAY_SPEED"]->get_active();
        numbersInside  = bool_buttons["NUMBERS_INSIDE"]->get_active();
        numberTicks    = bool_buttons["NUMBER_TICKS"]->get_active();

        std::cout << "displaySpeed:" << displaySpeed << "\n"
                << "numbersInside:" << numbersInside << "\n"
                << "numberTicks:" << numberTicks << std::endl;

        for (auto* speedometer : {rightSpeedometer, leftSpeedometer}) {
            if (speedometer) {
                speedometer->set_display_speed(displaySpeed);
                speedometer->set_numbers_inside(numbersInside);
                speedometer->set_numbers_on_ticks(numberTicks);
                speedometer->queue_draw();
            }
        }
        updateGUI();
    });
    
    configWindow->signal_hide().connect([]() {
        std::cout << "Cleared bool_buttons" << std::endl;
        bool_buttons.clear();
        allowConfig = true;
    });

    main_box->pack_start(*grid);
    outer_box->add(*speed_box);
    outer_box->add(*sensorsBox);
    main_box->add(*outer_box);
    auto save_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    save_box->set_halign(Gtk::ALIGN_CENTER); 
    save_box->set_valign(Gtk::ALIGN_CENTER);

    save_button->set_size_request(250, 50);
    save_button->set_hexpand(false); 
    save_button->set_halign(Gtk::ALIGN_START); 

    reset_button->set_size_request(250, 50);
    reset_button->set_hexpand(false); 
    reset_button->set_halign(Gtk::ALIGN_START);

    save_box->pack_start(*save_button, Gtk::PACK_SHRINK);
    save_box->add(*reset_button);

    main_box->add(*save_box);
    scrolledWindow->add(*main_box);
    configWindow->add(*scrolledWindow);
    configWindow->show_all_children();
    configWindow->show_all();
}

void setupGUI(Glib::RefPtr<Gtk::Application> application) {
    initialize_maps();
    // Create window instance
    window = new Gtk::Window();
    window->maximize();

    try {
        auto icon = "../resources/razorbotz.png";
        window->set_icon_from_file(icon);
    } catch (const Glib::FileError& e) {
        g_print("Failed to load image: %s\n", e.what().c_str());
        return;
    }

    // Handles key press and release events  
    window->add_events(Gdk::KEY_PRESS_MASK);
    window->add_events(Gdk::KEY_RELEASE_MASK);
    window->signal_key_press_event().connect(sigc::ptr_fun(&on_key_press_event));
    window->signal_key_release_event().connect(sigc::ptr_fun(&on_key_release_event));

    // Create vertical box to hold top level widgets 
    Gtk::Box* topLevelBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    Gtk::Box* topControlsBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    Gtk::Box* videoTopLevelBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));

    // Create horizontal box to hold control widgets
    Gtk::Box* controlsBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));

    // Create scrolled window instance and list of addresses 
    Gtk::ScrolledWindow* scrolledList = Gtk::manage(new Gtk::ScrolledWindow());
    addressListBox = Gtk::manage(new Gtk::ListBox());
    addressListBox->signal_row_activated().connect(sigc::ptr_fun(&rowActivated));

    // Create vertical box on right of screen to house controls 
    Gtk::Box* controlsRightBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));

    // Create box to hold connection information (IP, connect button, etc.)
    Gtk::Box* parentConnectBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    Gtk::Box* connectBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));


    Gtk::Label* ipAddressLabel = Gtk::manage(new Gtk::Label(" IP Address "));

    // Create entry box for IP connection
    ipAddressEntry = Gtk::manage(new Gtk::Entry());
    ipAddressEntry->set_can_focus(true);
    ipAddressEntry->set_editable(true);
    if(useOrin)
        ipAddressEntry->set_text(ORIN_IP);
    else
        ipAddressEntry->set_text(NANO_IP);
    ipAddressEntry->set_name("dark_text");

    // Create connection button, single click logic to connectOrDisconnect function
    connectButton = Gtk::manage(new Gtk::Button("Connect"));
    connectButton->signal_clicked().connect(sigc::ptr_fun(&connectOrDisconnect));
    connectButton->set_name("dark_text");

    connectionStatusLabel = Gtk::manage(new Gtk::Label("Not Connected"));
    // Disconnect graphics for connect button
    Gdk::RGBA red;
    red.set_rgba(1.0, 0, 0, 1.0);
    connectionStatusLabel->override_background_color(red);
    connectionStatusLabel->set_name("dark_text");

    // Create horizontal box to hold silent run functionality
    Gtk::Box* stateBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 2));
    silentRunButton = Gtk::manage(new Gtk::Button("Silent Running"));
    silentRunButton->signal_clicked().connect(sigc::ptr_fun(&silentRun));
    silentRunButton->set_name("dark_text");

    // Create horizontal box to hold remote control functionality
    Gtk::Box* remoteControlBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 2));

    // Create button to shutdown robot
    Gtk::Button* shutdownRobotButton = Gtk::manage(new Gtk::Button("Shutdown Robot"));
    shutdownRobotButton->signal_clicked().connect(sigc::bind<Gtk::Window*>(sigc::ptr_fun(&shutdownDialog), window));
    shutdownRobotButton->set_name("dark_text");

    // Button to toggle from dark to light mode
    toggleModeButton = Gtk::manage(new Gtk::Button("Toggle Dark/Light Mode"));
    toggleModeButton->signal_clicked().connect(sigc::ptr_fun(&toggleMode));
    toggleModeButton->set_name("dark_text");
    toggleModeButton->set_size_request(100, 50);

    settingsButton = Gtk::make_managed<Gtk::Button>();
    auto image = Gtk::make_managed<Gtk::Image>("emblem-system", Gtk::ICON_SIZE_BUTTON);
    settingsButton->set_image(*image);
    settingsButton->set_tooltip_text("Open Settings");
    settingsButton->signal_clicked().connect([]() {
        if(allowConfig)
            create_config_editor_window(configFile);
    });
    settingsButton->set_size_request(50, 50);
    settingsButton->set_name("dark_text");

    // Apply CSS
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(generateLightModeString(lightBackgroundColor));
    auto screen = Gdk::Screen::get_default();
    auto style_context = Gtk::StyleContext::create();
    style_context->add_provider_for_screen(screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Load and apply the new font
    try {
        auto font_provider = Gtk::CssProvider::create();
        font_provider->load_from_data("* { font-family: 'Proxima Nova'; }");
        style_context->add_provider_for_screen(screen, font_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        // Load the font file
        std::string font_file = "../resources/ProximaNova.otf";
        if (!Glib::file_test(font_file, Glib::FILE_TEST_EXISTS)) {
            g_print("Font file not found: %s\n", font_file.c_str());
        } else {
            // If you need to load the font into Pango, you can do it here
            Pango::FontDescription font_desc;
            font_desc.set_family("Proxima Nova");
            font_desc.set_weight(Pango::WEIGHT_BOLD);
        }
    } catch (const Glib::Error& e) {
        g_print("Failed to load font: %s\n", e.what().c_str());
    }
    Gtk::Box* videoControlsBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));

    Gtk::ScrolledWindow* videoScrolledList=Gtk::manage(new Gtk::ScrolledWindow());
    videoAddressListBox=Gtk::manage(new Gtk::ListBox());
    videoAddressListBox->signal_row_activated().connect(sigc::ptr_fun(&videoRowActivated));

    Gtk::Box* videoControlsRightBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));

    Gtk::Box* videoConnectBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));
    Gtk::Label* videoIPAddress=Gtk::manage(new Gtk::Label(" IP Address "));
    videoIPAddressEntry=Gtk::manage(new Gtk::Entry());
    videoIPAddressEntry->set_can_focus(true);
    videoIPAddressEntry->set_editable(true);
    if(useOrin)
        videoIPAddressEntry->set_text(ORIN_IP);
    else
        videoIPAddressEntry->set_text(NANO_IP);
    videoIPAddressEntry->set_name("dark_text");

    videoConnectButton=Gtk::manage(new Gtk::Button("Connect"));
    videoConnectButton->signal_clicked().connect(sigc::ptr_fun(&videoConnectOrDisconnect));
    videoConnectButton->set_name("dark_text");
    videoConnectionStatusLabel=Gtk::manage(new Gtk::Label("Not Connected"));
    videoConnectionStatusLabel->override_background_color(red);
    videoConnectionStatusLabel->set_name("dark_text");
    
    Gtk::Box* videoStateBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2));
    videoStreamButton=Gtk::manage(new Gtk::Button("Not Video Streaming"));
    videoStreamButton->signal_clicked().connect(sigc::ptr_fun(&videoStream));
    videoStreamButton->set_name("dark_text");

    videoAddressListBox->set_size_request(200,30);
    videoScrolledList->set_size_request(200,75);

    videoConnectBox->add(*videoIPAddress);
    videoConnectBox->add(*videoIPAddressEntry);
    videoConnectBox->add(*videoConnectButton);
    videoConnectBox->add(*videoConnectionStatusLabel);

    videoStateBox->add(*videoStreamButton);
    videoControlsRightBox->add(*videoConnectBox);
    videoControlsRightBox->add(*videoStateBox);    
    videoScrolledList->add(*videoAddressListBox);

    Gtk::Label* spacer = Gtk::manage(new Gtk::Label());
    spacer->set_hexpand(true);

    videoControlsBox->add(*videoScrolledList);
    videoControlsBox->add(*videoControlsRightBox);
    videoControlsBox->add(*spacer);
    videoControlsBox->add(*toggleModeButton);
    videoControlsBox->add(*settingsButton);
    videoTopLevelBox->add(*videoControlsBox);

    // Set size for address list box
    addressListBox->set_size_request(200, 75);
    scrolledList->set_size_request(200, 75);

    // Add widgets to connect box
    connectBox->add(*ipAddressLabel);
    connectBox->add(*ipAddressEntry);
    connectBox->add(*connectButton);
    connectBox->add(*connectionStatusLabel);

    // Add widgets to silent run box
    stateBox->add(*silentRunButton);
    stateBox->add(*shutdownRobotButton);

    // Add widgets to controls box
    controlsRightBox->add(*connectBox);
    controlsRightBox->add(*stateBox);

    // Add address list to scrollable list
    scrolledList->add(*addressListBox);

    // Add widgets to controls box
    controlsBox->add(*scrolledList);
    controlsBox->add(*controlsRightBox);

    // Add widgets to top level box
    topControlsBox->add(*controlsBox);
    topControlsBox->add(*videoTopLevelBox);
    topLevelBox->add(*topControlsBox);

    if(!noVideo){
        Gtk::Box* bottomBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        Gtk::Box* bottomInnerBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
        innerLeftBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        Gtk::Box* innerMiddleBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        innerRightBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        bottomLowerBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
        Gtk::Box* lowerLeftBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        Gtk::Box* lowerRightBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        
        innerLeftBox = create_motor_column({
            {"Arm", &talon1Circle},
            {"Bucket", &talon3Circle}
            }, initArmPos,
            {"Talon 1", "Talon 3"},
            true 
        );

        auto cameraBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
        if(smallLaptop){
            cameraBox->set_size_request(800, 500);
        }
        else{
            cameraBox->set_size_request(1600, 1000);
        }
        videoArea = Gtk::manage(new VideoWidget());
        if(smallLaptop){
            videoArea->set_size_request(800, 500);
        }
        else{
            videoArea->set_size_request(1600, 1000);
        }
        //videoArea->set_hexpand(true);
        //videoArea->set_vexpand(true);
        cameraBox->add(*videoArea);
        innerMiddleBox->add(*cameraBox);

        innerRightBox = create_motor_column({
            {"Falcon 1", &falcon1Circle},
            {"Falcon 2", &falcon2Circle},
            {"Falcon 3", &falcon3Circle},
            {"Falcon 4", &falcon4Circle}
            }, initBucketPos,
            {"Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4"},
            false
        );

        bottomInnerBox->add(*innerLeftBox);
        bottomInnerBox->add(*innerMiddleBox);
        bottomInnerBox->add(*innerRightBox);
        bottomInnerBox->set_halign(Gtk::ALIGN_CENTER);

        bottomBox->add(*bottomInnerBox);
        initRoll();

        lowerLeftBox  = create_lower_motor_column({
            {"Falcon 1", &lowerFalcon1Circle},
            {"Falcon 2", &lowerFalcon2Circle}
            },
            {"Falcon 1", "Falcon 2"},
            true
        );

        bottomLowerBox->add(*lowerLeftBox);

        // To change Speedometer sizes, need to change this value
        leftSpeedometer = Gtk::manage(new Speedometer("Left Speedometer"));
        leftSpeedometer->set_size_request(300, 175);
        leftSpeedometer->set_display_speed(displaySpeed);
        leftSpeedometer->set_numbers_inside(numbersInside);
        leftSpeedometer->set_numbers_on_ticks(numberTicks);        
        bottomLowerBox->add(*leftSpeedometer);

        gear_dial = create_gear_dial(gears, gear_labels, gear_label_box);
        bottomLowerBox->add(*gear_dial);
        highlight_gear_and_scroll("3", gears, gear_labels, gear_dial, gear_label_box);

        rightSpeedometer = Gtk::manage(new Speedometer("Right Speedometer"));
        rightSpeedometer->set_size_request(300, 175);
        rightSpeedometer->set_display_speed(displaySpeed);
        rightSpeedometer->set_numbers_inside(numbersInside);
        rightSpeedometer->set_numbers_on_ticks(numberTicks); 
        bottomLowerBox->add(*rightSpeedometer);

        lowerRightBox  = create_lower_motor_column({
            {"Falcon 3", &lowerFalcon3Circle},
            {"Falcon 4", &lowerFalcon4Circle}
            },
            {"Falcon 3", "Falcon 4"});

        bottomLowerBox->add(*lowerRightBox);

        initPitch();
        bottomLowerBox->set_halign(Gtk::ALIGN_CENTER);
        bottomBox->add(*bottomLowerBox);
        topLevelBox->add(*bottomBox);

        Gdk::RGBA background;
        background.set(lightBackgroundColor);
        setBackgroundColors(background);
    }
    else{
        sensorBox = Gtk::manage(new Gtk::FlowBox());
        sensorBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        topLevelBox->add(*sensorBox);
    }


    window->add(*topLevelBox);
    window->signal_delete_event().connect(sigc::ptr_fun(quit));
    window->show_all();
}


void initSensorsWindow() {
    sensorsWindow = new Gtk::Window();
    if(monitor_count == 3){
        auto display = Gdk::Display::get_default();
        auto third_monitor = display->get_monitor(2);
        Gdk::Rectangle third_monitor_geometry;
        third_monitor->get_geometry(third_monitor_geometry);
        sensorsWindow->set_default_size(third_monitor_geometry.get_width(), third_monitor_geometry.get_height());
        sensorsWindow->move(third_monitor_geometry.get_x(), third_monitor_geometry.get_y());
    }
    else{
        sensorsWindow->maximize();
    }

    Gtk::Box* mainBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    mainBox->property_margin().set_value(10);

    // Create motor name vectors
    std::vector<std::string> talonNames = {"Talon 1", "Talon 3" };
    std::vector<std::string> falconNames = {"Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4"};
    std::vector<std::string> linearNames = {"Linear 1", "Linear 2"};

    // Create tabbed interface
    Gtk::Notebook* tabs = Gtk::manage(new Gtk::Notebook());
    tabs->set_vexpand(true);

    // Tab 1: Talon Motors
    Gtk::Box* talonTab = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    talonTab->property_margin().set_value(5);

    talonVoltageGraph = Gtk::manage(new MultiMotorGraph(
        "Talon Bus Voltage", MultiMotorGraph::VOLTAGE, talonNames));
    talonCurrentGraph = Gtk::manage(new MultiMotorGraph(
        "Talon Output Current", MultiMotorGraph::CURRENT, talonNames));
    talonPositionGraph = Gtk::manage(new MultiMotorGraph(
        "Talon Sensor Position", MultiMotorGraph::POSITION, talonNames));
    talonOutputGraph = Gtk::manage(new MultiMotorGraph(
        "Talon Output Percentage", MultiMotorGraph::OUTPUT_PERCENT, talonNames));

    talonTab->add(*talonVoltageGraph);
    talonTab->add(*talonCurrentGraph);
    talonTab->add(*talonPositionGraph);
    talonTab->add(*talonOutputGraph);
    tabs->append_page(*talonTab, "Talon Motors");

    // Tab 2: Falcon Motors
    Gtk::Box* falconTab = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    falconTab->property_margin().set_value(5);

    falconVoltageGraph = Gtk::manage(new MultiMotorGraph(
        "Falcon Bus Voltage", MultiMotorGraph::VOLTAGE, falconNames));
    falconCurrentGraph = Gtk::manage(new MultiMotorGraph(
        "Falcon Output Current", MultiMotorGraph::CURRENT, falconNames));
    falconPositionGraph = Gtk::manage(new MultiMotorGraph(
        "Falcon Sensor Position", MultiMotorGraph::POSITION, falconNames));
    falconOutputGraph = Gtk::manage(new MultiMotorGraph(
        "Falcon Output Percentage", MultiMotorGraph::OUTPUT_PERCENT, falconNames));

    falconTab->add(*falconVoltageGraph);
    falconTab->add(*falconCurrentGraph);
    falconTab->add(*falconPositionGraph);
    falconTab->add(*falconOutputGraph);
    tabs->append_page(*falconTab, "Falcon Motors");

    // Tab 3: Linear Actuators
    Gtk::Box* linearTab = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    linearTab->property_margin().set_value(5);

    linearSpeedGraph = Gtk::manage(new MultiMotorGraph(
        "Linear Actuator Speed", MultiMotorGraph::SPEED, linearNames));
    linearPotentiometerGraph = Gtk::manage(new MultiMotorGraph(
        "Linear Actuator Position", MultiMotorGraph::POTENTIOMETER, linearNames));

    linearTab->add(*linearSpeedGraph);
    linearTab->add(*linearPotentiometerGraph);
    tabs->append_page(*linearTab, "Linear Actuators");

    // Tab 4: Sensors Box
    Gtk::Box* sensorsTab = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    sensorsTab->property_margin().set_value(5);
    sensorBox = Gtk::manage(new Gtk::FlowBox());
    sensorBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    sensorsTab->add(*sensorBox);

    tabs->append_page(*sensorsTab, "Sensors");

    // Tab 5: Diagnostics Window
    // TODO: Figure out what information should be displayed here and add it
    Gtk::Box* diagnosticsTab = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    diagnosticsTab->property_margin().set_value(5);

    tabs->append_page(*diagnosticsTab, "Diagnostics");

    // Add everything to main window
    mainBox->add(*tabs);
    sensorsWindow->add(*mainBox);
    sensorsWindow->show_all();
}

struct RemoteRobot{
    std::string tag;
    time_t lastSeenTime;
};
std::vector<RemoteRobot> robotList;
std::mutex robotListMutex;

std::vector<RemoteRobot> videoRobotList;
std::mutex videoRobotListMutex;


bool contains(std::vector<std::string>& list, std::string& value){
    for(std::string storedValue: list) if(storedValue==value) return true;
    return false;
}


bool contains(std::vector<RemoteRobot>& list, std::string& robotTag){
    for(RemoteRobot storedValue: list) if(storedValue.tag==robotTag) return true;
    return false;
}


void update(std::vector<RemoteRobot>& list, std::string& robotTag){
    for(int index=0;index < list.size() ; ++index){
    time_t now;
    time(&now);
        list.at(index).lastSeenTime=now;
    }
}


std::vector<std::string> getAddressList(){
    std::vector<std::string> addressList;
    ifaddrs* interfaceAddresses = nullptr;
    for(int failed=getifaddrs(&interfaceAddresses); !failed && interfaceAddresses; interfaceAddresses=interfaceAddresses->ifa_next){
        if(interfaceAddresses->ifa_addr != NULL && interfaceAddresses->ifa_addr->sa_family == AF_INET){
            std::cout << "address" << std::endl;
            sockaddr_in* socketAddress=reinterpret_cast<sockaddr_in*>(interfaceAddresses->ifa_addr);
            std::string addressString(inet_ntoa(socketAddress->sin_addr));
            if(addressString=="0.0.0.0") continue;
            if(addressString=="127.0.0.1") continue;
            if(contains(addressList,addressString)) continue;
            addressList.push_back(addressString);
        }
    }
    return addressList;
}


void broadcastListen(){
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("Opening datagram socket error");
        return; 
    }
//
    int reuse = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR error");
        close(sd);
        return;
    }
//
    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    struct sockaddr_in localSock;
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(4321);
    localSock.sin_addr.s_addr = INADDR_ANY;
    if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
        perror("Binding datagram socket error");
        close(sd);
        return;
    }
//
    /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */
//
    std::vector<std::string> addressList=getAddressList(); 
    for(std::string addressString:addressList){
        std::cout << "got " << addressString << std::endl;
        struct ip_mreq group;
        group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
        group.imr_interface.s_addr = inet_addr(addressString.c_str());
        if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
            perror("Adding multicast group error");
        } 
    }
//
    char databuf[2048];
    int datalen = sizeof(databuf);
    while(true){
        ssize_t bytesRead = read(sd, databuf, datalen);
        if (bytesRead > 0) {
            std::string message(databuf, bytesRead); 
            std::lock_guard<std::mutex> lock(robotListMutex);
            bool robotExists = false;

            for (auto& robot : robotList) {
                if (robot.tag == message) {
                    time(&robot.lastSeenTime);
                    robotExists = true;
                    break; 
                }
            }

            if (!robotExists) {
                RemoteRobot newRobot;
                newRobot.tag = message;
                time(&newRobot.lastSeenTime);
                robotList.push_back(newRobot);
            }
        }
    }
}


void videoBroadcastListen(){
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("Opening datagram socket error");
        return; 
    }

    int reuse = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR error");
        close(sd);
        return;
    }

    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    struct sockaddr_in localSock;
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(4322);
    localSock.sin_addr.s_addr = INADDR_ANY;
    if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
        perror("Binding datagram socket error");
        close(sd);
        return;
    }

    /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */

    std::vector<std::string> addressList=getAddressList(); 
    for(std::string addressString:addressList){
        std::cout << "got " << addressString << std::endl;
        struct ip_mreq group;
        group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
        group.imr_interface.s_addr = inet_addr(addressString.c_str());
        if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
            perror("Adding multicast group error");
        } 
    }

    char databuf[1024];
    int datalen = sizeof(databuf);
    while(true){
        try{
            ssize_t bytesRead = read(sd, databuf, datalen);
            if (bytesRead > 0) {
                std::string message(databuf, bytesRead);
                std::lock_guard<std::mutex> lock(videoRobotListMutex);

                bool robotExists = false;
                for (auto& robot : videoRobotList) {
                    if (robot.tag == message) {
                        time(&robot.lastSeenTime);
                        robotExists = true;
                        break;
                    }
                }

                if (!robotExists) {
                    RemoteRobot newRobot;
                    newRobot.tag = message;
                    time(&newRobot.lastSeenTime);
                    videoRobotList.push_back(newRobot);
    }
            }
        }
        catch(std::exception e){
            std::cout << "Caught exception: " << e.what() << " in videoBroadcastLisetn" << std::endl;
        }
    }
}


void adjustRobotList(){
    std::lock_guard<std::mutex> lock(robotListMutex);
    if (!addressListBox) {
        std::cerr << "[ERROR] addressListBox is null in adjustRobotList()" << std::endl;
        return;
    }

    for(int index=0;index < robotList.size() ; ++index){
        time_t now;
        time(&now);
        if(now-robotList[index].lastSeenTime>12){
            robotList.erase(robotList.begin()+index--);
        }
    }
    //add new elements
    for(RemoteRobot remoteRobot:robotList){
        std::string robotID=remoteRobot.tag;
        bool match=false;
        int index=0;
        for(Gtk::ListBoxRow* listBoxRow=addressListBox->get_row_at_index(index); listBoxRow ; listBoxRow=addressListBox->get_row_at_index(++index)){
            Gtk::Label* label=static_cast<Gtk::Label*>(listBoxRow->get_child());
            Glib::ustring addressString=label->get_text();
            if(robotID==addressString.c_str()){
                match=true;
                break;
            }
        }
        if(match==false){
            Gtk::Label* label=Gtk::manage(new Gtk::Label(robotID));
            label->set_visible(true);
            addressListBox->append(*label);
        }
    }

    //remove old element
    std::vector<Gtk::ListBoxRow*> rows_to_remove;
    int index=0;
    for(Gtk::ListBoxRow* listBoxRow=addressListBox->get_row_at_index(index); listBoxRow ; listBoxRow=addressListBox->get_row_at_index(++index)){
        Gtk::Label* label=static_cast<Gtk::Label*>(listBoxRow->get_child());
        Glib::ustring addressString=label->get_text();
        bool match=false;
        for(RemoteRobot remoteRobot:robotList){
            std::string robotID=remoteRobot.tag;
            if(robotID==addressString.c_str()){
                match=true;
                break;
            }
        }
        if (!match) {
            rows_to_remove.push_back(listBoxRow);
        }
    }
    for (auto* row : rows_to_remove) {
        addressListBox->remove(*row);
    }
}

void adjustVideoRobotList(){
    std::lock_guard<std::mutex> lock(videoRobotListMutex);
    if (!videoAddressListBox) {
        std::cerr << "[ERROR] videoAddressListBox is null in adjustVideoRobotList()" << std::endl;
        return;
    }

    for(int index=0;index < videoRobotList.size() ; ++index){
        time_t now;
        time(&now);
        if(now-videoRobotList[index].lastSeenTime>12){
            videoRobotList.erase(videoRobotList.begin()+index--);
        }
    }
    //add new elements
    for(RemoteRobot remoteRobot:videoRobotList){
        std::string robotID=remoteRobot.tag;
        bool match=false;
        int index=0;
        for(Gtk::ListBoxRow* listBoxRow=videoAddressListBox->get_row_at_index(index); listBoxRow ; listBoxRow=videoAddressListBox->get_row_at_index(++index)){
            Gtk::Label* label=static_cast<Gtk::Label*>(listBoxRow->get_child());
            Glib::ustring addressString=label->get_text();
            if(robotID==addressString.c_str()){
                match=true;
                break;
            }
        }
        if(match==false){
            Gtk::Label* label=Gtk::manage(new Gtk::Label(robotID));
            label->set_visible(true);
            videoAddressListBox->append(*label);
        }
    }

    //remove old element
    std::vector<Gtk::ListBoxRow*> rows_to_remove;
    int index=0;
    for(Gtk::ListBoxRow* listBoxRow=videoAddressListBox->get_row_at_index(index); listBoxRow ; listBoxRow=videoAddressListBox->get_row_at_index(++index)){
        Gtk::Label* label=static_cast<Gtk::Label*>(listBoxRow->get_child());
        Glib::ustring addressString=label->get_text();
        bool match=false;
        for(RemoteRobot remoteRobot:videoRobotList){
            std::string robotID=remoteRobot.tag;
            if(robotID==addressString.c_str()){
                match=true;
                break;
            }
        }
        if(!match){
            rows_to_remove.push_back(listBoxRow);
        }
    }
    for (auto* row : rows_to_remove) {
        addressListBox->remove(*row);
    }
}

int key = 0x2C;
int checksum_decode(std::list<uint8_t>& byteList){
    //Checks last byte of data for the checksum
    if (byteList.size() < 1) {
        std::cout << "Not enough data to decode checksum." << std::endl;
        return -1;
    }

    // Extracts checksum (last byte)
    auto it = byteList.end();
    std::advance(it, -1);
    uint8_t storedChecksum = *it;

    // Sums byteList, excludes last byte (checksum) 
    uint32_t sum = 0;
    auto dataEnd = byteList.end();
    std::advance(dataEnd, -2);
    //std::cout << "Data: ";
    for (auto dataIt = byteList.begin(); dataIt != dataEnd; ++dataIt) {
        sum += *dataIt;
        //std::cout<<std::hex<<static_cast<int>(*dataIt)<<" ";
        
    }
    //std::cout<<std::endl;

    // Recalculate the checksum as sum modulo key.
    uint8_t computedChecksum = sum % key;

    //std::cout << "Computed checksum from data: 0x" << std::hex << static_cast<int>(computedChecksum) << std::endl;
    //std::cout << "Stored checksum: 0x" << std::hex << static_cast<int>(storedChecksum) << std::endl;

    if (computedChecksum == storedChecksum) {
        //std::cout << "Checksum is valid." << std::endl;
        return 1;
    } else {
        //std::cout << "Checksum is invalid." << std::endl;
        byteList.clear();
        return 0;

    }

}
void print_data(std::list<uint8_t>& byteList){

	    auto dataEnd = byteList.end();
	    std::advance(dataEnd, -2);
	for (auto dataIt = byteList.begin(); dataIt != dataEnd; ++dataIt){
		std::cout<<std::hex<<static_cast<int>(*dataIt)<<" ";
	}

}


void initArenaWindow(){
    arenaWindow = new Gtk::Window();
    arenaWindow->set_title("Arena Map/Cams");

    if(monitor_count == 3){
        auto display = Gdk::Display::get_default();
        auto second_monitor = display->get_monitor(1);
        Gdk::Rectangle second_monitor_geometry;
        second_monitor->get_geometry(second_monitor_geometry);
        arenaWindow->move(second_monitor_geometry.get_x(), second_monitor_geometry.get_y());
        arenaWindow->set_default_size(second_monitor_geometry.get_width(), second_monitor_geometry.get_height());
    }
    else{
        arenaWindow->maximize();
    }

    try {
        auto icon = "../resources/razorbotz.png";
        arenaWindow->set_icon_from_file(icon);
    } catch (const Glib::FileError& e) {
        g_print("Failed to load image: %s\n", e.what().c_str());
        return;
    }

    // Arena cams
    // Add mainBox to window
    Gtk::Box* mainBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,10));
    arenaWindow->add(*mainBox);
    
    // Arena map left
    overlay_area = Gtk::manage(new ImageOverlay());
    mainBox->pack_start(*overlay_area, Gtk::PACK_EXPAND_WIDGET);
    
    // Cameras box
    Gtk::Box* camsBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    mainBox->pack_start(*camsBox, Gtk::PACK_SHRINK);
    
    // Awareness Cam
    Gtk::Overlay* awareness_overlay = Gtk::manage(new Gtk::Overlay());
    Gtk::Box* livestreamBox1 = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,0));
    livestreamBox1->set_size_request(800, 600);
    camsBox->pack_start(*awareness_overlay, Gtk::PACK_SHRINK);
    
    // Webview 1 (Awareness)
    auto webview1 = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webkit_web_view_load_uri(webview1, "http://192.168.1.8/mjpeg/1");
    Gtk::Widget* webview_widget1 = Glib::wrap(GTK_WIDGET(webview1));
    livestreamBox1->pack_start(*webview_widget1, Gtk::PACK_EXPAND_WIDGET);
    awareness_overlay->add(*livestreamBox1);

    // Awareness cam label
    Gtk::Label* awareness_label = Gtk::manage(new Gtk::Label("Awareness Camera:"));
    //awareness_label->override_color(Gdk::RGBA("black"));
    awareness_label->set_halign(Gtk::ALIGN_START);
    awareness_label->set_valign(Gtk::ALIGN_START);
    awareness_overlay->add_overlay(*awareness_label);
    
    // Back Cam
    Gtk::Overlay* back_overlay = Gtk::manage(new Gtk::Overlay());
    Gtk::Box* livestreamBox2 = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,0));
    livestreamBox2->set_size_request(800, 600);
    camsBox->pack_start(*back_overlay, Gtk::PACK_SHRINK);
    
    // Webview 2 (Back)
    auto webview2 = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webkit_web_view_load_uri(webview2, "http://192.168.1.9/mjpeg/1");
    Gtk::Widget* webview_widget2 = Glib::wrap(GTK_WIDGET(webview2));
    livestreamBox2->pack_start(*webview_widget2, Gtk::PACK_EXPAND_WIDGET);
    back_overlay->add(*livestreamBox2);

    // Awareness cam label
    Gtk::Label* back_label = Gtk::manage(new Gtk::Label("Back Camera:"));
    //back_label->override_color(Gdk::RGBA("black"));
    back_label->set_halign(Gtk::ALIGN_START);
    back_label->set_valign(Gtk::ALIGN_START);
    back_overlay->add_overlay(*back_label);

    // Style the overlay label
    auto css_provider = Gtk::CssProvider::create();
    std::string format = "* { font-family: 'Proxima Nova'; }\n"
        ".overlay-text {\n"
            "font-size: 30px;\n"
            "background-color: " + lightBackgroundColor + ";\n"
            "padding: 5px;\n"
            "margin: 10px;\n"
            "border-radius: 3px;\n"
        "}";
    css_provider->load_from_data(format);
    awareness_label->get_style_context()->add_provider(
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    awareness_label->get_style_context()->add_class("overlay-text");
    back_label->get_style_context()->add_provider(
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    back_label->get_style_context()->add_class("overlay-text");

    arenaWindow->show_all();
}


/* Main function to receive the video stream and display it*/
void videoMain(){
    std::thread broadcastListenThread2(videoBroadcastListen);

    cv::Mat img = cv::Mat::zeros(720, 1280, CV_8UC1);
    int imgSize = img.total() * img.elemSize();
    uchar sockData[imgSize];
    int bytesRead=0, total = 0;

    bool running=true;
    while(running){    
        if(!videoConnected || !isStreamingActive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
    
    
        uint32_t network_frame_size = 0;
        ssize_t bytesRead = 0;
        size_t totalHeaderRead = 0;
    
        while (totalHeaderRead < sizeof(network_frame_size)) {
            bytesRead = recv(videoSock, reinterpret_cast<char*>(&network_frame_size) + totalHeaderRead, sizeof(network_frame_size) - totalHeaderRead, 0);
            if (bytesRead > 0) {
                totalHeaderRead += bytesRead;
            }
            else if (bytesRead == 0) {
                shouldVideoDisconnect = true;
                videoDisconnectDispatcher.emit();
                isStreamingActive = false;
                break;
            }
            else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                else {
                    perror("recv header error");
                    shouldVideoDisconnect = true;
                    videoDisconnectDispatcher.emit();
                    isStreamingActive = false;
                    break;
                }
            }
        }
    
        if (!videoConnected || !isStreamingActive) {
            continue;
        }
    
    
        uint32_t frameSize = ntohl(network_frame_size);
    
        if (frameSize == 0) {
            std::cerr << "Invalid frame size received: " << frameSize << std::endl;
            shouldVideoDisconnect = true;
            videoDisconnectDispatcher.emit();
            isStreamingActive = false;
            continue;
        }
    
    
        std::vector<uchar> frameDataBuffer(frameSize);
        size_t totalFrameRead = 0;
        while (totalFrameRead < frameSize) {
            bytesRead = recv(videoSock, frameDataBuffer.data() + totalFrameRead, frameSize - totalFrameRead, 0);
             if (bytesRead > 0) {
                totalFrameRead += bytesRead;
            }
            else if (bytesRead == 0) {
                shouldVideoDisconnect = true;
                videoDisconnectDispatcher.emit();
                isStreamingActive = false;
                break;
            }
            else {
                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(1));
                     continue;
                 }
                 else {
                    perror("recv frame error");
                    shouldVideoDisconnect = true;
                    videoDisconnectDispatcher.emit();
                    isStreamingActive = false;
                    break;
                }
            }
        }
    
    
        if (!videoConnected || !isStreamingActive) {
            continue;
        }
    
    
        cv::Mat decoded_frame;
        try {
            decoded_frame = cv::imdecode(frameDataBuffer, isGray ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
        }
        catch (const cv::Exception& e) {
            std::cerr << "OpenCV exception during imdecode: " << e.what() << ". Buffer size: " << frameDataBuffer.size() << std::endl;
            continue;
        }

        if (decoded_frame.empty()) {
            std::cerr << "Failed to decode JPEG image. Buffer size: " << frameDataBuffer.size() << std::endl;
            continue; 
        }

        cv::Mat display_img;
        try {
            cv::resize(decoded_frame, display_img, cv::Size(1600, 1000), 0, 0, cv::INTER_LINEAR);
        }
        catch (const cv::Exception& e) {
            std::cerr << "OpenCV exception during resize: " << e.what() << std::endl;
            continue;
        }
        
        if (display_img.empty()) {
            std::cerr << "Image is empty after resize." << std::endl;
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            latestFrame = display_img.clone();
            newFrameAvailable = true;
        }
    }
    return; 

}


/* Function to check whether the old laptop is running the control program.
Because the old laptop has a smaller screen, the size of the window should be smaller.*/
void checkSize(){
    auto display = Gdk::Display::get_default();
    auto primary_monitor = display->get_monitor(0);
    if (primary_monitor) {
        Gdk::Rectangle geometry;
        primary_monitor->get_geometry(geometry);
        int x = geometry.get_x();
        int y = geometry.get_y();
        int width = geometry.get_width();
        int height = geometry.get_height();
        std::cout << "Height: " << height << std::endl << "Width: " << width << std::endl;
        if(width < 1920){
            smallLaptop = true;
        }
    }
}


/*
Function to set the various config values. Given a variable name and a 
value, this function gets the correct variable and then sets the value.
*/
void setConfigValues(std::string variableName, std::string value){
    std::cout << "Variable: " << variableName << ", Value: " << value << std::endl;
    if("LIGHT_BACKGROUND" == variableName){
        lightBackgroundColor = value;
    }
    if("DARK_BACKGROUND" == variableName){
        darkBackgroundColor = value;
    }
    if("DISPLAY_SPEED" == variableName){
        if("false" == value){
            displaySpeed = false;
        }
        else{
            displaySpeed = true;
        }
    }
    if("NUMBERS_INSIDE" == variableName){
        if("false" == value){
            numbersInside = false;
        }
        else{
            numbersInside = true;
        }
    }
    if("NUMBER_TICKS" == variableName){
        if("false" == value){
            numberTicks = false;
        }
        else{
            numberTicks = true;
        }
    }
}


/*
Function to parse the config file given by the file name. If the file
doesn't exist, the file check fails and the program uses the default 
values for the config. It splits each line by the = to get the variable
name and value, then sets the values using the setConfigValues function.
*/
void parseConfigFile(std::string filename){
    std::ifstream file("../resources/" + filename);

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string variableName = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                setConfigValues(variableName, value);
            }
            else {
                std::cerr << "Invalid line (no '='): " << line << std::endl;
            }
        }
        file.close();
    }
    else {
        std::cout << "Unable to open file. Using default configuration" << std::endl;
    }
}


void processArguments(int argc, char** argv){
    if(argc > 1){
        for(int i = 1; i < argc; ++i){
            if(!strcmp("--help", argv[i])){
                std::cout << "Control Flag Options:" << std::endl;
                std::cout << "--init: Initialize GUI with values" << std::endl;
                std::cout << "--no_video: Remove large center space for video stream, Displays sensor values instead" << std::endl;
                std::cout << "--no_arena: Disables arena map window" << std::endl;
                std::cout << "--set_colors: Specifies values to use as background colors. Should have light color, then dark color in" 
                "format \"#FFFFFF\" \"#000000\""<< std::endl;
                std::cout << "NOTE: All strings must be enclosed in \" to have them work properly" << std::endl;
                std::cout << "--set_map: Sets the background map used in the arena" << std::endl;
                std::cout << "--wsl: Sets the video size to a smaller size" << std::endl;
                std::cout << "--config_file: Specifies the config file to be used to load the settings" << std::endl;
                std::cout << "--nano: Switches IP address used to connect to the Jetson Nano" << std::endl;
                std::cout << "--test_input: Allows for testing inputs without being connected to robot" << std::endl;
                std::cout << "--alt_layout: Uses alternate joystick control mapping for robot" << std::endl;
                exit(0);
            }
            else if(!strcmp("--init", argv[i])){
                initVals = true;
            }
            else if(!strcmp("--testing", argv[i])){
                
            }
            else if(!strcmp("--no_video", argv[i])){
                noVideo = true;
            }
            else if(!strcmp("--no_arena", argv[i])){
                noArena = true;
            }
            else if(!strcmp("--set_colors", argv[i])){
                if(i+1 < argc){
                    lightBackgroundColor = argv[i+1];
                    i++;
                }
                if(i+1 < argc){
                    darkBackgroundColor = argv[i+1];
                    i++;
                }
            }
            else if(!strcmp("--set_map", argv[i])){
                mapUsed = argv[i+1];
            }
            else if(!strcmp("--wsl", argv[i])){
                smallLaptop = true;
            }
            else if(!strcmp("--config_file", argv[i])){
                parseConfigFile(argv[i+1]);
                if(allowConfig)
                    create_config_editor_window(argv[i+1]);
                return;
            }
            else if(!strcmp("--nano", argv[i])){
                useOrin = false;
            }
            else if(!strcmp("--test_input", argv[i])){
                testInput = true;
            }
            else if(!strcmp("--alt_layout", argv[i])){
                useAltLayout = true;
            }
        }
    }
}


void moveWindows(){
    auto display = Gdk::Display::get_default();
    monitor_count = display->get_n_monitors();
    if(monitor_count == 1){
        auto primary_monitor = display->get_monitor(0);
        Gdk::Rectangle primary_monitor_geometry;
        primary_monitor->get_geometry(primary_monitor_geometry);
        window->move(primary_monitor_geometry.get_x(), primary_monitor_geometry.get_y());
        window->show();
        window->raise();
    }
    
}


void remapJoystickInputs(uint8_t* which, uint8_t* axis){
    // Expected values are as follows:
    // Joystick 0:
    // Axis 0 - Roll
    // Axis 1 - Pitch
    // Joystick 1:
    // Axis 0 - Bucket
    // Axis 1 - Arm
    if(isController){
        // If a controller is used, axes 0 and 1 should be mapped to joystick 0
        // Axes 2 and 3 should be mapped to joystick 1
        if(*axis == 2){
            *which = 1;
            *axis = 0;
        }
        if(*axis == 3){
            *which = 1;
            *axis = 1;
        }
    }
    if(useAltLayout){
        // Alt layout is as follows:
        // Joystick 0:
        // Axis 0 - Left Speed
        // Axis 1 - Arm
        // Joystick 1:
        // Axis 0 - Right Speed
        // Axis 1 - Bucket
        // Note: This probably isn't going to respond as expected. The speed calculations aren't meant
        // to have individual speed components like this
        if(*which == 0){
            if(*axis == 1){
                *which = 1;
                *axis = 1;
            }
        }
        if(*which == 1){
            if(*axis == 0){
                *which = 0;
                *axis = 0;
            }
        }
    }
    if(!twoJoysticks){
        // If a single joystick is used, control the bucket speed with twist of axis 2
        // Buttons 
        if(*axis == 2){
            *which = 1;
            *axis = 0;
        }
    }
}


//UDP Version
int main(int argc, char** argv) { 
    //Setup GUI
    Glib::RefPtr<Gtk::Application> application = Gtk::Application::create(argc, argv, "edu.uark.razorbotz");
    processArguments(argc, argv);
    checkSize();
    setupGUI(application);
    if(!noArena)
        initArenaWindow();
    if(!noVideo)
        initSensorsWindow();
    moveWindows();
    initGUI();

    videoDisconnectDispatcher.connect([]() {
        if (shouldVideoDisconnect) {
            setVideoDisconnectedState();
            shouldVideoDisconnect = false;
        }

    });
    
    //Start a thread to listen to updates from the robot
    std::thread broadcastListenThread(broadcastListen);
    broadcastListenThread.detach();
    std::thread broadcastVideoListenThread(videoMain);
    broadcastVideoListenThread.detach();

    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }
    SDL_JoystickEventState(SDL_ENABLE);

    //-------------------------------------------------------------------------Initializing joystick(s)--------------------------------------------------------------------------
    int joystickCount=SDL_NumJoysticks();
    std::cout << "number of joysticks " << joystickCount << std::endl;
    if(joystickCount == 2){
        twoJoysticks = true;
    }
    SDL_Joystick* joystickList[joystickCount];
    SDL_GameController* controller = nullptr;

    if(joystickCount>0){
        axisEventList = new std::vector<std::vector<AxisEvent*>*>(joystickCount);
        for(int joystickIndex=0;joystickIndex<joystickCount;joystickIndex++) {

            if(SDL_IsGameController(joystickIndex)){
                isController = true;
                controller = SDL_GameControllerOpen(joystickIndex);
                if(controller){
                    std::cout << "Opened controller: " << SDL_GameControllerName(controller) << std::endl;
                    joystickList[joystickIndex]=SDL_GameControllerGetJoystick(controller);
                }
            }
            else{
                joystickList[joystickIndex]=SDL_JoystickOpen(joystickIndex);
            }
            if (joystickList[joystickIndex]) {
                axisEventList->at(joystickIndex) = new std::vector<AxisEvent*>(SDL_JoystickNumAxes(joystickList[joystickIndex]));
                for(int axisIndex=0; axisIndex < SDL_JoystickNumAxes(joystickList[joystickIndex]); axisIndex++){
                    axisEventList->at(joystickIndex)->at(axisIndex) = new AxisEvent();
                }
                std::cout << "Opened Joystick " << joystickIndex << std::endl;
                std::cout << "   Name: " << SDL_JoystickName(joystickList[joystickIndex]) << std::endl;
                std::cout << "   Number of Axes: " << SDL_JoystickNumAxes(joystickList[joystickIndex]) << std::endl;
                std::cout << "   Number of Buttons: " << SDL_JoystickNumButtons(joystickList[joystickIndex]) << std::endl;
                std::cout << "   Number of Balls: " << SDL_JoystickNumBalls(joystickList[joystickIndex]) << std::endl;
            }
            else {
                (*axisEventList)[joystickIndex] = new std::vector<AxisEvent*>(0);
                std::cout << "Couldn't open Joystick " << joystickIndex << std::endl;
            }
        }
    }

    SDL_Event event;
    char buffer[16384] = {0}; 
    int bytesRead=0;

    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastTransmitTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastReceiveTime = std::chrono::high_resolution_clock::now();
    lastHeartbeatTime = std::chrono::high_resolution_clock::now();
    now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTransmitTime);
    double deltaTime = time_span.count();
    
    std::list<uint8_t> messageBytesList; //List to store incoming bytes
    uint8_t message[256];
    bool running=true;
    while(running){
        adjustRobotList();
        adjustVideoRobotList();

        while(Gtk::Main::events_pending()){
            Gtk::Main::iteration();
        }

        if (newFrameAvailable) {
            if (videoArea) {
                videoArea->setFrame(latestFrame);
            }
            newFrameAvailable = false;
        }

        if(!testInput){
            if(!initialized)
            continue;
        }

        //std::cout << "Before Read" << std::endl;

        //Avoid blocking if we hear nothing
        fcntl(sock,F_SETFL, O_NONBLOCK);
        //Receive messages from the robot, store in buffer of size 16384
        bytesRead = recvfrom(sock, buffer, 16384, 0, (struct sockaddr *)&serv_addr, &addr_len);

        if(bytesRead == 17){
            for(int index=0;index<bytesRead;index++){
                buffer[index] = 0;
            }
            setConnectedState();
            lastReceiveTime = std::chrono::high_resolution_clock::now();
            continue;
        }

        if(!testInput){
            if(!connected){
                if(messageBytesList.size() > 0){
                    messageBytesList.clear();
                }
                continue;
            }
            if(bytesRead==0){
                //std::cout << "Lost Connection" << std::endl;
                setDisconnectedState();
                if(messageBytesList.size() > 0){
                    messageBytesList.clear();
                }
                continue;
            }
        }

        //std::cout << "After Read" << std::endl;
        
        //Fill the messageBytesList with the bytes read from the socket
        if(bytesRead != -1){
        	//std::cout << bytesRead << std::endl;
            for(int index=0;index<bytesRead;index++){
                messageBytesList.push_back(buffer[index]);
            }
            lastReceiveTime = std::chrono::high_resolution_clock::now();
        }

        if(silentRunning){
            lastReceiveTime = std::chrono::high_resolution_clock::now();
        }
        else{
            now = std::chrono::high_resolution_clock::now();
            time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastReceiveTime);
            deltaTime = time_span.count();
            if(deltaTime > 5.0 && connected){
                setDisconnectedState();
            }
        }
        
        //std::cout << "Before hasMessage check" << std::endl;
        while(BinaryMessage::hasMessage(messageBytesList)){
	    //print_data(messageBytesList);
            /****************CHECKSUM: Branch to process each message in messageBytesList in the case that the checksum is to be verified****************/
            //std::cout << "Before message create" << std::endl;
            int checksum = checksum_decode(messageBytesList); 
            if (checksum == 0){
                break; 
            }
            else{
                BinaryMessage message(messageBytesList);
                //std::cout << "Before GUI update" << std::endl;
                updateGUI(message); //Update the GUI with the message
                //std::cout << "Before size decode" << std::endl;
                uint64_t size=BinaryMessage::decodeSizeBytes(messageBytesList); //Decode the size of the message
                for(int count=0; count < size + 1; count++){
                    //std::cout << messageBytesList.front();
                    messageBytesList.pop_front();
                }
            }

        }

        now = std::chrono::high_resolution_clock::now();
        time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastHeartbeatTime);
        deltaTime = time_span.count();
        if(deltaTime > 1.0 && connected){
            lastHeartbeatTime = std::chrono::high_resolution_clock::now();
            uint8_t command=0;
            int length=2;
            uint8_t message[length];
            message[0]=length;
            message[1]=command;

            // send(sock, message, length, 0);
            sendto(sock , message , length , 0 ,(struct sockaddr *)&serv_addr, addr_len);
        }

        /******************************Handle control events******************************/
        while(SDL_PollEvent(&event)){
            const Uint8 *state = SDL_GetKeyboardState(NULL);

            switch(event.type){

                case SDL_MOUSEMOTION:{
                    int mouseX = event.motion.x;
                    int mouseY = event.motion.y;

                    std::cout << "X: " << mouseX << " Y: " << mouseY << std::endl;

                    break;
                }

                case SDL_KEYDOWN:{
                    std::cout << event.key.keysym.sym << std::endl;
                    std::cout << "key down" << std::endl;
                    break;
                }

                case SDL_KEYUP:{
                    std::cout << "key up" << std::endl;
                    break;
                }

                case SDL_JOYHATMOTION:{

                    uint8_t command=6;
                    int length=5;
                    uint8_t message[length];
                    message[0]=length;
                    message[1]=command;
                    message[2]=event.jhat.which;
                    message[3]=event.jhat.hat;
                    message[4]=event.jhat.value;

                    // send(sock, message, length, 0);
                    sendto(sock , message , length , 0 ,(struct sockaddr *)&serv_addr, addr_len);
                    break;
                }
                case SDL_JOYBUTTONDOWN:{
                    std::cout << "Joystick button down" << std::endl;
                    if(!twoJoysticks){
                        if(event.jbutton.button == 2 && event.jbutton.state == 1){
                            axisEventList->at(1)->at(1)->value = 32768.0;
                        }
                        if(event.jbutton.button == 2 && event.jbutton.state == 1){
                            axisEventList->at(1)->at(1)->value = -32768.0;
                        }
                    }
                    uint8_t command=5;
                    int length=5;
                    uint8_t message[length];
                    message[0]=length;
                    message[1]=command;
                    message[2]=event.jbutton.which;
                    message[3]=event.jbutton.button;
                    message[4]=event.jbutton.state;

                    // send(sock, message, length, 0);
                    sendto(sock , message , length , 0 ,(struct sockaddr *)&serv_addr, addr_len);
                    break;
                }
                case SDL_JOYBUTTONUP:{
                    std::cout << "Joystick button up" << std::endl;
                    uint8_t command=5;
                    int length=5;
                    uint8_t message[length];
                    message[0]=length;
                    message[1]=command;
                    message[2]=event.jbutton.which;
                    message[3]=event.jbutton.button;
                    message[4]=event.jbutton.state;

                    // send(sock, message, length, 0);
                    sendto(sock , message , length , 0 ,(struct sockaddr *)&serv_addr, addr_len);
                    break;
                }
                case SDL_JOYAXISMOTION: {
                    //std::cout << "Joystick axis motion" << std::endl;
                    int deadZone=4000;
                    if(event.jaxis.value < -deadZone || deadZone < event.jaxis.value ) {
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->isSet = true;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->which = event.jaxis.which;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->axis  = event.jaxis.axis;

                        int value = event.jaxis.value;
                        if(value < -deadZone)   value-=deadZone;
                        if(deadZone < value) value+=deadZone;

                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->value = value;
                    }
                    else{
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->isSet = true;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->which = event.jaxis.which;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->axis  = event.jaxis.axis;

                        int value = 0;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->value = value;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        now = std::chrono::high_resolution_clock::now();
        time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTransmitTime);
        deltaTime = time_span.count();
        if(deltaTime > 0.05 ){
            lastTransmitTime = std::chrono::high_resolution_clock::now();
            for(int joystickIndex=0; joystickIndex < axisEventList->size(); joystickIndex++){
                for(int axisIndex=0; axisIndex < axisEventList->at(joystickIndex)->size(); axisIndex++){
                    if(axisEventList->at(joystickIndex)->at(axisIndex)->isSet){
                        //std::cout << joystickIndex << " " << axisIndex << " " << axisEventList->at(joystickIndex)->at(axisIndex)->value << std::endl;
                        axisEventList->at(joystickIndex)->at(axisIndex)->isSet = false;

                        uint8_t command = 1;
                        int length = 8;
                        float value = ((float)axisEventList->at(joystickIndex)->at(axisIndex)->value) / -32768.0;
                        uint8_t message[length];
                        uint8_t which = axisEventList->at(joystickIndex)->at(axisIndex)->which;
                        uint8_t axis = axisEventList->at(joystickIndex)->at(axisIndex)->axis;//0-roll 1-pitch 2-throttle 3-yaw
                        remapJoystickInputs(&which, &axis);
                        message[0] = length;
                        message[1] = command;
                        message[2] = which;
                        message[3] = axis;
                        insert(value, &message[4]);

                        // send(sock, message, length, 0);
                        sendto(sock , message , length , 0 ,(struct sockaddr *)&serv_addr, addr_len);
                
                    }
                }
            }
        }
    }
    return 0; 
}
