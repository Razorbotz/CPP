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
#include <zlib.h>
#include <foxglove/websocket/websocket_notls.hpp>
#include <foxglove/websocket/websocket_server.hpp>
#include <nlohmann/json.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "InfoFrame.hpp"
#include "BinaryMessage.hpp"
#include "Speedometer.hpp"
#include "ConfigDefinitions.hpp"
#include "ConfigEditorWindow.hpp"
#include "NetworkHandler.hpp"
#include "ProximityBar.hpp"
#include "PositionBar.hpp"
#include "ArtificialHorizon.hpp"
#include "BatteryBar.hpp"
#include "BotConfig.hpp"

/*
TODO: 
Map Issues:

*/

std::string ORIN_IP = "192.168.0.6";
std::string NANO_IP = "192.168.0.5";
bool useOrin = true;

#define LOW_VOLTAGE 12.0f

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

std::unique_ptr<foxglove::Server<foxglove::WebSocketNoTls>> foxglove_server;
bool quit(GdkEventAny* event){
    if (foxglove_server) {
        foxglove_server->stop();
    }
    
    exit(0);
}

Gtk::ListBox* addressListBox;
Gtk::Entry* ipAddressEntry;
Gtk::Label* connectionStatusLabel;
Gtk::Entry* ipAddressEntry2;
Gtk::Label* connectionStatusLabel2;
  
Gtk::Button* silentRunButton;
Gtk::Button* connectButton;
Gtk::Button* silentRunButton2;
Gtk::Button* connectButton2;
Gtk::Button* toggleModeButton;
Gtk::Button* settingsButton;

Gtk::Box* topControlsBox;

Gtk::ListBox* videoAddressListBox;
Gtk::Entry* videoIPAddressEntry;
Gtk::Label* videoConnectionStatusLabel;
  
Gtk::Button* videoStreamButton;
Gtk::Button* videoConnectButton;
bool isGray = true;
std::mutex frameMutex;
cv::Mat latestFrame;
std::atomic<bool> newFrameAvailable;
Glib::Dispatcher videoDisconnectDispatcher;
std::atomic<bool> shouldVideoDisconnect = false;
Glib::Dispatcher connection_finished_dispatcher;
Glib::Dispatcher connection_finished_dispatcher2;
Glib::Dispatcher video_connection_finished_dispatcher;
  
Gtk::FlowBox* sensorBox;
Gtk::Box* innerLeftBox;
Gtk::Box* innerRightBox;
Gtk::Box* bottomLowerBox;
Gtk::Box* armPositionPlaceholder;
Gtk::Box* bucketTiltPlaceholder;
Gtk::Box* rollImagePlaceholder;

Gtk::Window* window;

bool initVals = false;
bool threeMonitors = false;
bool smallLaptop = false;
bool wsl = false;
double GUI_SCALE = 1.0;
bool noVideo = false;
bool debugGladeBounds = false;
bool testInput = false;
bool useAltLayout = false;
bool isController = false;
bool twoJoysticks = false;
BotConfig activeConfig = configs::primaryBot();

// Backward-compatible flags — derived from activeConfig in processArguments().
// Use these in code that hasn't been migrated to read activeConfig directly yet.
// Once all if(primaryBot)/if(backupBot)/if(dumpBot) branches are converted to
// use activeConfig or feature flags, remove these.
bool primaryBot = true;
bool backupBot = false;
bool dumpBot = false;


bool simulateNetwork = false;
Gtk::Window* simulatorWindow = nullptr;
Gtk::ComboBoxText* simTypeCombo = nullptr;
Gtk::Box* simContentBox = nullptr;

std::map<std::string, Gtk::Widget*> activeSimWidgets;

ProximityBar* proximityBar = nullptr;

// --- Foxglove Globals ---
foxglove::ChannelId arena_mesh_channel;
foxglove::ChannelId tf_channel;
std::chrono::high_resolution_clock::time_point lastFoxgloveTransmit = std::chrono::high_resolution_clock::now();

// --- Encode Tool Globals ---
bool start_encode_tool = false;
Gtk::Window* encodeToolWindow = nullptr;
Gtk::ComboBoxText* encodeTypeCombo = nullptr;
Gtk::Box* encodeContentBox = nullptr;

Gtk::CheckButton* cbUseFieldStrings = nullptr;
Gtk::CheckButton* cbIncludeChecksum = nullptr;
Gtk::CheckButton* cbApplyEnvelope   = nullptr;

Gtk::CheckButton* cbShowDecoded = nullptr;
Gtk::ComboBoxText* decodeStageCombo = nullptr;   // Raw / After checksum / After envelope
Gtk::CheckButton* cbValidateChecksum = nullptr;

Gtk::TextView* txtHexStringLabels = nullptr;
Gtk::TextView* txtHexFieldLabels  = nullptr;
Gtk::TextView* txtDecodedStringLabels = nullptr;
Gtk::TextView* txtDecodedFieldLabels  = nullptr;
Gtk::Label* encodeSummaryLabel = nullptr;

std::map<std::string, Gtk::Widget*> encodeWidgets;
std::map<std::string, uint8_t> encodeTypes;
std::map<std::string, Gtk::CheckButton*> encodeInclude;

static std::map<std::string, Field_Strings> LABEL_TO_FIELD;

std::map<std::string, uint8_t> activeSimTypes;

ServerUI server_ui;
VideoServerUI video_server_ui;

Gtk::Window* arenaWindow;
Gtk::Window* sensorsWindow;
ConfigEditorWindow* configWindow = nullptr;
Gtk::Window* motorWindow;
int monitor_count = 0;

// For CSS backgrounds (transparent for video overlay)
std::string darkBackgroundColorCSS = "rgba(11, 26, 33, 0.0)";
std::string lightBackgroundColorCSS = "rgba(240, 250, 242, 0.0)";

// For Cairo drawing and RGBA widgets (opaque)
std::string darkBackgroundColor = "#0b1a21";
std::string lightBackgroundColor = "#f0faf2";

bool isLightMode = true;

static constexpr int ROLL_PITCH_IMAGE_SIZE = 200;
static constexpr int BUCKET_TILT_IMAGE_SIZE = 200;
static constexpr int EDGE_PANEL_WIDTH = 250;


double roll_rotation_angle = 0.0;
Glib::RefPtr<Gdk::Pixbuf> roll_pixbuf;
Gtk::Image* roll_image;

double pitch_rotation_angle = 0.0;
Glib::RefPtr<Gdk::Pixbuf> pitch_pixbuf;
Glib::RefPtr<Gdk::Pixbuf> lvl_pixbuf;
Gtk::Image* pitch_image;
Gtk::Image* lvl_image;

double bucket_rotation_angle = 0.0;
Glib::RefPtr<Gdk::Pixbuf> bucket_rot_pixbuf;
Gtk::Image* bucket_rot_image;

double robot_x_m = 0.0;
double robot_y_m = 0.0;
double robot_pitch_rad = 0.0;
double arm_angle_deg = 0.0; 
double bucket_angle_deg = 0.0;

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

PositionBar* right_arm = nullptr;
PositionBar* left_arm = nullptr;
PositionBar* right_bucket = nullptr;
PositionBar* left_bucket = nullptr;
PositionBar* elevation_bar = nullptr;
SyncStatusLabel* armSyncLabel = nullptr;
SyncStatusLabel* bucketSyncLabel = nullptr;

Gtk::Box* armBox;
Gtk::Box* bucketBox;
Gtk::Box* elevationBox;
bool arm_init = false, bucket_init = false, roll_init = false, pitch_init = false, bucketLevel_init = false;
bool bucketRot_init = false, bucketElevation_init = false;

int right_arm_pos = 0, left_arm_pos = 0, right_bucket_pos = 0, left_bucket_pos = 0, bucket_elevation_height = 0;

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

Glib::RefPtr<Gdk::Pixbuf> rotate_image(Glib::RefPtr<Gdk::Pixbuf> pixbuf, double angle_deg, int target_width, int target_height, int high_angle, int low_angle) {
    double angle_rad = angle_deg * M_PI / 180.0;

    int width = pixbuf->get_width();
    int height = pixbuf->get_height();

    int new_width = static_cast<int>(std::abs(width * std::cos(angle_rad)) + std::abs(height * std::sin(angle_rad)));
    int new_height = static_cast<int>(std::abs(width * std::sin(angle_rad)) + std::abs(height * std::cos(angle_rad)));

    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, new_width, new_height);
    auto cr = Cairo::Context::create(surface);

    // Fill background
    cr->set_source_rgb(1.0, 1.0, 1.0); // Default to white
    if (angle_deg > high_angle || angle_deg < low_angle) {
        cr->set_source_rgb(1.0, 0.0, 0.0); // Red for high angle warning
    }
    cr->paint();

    cr->translate(new_width / 2.0, new_height / 2.0);
    cr->rotate(angle_rad);
    cr->translate(-width / 2.0, -height / 2.0);

    Gdk::Cairo::set_source_pixbuf(cr, pixbuf, 0, 0);
    cr->paint();

    Glib::RefPtr<Gdk::Pixbuf> rotated_pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, new_width, new_height);

    const unsigned char* src_pixels = surface->get_data();
    int src_stride = surface->get_stride();
    unsigned char* dest_pixels = rotated_pixbuf->get_pixels();
    int dest_stride = rotated_pixbuf->get_rowstride();

    // Manually copy pixels, converting ARGB (Cairo) to RGBA (GdkPixbuf)
    for (int y = 0; y < new_height; ++y) {
        for (int x = 0; x < new_width; ++x) {
            const guint32* src_pixel = reinterpret_cast<const guint32*>(src_pixels + y * src_stride) + x;
            guint8* dest_pixel = dest_pixels + y * dest_stride + x * 4;

            // Cairo is ARGB (BGRA in little-endian memory) -> 0xAARRGGBB
            // GdkPixbuf wants RGBA
            dest_pixel[0] = (*src_pixel >> 16) & 0xFF; // Red
            dest_pixel[1] = (*src_pixel >> 8) & 0xFF;  // Green
            dest_pixel[2] = (*src_pixel >> 0) & 0xFF;  // Blue
            dest_pixel[3] = (*src_pixel >> 24) & 0xFF; // Alpha
        }
    }

    int crop_x = std::max(0, (new_width - target_width) / 2);
    int crop_y = std::max(0, (new_height - target_height) / 2);
    Glib::RefPtr<Gdk::Pixbuf> cropped_pixbuf = rotated_pixbuf->create_subpixbuf(rotated_pixbuf, crop_x, crop_y, target_width, target_height);

    // Draw black markers (unchanged)
    unsigned char* new_pixels = cropped_pixbuf->get_pixels();
    int new_rowstride = cropped_pixbuf->get_rowstride();
    int new_channels = cropped_pixbuf->get_n_channels();

    for (int y = 98; y <= 101; ++y) {
        unsigned char* row_start = new_pixels + y * new_rowstride;
        for (int x = 0; x <= 15; ++x) {
            unsigned char* new_pixel = row_start + x * new_channels;
            new_pixel[0] = 0; new_pixel[1] = 0; new_pixel[2] = 0;
            if (new_channels == 4) new_pixel[3] = 255;
        }
        for (int x = 185; x <= 199; ++x) {
            unsigned char* new_pixel = row_start + x * new_channels;
            new_pixel[0] = 0; new_pixel[1] = 0; new_pixel[2] = 0;
            if (new_channels == 4) new_pixel[3] = 255;
        }
    }

    return cropped_pixbuf;
}

class BorderedBox : public Gtk::Box {
    public:
    BorderedBox(Gtk::Orientation orientation, int spacing)
    : Gtk::Box(orientation, spacing) {}
    
    protected:
        bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
            auto allocation = get_allocation();
            double width = allocation.get_width();
            double height = allocation.get_height();
            
            if (isLightMode) {
                set_source_hex_color(cr, lightBackgroundColor);
            } else {
                set_source_hex_color(cr, darkBackgroundColor);
            }
            cr->rectangle(0, 0, width, height);
            cr->fill();
            
            Gtk::Box::on_draw(cr); 
    
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

// Circles for motor status indicators
//Primary Bot - 3 Talons, 4 Falcons
//Dump Bot - 4 Neos, 1 Falcon
//Backup Bot - 4 Talons, 4 Falcons
CircleDrawingArea* talon1Circle;
CircleDrawingArea* talon2Circle;
CircleDrawingArea* talon3Circle;
CircleDrawingArea* talon4Circle;
CircleDrawingArea* kraken1Circle;
CircleDrawingArea* kraken2Circle;
CircleDrawingArea* kraken3Circle;
CircleDrawingArea* kraken4Circle;
CircleDrawingArea* neo1Circle;
CircleDrawingArea* neo2Circle;
CircleDrawingArea* neo3Circle;
CircleDrawingArea* neo4Circle;
CircleDrawingArea* lowerFalcon1Circle;
CircleDrawingArea* lowerFalcon2Circle;
CircleDrawingArea* lowerFalcon3Circle;
CircleDrawingArea* lowerFalcon4Circle;
CircleDrawingArea* falcon1Circle;
CircleDrawingArea* falcon2Circle;
CircleDrawingArea* falcon3Circle;
CircleDrawingArea* falcon4Circle;

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

            // Define colors for text, grid, and border based on the current mode.
            Gdk::RGBA text_color, grid_color, border_color;
            if (isLightMode) {
                set_source_hex_color(cr, lightBackgroundColor);
                text_color.set("black");
                grid_color.set_rgba(0.9, 0.9, 0.9, 1.0);
                border_color.set_rgba(0.7, 0.7, 0.7, 1.0);
            }
            else {
                set_source_hex_color(cr, darkBackgroundColor);
                text_color.set("white");
                grid_color.set_rgba(0.25, 0.25, 0.25, 1.0);
                border_color.set_rgba(0.4, 0.4, 0.4, 1.0);
            }
    
            // Clear background
            cr->paint();
    
            // Draw border
            cr->set_source_rgba(border_color.get_red(), border_color.get_green(), border_color.get_blue(), border_color.get_alpha());
            cr->rectangle(0, 0, width, height);
            cr->stroke();
    
            // Calculate grid steps based on range
            float range = maxVal - minVal;
            float step;
            
            if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                step = 0.1f; // 25% increments for output and speed
            }
            else if (graphType == POTENTIOMETER) {
                step = 100.0f; // 1V increments for potentiometer
            }
            else {
                step = (range > 1000) ? 100.0f :
                      (range > 20) ? 5.0f : 
                      (range > 10) ? 1.0f : 
                      (range > 5) ? 1.0f : 0.5f;
            }
    
            // Draw grid and labels
            cr->set_source_rgba(grid_color.get_red(), grid_color.get_green(), grid_color.get_blue(), grid_color.get_alpha());
            cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
            cr->set_font_size(10);
            
            // Special case for output percentage and speed to show 0 line
            if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                float zeroY = height - ((0 - minVal) / range) * (height - 20);
                cr->set_source_rgba(border_color.get_red(), border_color.get_green(), border_color.get_blue(), border_color.get_alpha());
                cr->move_to(0, zeroY);
                cr->line_to(width, zeroY);
                cr->stroke();
                
                cr->set_source_rgba(text_color.get_red(), text_color.get_green(), text_color.get_blue(), text_color.get_alpha());
                cr->move_to(5, zeroY - 5);
                cr->show_text("0");
            }
            
            for (float v = minVal; v <= maxVal; v += step) {
                // Skip 0 if we already drew it specially
                if ((graphType == OUTPUT_PERCENT || graphType == SPEED) && v == 0) {
                    continue;
                }
                
                float y = height - ((v - minVal) / range) * (height - 20);
                cr->set_source_rgba(grid_color.get_red(), grid_color.get_green(), grid_color.get_blue(), grid_color.get_alpha());
                cr->move_to(0, y);
                cr->line_to(width, y);
                cr->stroke();
                
                cr->set_source_rgba(text_color.get_red(), text_color.get_green(), text_color.get_blue(), text_color.get_alpha());
                cr->move_to(5, y - 5);
                
                // Format label based on value size and type
                if (graphType == OUTPUT_PERCENT || graphType == SPEED) {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(0), v * 100) + "%");
                }
                else if (graphType == POTENTIOMETER) {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(1), v) + "V");
                }
                else if (maxVal > 100) {
                    cr->show_text(Glib::ustring::format(std::fixed, std::setprecision(0), v));
                }
                else {
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

ArtificialHorizon* attitudeIndicator = nullptr;

std::map<std::string, Gtk::Label*> motorTelemetryLabels;
bool showMotorTelemetry = true;

BatteryBar* batteryBar = nullptr;


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

            cv::Mat frameToDisplay_CV = latestFrame;
            if (frameToDisplay_CV.channels() == 1) {
                cv::cvtColor(frameToDisplay_CV, frameToDisplay_CV, cv::COLOR_GRAY2RGB);
            }

            int width = frameToDisplay_CV.cols;
            int height = frameToDisplay_CV.rows;
            int cv_channels = frameToDisplay_CV.channels();
            int pixbuf_rowstride = width * cv_channels;
            size_t data_size = static_cast<size_t>(height) * pixbuf_rowstride;

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

std::map<std::string, CircleDrawingArea*> motorCircles;

void setBackgroundColors(Gdk::RGBA color){
    for (auto& kv : motorCircles) {
        if (kv.second) {
            kv.second->set_background_color(color);
        }
    }
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
        "window, notebook, box, flowbox { background-color: " + color + "; }\n"
        "#topControlsBox { background-color: " + darkBackgroundColor + "; }\n"
        ".edge-panel { background-color: " + darkBackgroundColor + "; }\n"
        "#dark_text, #dark_text label { color: #000000; }\n"
        "label, button, entry { color: #edf6fa; }\n"
        "button { border: 1px solid #edf6fa; background-color: transparent; }\n"
        
        "notebook tab { background-color: #2a2a2e; border-color: #444; }\n"
        "notebook tab label { color: #edf6fa; }\n"
        "notebook tab:checked { background-color: " + color + "; }\n";
}

// Light mode
std::string generateLightModeString(const std::string& color) {
    return
        "* { font-family: 'Proxima Nova'; font-weight: bold }\n"
        "window, notebook, box, flowbox { background-color: " + color + "; }\n"
        "#topControlsBox { background-color: " + lightBackgroundColor + "; }\n"
        ".edge-panel { background-color: " + lightBackgroundColor + "; }\n"
        "label, button, entry { color: #000000; }\n"
        "button { border: 1px solid #000000; background-color: #f0f0f0; }\n"
        
        "notebook tab { background-color: #e6e6e6; border-color: #cccccc; }\n"
        "notebook tab label { color: #000000; }\n"
        "notebook tab:checked { background-color: " + color + "; }\n";
}

void applyEdgePanelStyle(Gtk::Widget* widget) {
    if (!widget) {
        return;
    }

    widget->get_style_context()->add_class("edge-panel");
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

void toggleMode() {
    Gdk::RGBA background;
    isLightMode = !isLightMode;

    auto css_provider = Gtk::CssProvider::create();
    if (isLightMode) {
        css_provider->load_from_data(generateLightModeString(lightBackgroundColorCSS));
        background.set(lightBackgroundColor);    
    }
    else {
        css_provider->load_from_data(generateDarkModeString(darkBackgroundColorCSS));
        background.set(darkBackgroundColor);
    }

    auto screen = Gdk::Screen::get_default();
    Gtk::StyleContext::add_provider_for_screen(
        screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    for (InfoFrame* frame : infoFrameList) {
        std::string label = frame->get_label();
        std::vector<std::string> keys = getKeys(label);
        for (const std::string& key : keys) {
            updateBackgroundColor(frame, key);
        }
    }

    if (arenaWindow) {
        auto arena_css = Gtk::CssProvider::create();
        std::string arena_bg_css = "window { background-color: " + (isLightMode ? lightBackgroundColor : darkBackgroundColor) + "; }";
        arena_css->load_from_data(arena_bg_css);
        arenaWindow->get_style_context()->add_provider(arena_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    
    if (sensorsWindow) {
        auto sensors_css = Gtk::CssProvider::create();
        std::string sensors_bg_css = "window { background-color: " + (isLightMode ? lightBackgroundColor : darkBackgroundColor) + "; }";
        sensors_css->load_from_data(sensors_bg_css);
        sensorsWindow->get_style_context()->add_provider(sensors_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    if (simulatorWindow) {
        auto sim_css = Gtk::CssProvider::create();
        std::string sim_bg_css = "window { background-color: " + (isLightMode ? lightBackgroundColor : darkBackgroundColor) + "; }";
        sim_css->load_from_data(sim_bg_css);
        simulatorWindow->get_style_context()->add_provider(sim_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    if(!noVideo) {
        setBackgroundColors(background);
    }
    if (proximityBar) {
        proximityBar->set_light_mode(isLightMode);
    }
    if (attitudeIndicator) attitudeIndicator->set_light_mode(isLightMode);
    if (batteryBar) batteryBar->set_light_mode(isLightMode);
    if (armSyncLabel) armSyncLabel->set_light_mode(isLightMode);
    if (bucketSyncLabel) bucketSyncLabel->set_light_mode(isLightMode);
    if (left_arm) left_arm->set_light_mode(isLightMode);
    if (right_arm) right_arm->set_light_mode(isLightMode);
    if (left_bucket) left_bucket->set_light_mode(isLightMode);
    if (right_bucket) right_bucket->set_light_mode(isLightMode);
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

// Config-derived label sets and display name map.
// These are populated by rebuildConfigDerivedGlobals() after processArguments()
// selects the active config. They cannot be initialized at file scope because
// activeConfig may change during argument parsing.
std::set<std::string> talonLabels;
std::set<std::string> falconLabels;
std::set<std::string> krakenLabels;
std::set<std::string> neoLabels;
std::map<std::string, std::string> displayNameMap;

void rebuildConfigDerivedGlobals() {
    talonLabels  = activeConfig.getLabelsForType(MotorType::TALON);
    falconLabels = activeConfig.getLabelsForType(MotorType::FALCON);
    krakenLabels = activeConfig.getLabelsForType(MotorType::KRAKEN);
    neoLabels    = activeConfig.getLabelsForType(MotorType::NEO);
    displayNameMap = activeConfig.getDisplayNameMap();
}

CircleDrawingArea** getOrCreateCircle(const std::string& label) {
    return &motorCircles[label];  // auto-creates entry if missing
}

CircleDrawingArea* getMotorCircle(const std::string& label) {
    auto it = motorCircles.find(label);
    return (it != motorCircles.end()) ? it->second : nullptr;
}

CircleDrawingArea* getTalonCircle(const std::string& label) {
    if (label == "Talon 1") return talon1Circle;
    if (label == "Talon 2") return talon2Circle;
    if (label == "Talon 3") return talon3Circle;
    if (label == "Talon 4") return talon4Circle;
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

CircleDrawingArea* getNeoCircle(const std::string& label) {
    if (label == "Neo 1") return neo1Circle;
    if (label == "Neo 2") return neo2Circle;
    if (label == "Neo 3") return neo3Circle;
    if (label == "Neo 4") return neo4Circle;
    return nullptr;
}

CircleDrawingArea* getKrakenCircle(const std::string& label) {
    if (label == "Kraken 1") return kraken1Circle;
    if (label == "Kraken 2") return kraken2Circle;
    if (label == "Kraken 3") return kraken3Circle;
    if (label == "Kraken 4") return kraken4Circle;
    return nullptr;
}


void updateCircleColor(CircleDrawingArea* circle, bool lowVoltage, bool error) {
    if (!circle || noVideo) return;

    Gdk::RGBA color;
    if (error)
        color.set_rgba(1.0, 0.0, 0.0, 1.0); // Red
    else if (lowVoltage)
        color.set_rgba(1.0, 1.0, 0.0, 1.0); // Yellow
    else
        color.set_rgba(0.0, 1.0, 0.0, 1.0); // Green

    circle->set_color(color);
}


void updateCircleColor(CircleDrawingArea* circle, Gdk::RGBA color) {
    if (!circle || noVideo) return;
    circle->set_color(color);
}


/* Functions associated with the motor details window */
bool updateMotorDetails = false;
bool allowMotorsDoubleClick = true;
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

Speedometer* createDial(std::string label, double min_speed, double max_speed, 
                        int major_divisions, int minor_ticks, double zero_angle, double sweep){
    auto speedometer = Gtk::manage(new Speedometer(label));
    speedometer->set_size_request(300 * GUI_SCALE, 300 * GUI_SCALE);
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


/*** Functions associated with GUI initialization ***/
/**
 * Creates a position indicator widget composed of two vertical bars and labels.
 */
Gtk::Box* createPositionIndicator(const std::string& title, int spacing,
                                  PositionBar*& left_indicator,
                                  PositionBar*& right_indicator,
                                  Gtk::Box*& container_box,
                                  SyncStatusLabel*& sync_label,
                                  int max_val = 920,
                                  int box_width = 110,
                                  int indicator_width = 40,
                                  int indicator_height = 200)
{
    auto text_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 2));
    container_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, spacing));
    container_box->set_size_request(box_width * GUI_SCALE, -1);
 
    left_indicator = Gtk::manage(new PositionBar());
    left_indicator->set_size_request(indicator_width * GUI_SCALE, indicator_height * GUI_SCALE);
    left_indicator->set_range(0, max_val);
    left_indicator->set_warning_limits(max_val / 10, max_val * 9 / 10);
    left_indicator->set_light_mode(isLightMode);
    left_indicator->set_hexpand(true);
    left_indicator->set_halign(Gtk::ALIGN_CENTER);
    container_box->add(*left_indicator);
    left_indicator->show();
 
    right_indicator = Gtk::manage(new PositionBar());
    right_indicator->set_size_request(indicator_width * GUI_SCALE, indicator_height * GUI_SCALE);
    right_indicator->set_range(0, max_val);
    right_indicator->set_warning_limits(max_val / 10, max_val * 9 / 10);
    right_indicator->set_light_mode(isLightMode);
    right_indicator->set_hexpand(true);
    right_indicator->set_halign(Gtk::ALIGN_CENTER);
    container_box->add(*right_indicator);
    right_indicator->show();
 
    container_box->set_halign(Gtk::ALIGN_CENTER);
    container_box->set_valign(Gtk::ALIGN_CENTER);
 
    text_box->add(*container_box);
    text_box->set_halign(Gtk::ALIGN_CENTER);
 
    auto pos_label = Gtk::manage(new Gtk::Label("L         R"));
    auto title_label = Gtk::manage(new Gtk::Label(title));
    pos_label->set_halign(Gtk::ALIGN_CENTER);
    title_label->set_halign(Gtk::ALIGN_CENTER);
 
    text_box->add(*pos_label);
    text_box->add(*title_label);
 
    // Sync status badge
    sync_label = Gtk::manage(new SyncStatusLabel());
    sync_label->set_light_mode(isLightMode);
    sync_label->set_halign(Gtk::ALIGN_CENTER);
    text_box->add(*sync_label);
 
    return text_box;
}
 
Gtk::Box* createSinglePositionIndicator(const std::string& title,
                                         PositionBar*& indicator,
                                         int max_val = 920,
                                         int indicator_width = 40,
                                         int indicator_height = 200)
{
    auto text_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 2));
    text_box->set_halign(Gtk::ALIGN_CENTER);
 
    indicator = Gtk::manage(new PositionBar());
    indicator->set_size_request(indicator_width * GUI_SCALE, indicator_height * GUI_SCALE);
    indicator->set_range(0, max_val);
    indicator->set_warning_limits(max_val / 10, max_val * 9 / 10);
    indicator->set_light_mode(isLightMode);
    indicator->set_hexpand(false);
    indicator->set_halign(Gtk::ALIGN_CENTER);
    text_box->add(*indicator);
    indicator->show();
 
    auto title_label = Gtk::manage(new Gtk::Label(title));
    title_label->set_halign(Gtk::ALIGN_CENTER);
    text_box->add(*title_label);
 
    return text_box;
}

/**
 * Creates and initializes an image widget from a file.
 */
bool createImageIndicator(Gtk::Image*& image_widget, Glib::RefPtr<Gdk::Pixbuf>& pixbuf, 
                          const std::string& file_path, Gtk::Container* parent, double initial_rotation,
                          int high_angle, int low_angle, int target_size = 200)
{
    image_widget = Gtk::manage(new Gtk::Image());
    try {
        pixbuf = Gdk::Pixbuf::create_from_file(file_path);
    } catch(const Glib::FileError& e) {
        g_print("Failed to load image: %s\n", e.what().c_str());
        return false;
    }
    
    parent->add(*image_widget);

    Glib::RefPtr<Gdk::Pixbuf> new_pixbuf = rotate_image(pixbuf, initial_rotation, target_size, target_size, high_angle, low_angle);
    image_widget->set(new_pixbuf);
    return true;
}

static void center_image_widget(Gtk::Image* image_widget) {
    if (!image_widget) return;
    image_widget->set_hexpand(true);
    image_widget->set_vexpand(true);
    image_widget->set_halign(Gtk::ALIGN_CENTER);
    image_widget->set_valign(Gtk::ALIGN_CENTER);
}

void initRoll() {
    if (!roll_init) {
        attitudeIndicator = Gtk::manage(new ArtificialHorizon("Attitude"));
        attitudeIndicator->set_size_request(ROLL_PITCH_IMAGE_SIZE * GUI_SCALE,
                                             (ROLL_PITCH_IMAGE_SIZE + 30) * GUI_SCALE);
        attitudeIndicator->set_warning_angles(30.0, -30.0);
        attitudeIndicator->set_light_mode(isLightMode);
        attitudeIndicator->set_halign(Gtk::ALIGN_CENTER);
        attitudeIndicator->set_valign(Gtk::ALIGN_END);

        Gtk::Container* parent = noVideo
            ? static_cast<Gtk::Container*>(sensorBox)
            : (rollImagePlaceholder
                ? static_cast<Gtk::Container*>(rollImagePlaceholder)
                : static_cast<Gtk::Container*>(bottomLowerBox));

        parent->add(*attitudeIndicator);
        roll_init = true;
        pitch_init = true;  // Combined widget handles both axes
        window->show_all();
    }
}

void initPitch() {
    // Combined attitude indicator is created by initRoll().
    // This function exists so existing call sites don't break.
    if (!pitch_init && !roll_init) {
        initRoll();
    }
    pitch_init = true;
}

void initMechanisms() {
    for (const auto& mech : activeConfig.mechanisms) {
        if (mech.name == "Arm") {
            if (mech.mode == MechanismMode::PAIRED) {
                auto* widget = createPositionIndicator("Arm Positions", 4,
                    left_arm, right_arm, armBox, armSyncLabel,
                    mech.sensorMax, 90, 28, 200);
                // ... add to placeholder ...
            }
            else if (mech.mode == MechanismMode::SINGLE) {
                auto* widget = createSinglePositionIndicator(
                    "Arm Position", left_arm, mech.sensorMax, 40, 200);
                // ... add to placeholder ...
            }
            // NONE mode: do nothing
            arm_init = true;
        }
        else if (mech.name == "Bucket") {
            // Same pattern
            bucket_init = true;
        }
    }
    // If no Arm mechanism defined, arm_init stays false until data arrives
    // and the lazy init in updateGUI handles it — or just set the flag:
    if (!activeConfig.findMechanism("Arm")) arm_init = true;
    if (!activeConfig.findMechanism("Bucket")) bucket_init = true;
}
void initBucketLvl() {
    if (!bucketLevel_init) {
        if (!noVideo) {
            auto* padding = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
            padding->set_size_request(100, 100);
            bottomLowerBox->add(*padding);
        }

        Gtk::Container* parent = noVideo ? static_cast<Gtk::Container*>(sensorBox) : bottomLowerBox;
        if (createImageIndicator(lvl_image, lvl_pixbuf, "../resources/newbucket.png", parent, 0, 30, -30)) {
            bucketLevel_init = true;
            window->show_all();
        }
    }
}

void initArmPos() {
    if (!arm_init) {
        if (dumpBot) {
            arm_init = true;
            return;
        }
 
        Gtk::Widget* arm_widget;
        if (backupBot) {
            arm_widget = createSinglePositionIndicator("Arm Position", left_arm, 920, 40, 200);
        }
        else {
            arm_widget = createSinglePositionIndicator("Arm Position", left_arm, 920, 40, 200);
            
            // Currently the primary and backup bot both have a single arm actuator
            // This might change depending on the new bot design
            //arm_widget = createPositionIndicator(
            //    "Arm Positions", 4,
            //    left_arm, right_arm, armBox, armSyncLabel,
            //    920, 90, 28, 200);
        }
 
        if (noVideo)
            sensorBox->add(*arm_widget);
        else if (armPositionPlaceholder)
            armPositionPlaceholder->add(*arm_widget);
        else
            innerLeftBox->add(*arm_widget);
 
        arm_init = true;
        window->show_all();
    }
}

void initBucketPos() {
    if (!bucket_init) {
        if (dumpBot) {
            bucket_init = true;
            return;
        }
 
        Gtk::Widget* bucket_widget;
        if (backupBot) {
            bucket_widget = createSinglePositionIndicator("Bucket Position", left_bucket, 700, 40, 180);
        }
        else {
            bucket_widget = createSinglePositionIndicator("Bucket Position", left_bucket, 700, 40, 180);
            //bucket_widget = createPositionIndicator(
            //    "Bucket Positions", 20,
            //    left_bucket, right_bucket, bucketBox, bucketSyncLabel,
            //    700, 110, 40, 180);
        }
 
        if (noVideo)
            sensorBox->add(*bucket_widget);
        else
            innerRightBox->add(*bucket_widget);
 
        bucket_init = true;
        window->show_all();
    }
}

void initBucketElevation() {
    if (!bucketElevation_init) {
        if (dumpBot) {
            bucketElevation_init = true;
            return;
        }

        Gtk::Widget* elevation_widget;
        if (backupBot) {
            elevation_widget = createSinglePositionIndicator("Bucket Elevation", elevation_bar, 900, 40, 200);
        }

        else {
            elevation_widget = createSinglePositionIndicator("Bucket Elevation", elevation_bar, 900, 40, 200);
        }

        if (noVideo)
            sensorBox->add(*elevation_widget);
        else
            innerRightBox->add(*elevation_widget);

        bucketElevation_init = true;
        window->show_all();
    }
}

void initBucketRot() {
    if (!bucketRot_init) {

        Gtk::Container* parent = noVideo ? static_cast<Gtk::Container*>(sensorBox)
                                         : (bucketTiltPlaceholder ? static_cast<Gtk::Container*>(bucketTiltPlaceholder)
                                                                  : static_cast<Gtk::Container*>(innerLeftBox));

        bool success = createImageIndicator(bucket_rot_image, bucket_rot_pixbuf, "../resources/newbucket.png", parent,
            bucket_rotation_angle, 45, -90, BUCKET_TILT_IMAGE_SIZE);

        if (success) {
            center_image_widget(bucket_rot_image);
            bucketRot_init = true;
            window->show_all();
        }
    }
}

void updateMotorTelemetry(const std::string& display_name, float voltage, float current) {
    auto it = motorTelemetryLabels.find(display_name);
    if (it == motorTelemetryLabels.end() || !it->second) return;
 
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1fV  %.1fA", voltage, current);
    it->second->set_text(buf);
 
    Gdk::RGBA color;
    if (voltage < LOW_VOLTAGE) {
        color.set_rgba(0.94, 0.27, 0.27, 1.0);
    } else if (voltage < LOW_VOLTAGE + 1.0f) {
        color.set_rgba(0.98, 0.75, 0.17, 1.0);
    } else {
        color.set_rgba(0.29, 0.85, 0.50, 1.0);
    }
    it->second->override_color(color);
}

/*** Functions associated with GUI Updates ***/
const std::unordered_set<std::string> validLabels = {
    "Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4",
    "Talon 1", "Talon 2", "Talon 3", "Talon 4",
    "Neo 1", "Neo 2", "Neo 3", "Neo 4",
    "Kraken 1", "Kraken 2", "Kraken 3", "Kraken 4",
    "Linear 1", "Linear 2", "Linear 3", "Linear 4",
    "Zed", "Autonomy", "Communication", "Power", "Power2", "Drivetrain", "Lidar"
};

void addElementToInfoFrame(std::string label, InfoFrame* frame, const Element& element) {
    std::map<std::string, bool>& values = getMap(label);
    auto it = values.find(element.label);
    bool end = it == values.end();
    if(it == values.end() || !it->second){
        return;
    }

    addElementToInfoFrame(frame, element);
}

void updateBucketRotationImage() {
    bucket_rotation_angle = -roll_rotation_angle + arm_angle_deg + bucket_angle_deg;

    if (bucketRot_init && bucket_rot_image && bucket_rot_pixbuf) {
        bucket_rot_image->set(rotate_image(bucket_rot_pixbuf, bucket_rotation_angle, BUCKET_TILT_IMAGE_SIZE, BUCKET_TILT_IMAGE_SIZE, 45, -90));
    }
}

/*
The following functions with the names handleNodeElements handle any 
specific logic that is required to update any widgets that use the 
values from the node. The Generic elements function then updates the 
values displayed in the sensors tab.
*/
void handleZedElements(const std::vector<Element>& elements) {
    for (const auto& element : elements) {
        if (element.type != TYPE::FLOAT32) continue;
        float value = element.data.front().float32;

        if (element.label == "yaw") {
            pitch_rotation_angle = std::round(value);
            if (attitudeIndicator) attitudeIndicator->set_pitch(pitch_rotation_angle);
        }
        else if (element.label == "Z") {
            robot_y_m = -value; 
        }
        else if (element.label == "X") {
            robot_x_m = -value; 
        }
        else if (element.label == "pitch") {
            robot_pitch_rad = value * (M_PI / 180.0);
        }
        else if (element.label == "roll") {
            roll_rotation_angle = std::round(value);
            if (attitudeIndicator) attitudeIndicator->set_roll(-roll_rotation_angle);
        
            updateBucketRotationImage();
        }
    }
}

void handleDrivetrainElements(const std::vector<Element>& elements) {
    
}

void handleLidarElements(const std::vector<Element>& elements) {
    for (const auto& element : elements) {
        if (element.label == "Distance") {
            float distance_m = 0.0f;
            if (element.type == TYPE::UINT16) {
                distance_m = element.data.front().uint16 / 1000.0f;
            }
 
            if (proximityBar) {
                proximityBar->set_distance(distance_m);
            }
        }
    }
}

void handleTalonElements(const std::string& label, const std::vector<Element>& elements) {
    bool lowVoltage = false;
    float voltage_val = 0.0f;
    float current_val = 0.0f;
    for (const auto& element : elements) {
        if (element.label == "Sensor Position") {
            int pos = element.data.front().uint16;
            if (label == "Talon 1") {
                left_arm_pos = pos;
                if (left_arm) left_arm->set_position(pos);
                arm_angle_deg = ((pos - 20) / 900.0) * -57.2 + 17.1;
                std::cout << "pos: " << pos << std::endl;
                std::cout << "arm_angle_deg: " << arm_angle_deg << std::endl;

                updateBucketRotationImage();

                if (!dumpBot) {
                    if (proximityBar) {
                        proximityBar->set_arm_position(pos);
                    }
                }
            }
            if(label == "Talon 2") {
                right_arm_pos = pos;
                if (right_arm) right_arm->set_position(pos);
            }
            else if (label == "Talon 3") {
                left_bucket_pos = pos;
                if (left_bucket) left_bucket->set_position(pos);

                bucket_angle_deg = ((pos - 20) / 900.0) * 97.4 - 25.8;
                std::cout << "pos: " << pos << std::endl;
                std::cout << "bucket_angle_deg: " << bucket_angle_deg << std::endl;

                updateBucketRotationImage();
            }
            else if (label == "Talon 4") {
                right_bucket_pos = pos;
                if (right_bucket) right_bucket->set_position(pos);
            }

            if (label == "Talon 1" || label == "Talon 2"){
                if (armSyncLabel) armSyncLabel->update(left_arm_pos, right_arm_pos, 50);
            }
            else{
                if (bucketSyncLabel) bucketSyncLabel->update(left_bucket_pos, right_bucket_pos, 50);
            }
            if (!noVideo) talonPositionGraph->update_data(label, pos);
        }
        else if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            voltage_val = voltage;
            if (!noVideo) talonVoltageGraph->update_data(label, voltage);
            lowVoltage = voltage < LOW_VOLTAGE;
            if (batteryBar) batteryBar->report_voltage(label, voltage);
        }
        else if (element.label == "Output Current") {
            float current = element.data.front().uint16 / 100.0f;
            current_val = current;
            if (!noVideo) talonCurrentGraph->update_data(label, current);
        }
        else if (element.label == "Output Percent") {
            float percent = element.data.front().float32;
            if (!noVideo) talonOutputGraph->update_data(label, percent);
        }
    }
    updateCircleColor(getMotorCircle(label), false, lowVoltage);

    auto nameIt = displayNameMap.find(label);
    if (nameIt != displayNameMap.end()) {
        updateMotorTelemetry(nameIt->second, voltage_val, current_val);
    }
}

struct MotorState {
    bool error = false;
    bool lowVoltage = false;
};

std::map<std::string, MotorState> motorStates;

void handleFalconElements(const std::string& label, const std::vector<Element>& elements) {
    float voltage_val = 0.0f;
    float current_val = 0.0f;
    for (const auto& element : elements) {
        if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            voltage_val = voltage;
            if (!noVideo) falconVoltageGraph->update_data(label, voltage);
            bool lowVoltage = voltage < LOW_VOLTAGE;
            motorStates[label].lowVoltage = lowVoltage;
            if (batteryBar) batteryBar->report_voltage(label, voltage);
        }
        else if (element.label == "Output Current") {
            float current = element.data.front().uint16 / 100.0f;
            current_val = current;
            if (!noVideo) falconCurrentGraph->update_data(label, current);
        }
        else if (element.label == "Output Percent") {
            float percent = element.data.front().float32;
            if (!noVideo && falconOutputGraph) falconOutputGraph->update_data(label, percent);
            if ((label == "Falcon 2" || label == "Falcon 4") && leftSpeedometer) {
                leftSpeedometer->set_speed(percent * 100.0);
            }
            if ((label == "Falcon 1" || label == "Falcon 3") && rightSpeedometer) {
                rightSpeedometer->set_speed(percent * 100.0);
            }
        }
        else if (element.label == "Error"){
            bool error = element.data.front().boolean;
            motorStates[label].error = error;
        }
    }
    updateCircleColor(getMotorCircle(label), motorStates[label].lowVoltage, motorStates[label].error);

    auto nameIt = displayNameMap.find(label);
    if (nameIt != displayNameMap.end()) {
        updateMotorTelemetry(nameIt->second, voltage_val, current_val);
    }
}

void handleNeoElements(const std::string& label, const std::vector<Element>& elements) {
    float voltage_val = 0.0f;
    float current_val = 0.0f;
    for (const auto& element : elements) {
        if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            voltage_val = voltage;
            if (!noVideo) falconVoltageGraph->update_data(label, voltage);
            motorStates[label].lowVoltage = voltage < LOW_VOLTAGE;
            if (batteryBar) batteryBar->report_voltage(label, voltage);
        }
        else if (element.label == "Output Current") {
            float current = element.data.front().uint16 / 100.0f;
            current_val = current;
            if (!noVideo) falconCurrentGraph->update_data(label, current);
        }
        else if (element.label == "Output Percent") {
            float percent = element.data.front().float32;
            if (!noVideo) falconOutputGraph->update_data(label, percent);
            if(label == "Neo 2" || label == "Neo 4"){
                leftSpeedometer->set_speed(percent * 100.0);
            }
            if(label == "Neo 1" || label == "Neo 3"){
                rightSpeedometer->set_speed(percent * 100.0);
            }
        }
        else if (element.label == "Error"){
            bool error = element.data.front().boolean;
            motorStates[label].error = error;
        }
    }
    updateCircleColor(getMotorCircle(label), motorStates[label].lowVoltage, motorStates[label].error);
    auto neoNameIt = displayNameMap.find(label);
    if (neoNameIt != displayNameMap.end()) {
        updateMotorTelemetry(neoNameIt->second, voltage_val, current_val);
    }
}

void handleKrakenElements(const std::string& label, const std::vector<Element>& elements) {
    float voltage_val = 0.0f;
    float current_val = 0.0f;
    for (const auto& element : elements) {
        if (element.label == "Bus Voltage") {
            float voltage = element.data.front().uint16 / 100.0f;
            voltage_val = voltage;
            if (!noVideo) falconVoltageGraph->update_data(label, voltage);
            bool lowVoltage = voltage < LOW_VOLTAGE;
            motorStates[label].lowVoltage = lowVoltage;
            if (batteryBar) batteryBar->report_voltage(label, voltage);
        }
        else if (element.label == "Output Current") {
            float current = element.data.front().uint16 / 100.0f;
            current_val = current;
            if (!noVideo) falconCurrentGraph->update_data(label, current);
        }
        else if (element.label == "Output Percent") {
            float percent = element.data.front().float32;
            if (!noVideo) falconOutputGraph->update_data(label, percent);
            if(label == "Kraken 2" || label == "Kraken 4"){
                leftSpeedometer->set_speed(percent * 100.0);
            }
            if(label == "Kraken 1" || label == "Kraken 3"){
                rightSpeedometer->set_speed(percent * 100.0);
            }
        }
        else if (element.label == "Error"){
            bool error = element.data.front().boolean;
            motorStates[label].error = error;
        }
        updateCircleColor(getMotorCircle(label), motorStates[label].lowVoltage, motorStates[label].error);
    }
    auto krakenNameIt = displayNameMap.find(label);
    if (krakenNameIt != displayNameMap.end()) {
        updateMotorTelemetry(krakenNameIt->second, voltage_val, current_val);
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
            destX = element.data.front().float32;
            // TODO: Add destination to Foxglove
        }
        else if(element.label == "Dest Z"){
            destY = element.data.front().float32;
            // TODO: Add destination to Foxglove
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
                updateBackgroundColor(frame, element.label);

                if (element.label == "Bus Voltage" && val < LOW_VOLTAGE) {
                    frame->setBackground(element.label, "#FF0000");
                    frame->setTextColor(element.label, "white", true);
                }
                
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

void updateGUI(BinaryMessage& message) {
    std::string label = message.getLabel();

    for (InfoFrame* frame : infoFrameList) {
        if (frame->get_label() != label) continue;

        const auto& elements = message.getObject().elementList;

        const MotorDef* motorDef = activeConfig.findMotor(label);
        if (motorDef) {
            switch (motorDef->type) {
                case MotorType::TALON:  handleTalonElements(label, elements);  break;
                case MotorType::FALCON: handleFalconElements(label, elements); break;
                case MotorType::NEO:    handleNeoElements(label, elements);    break;
                case MotorType::KRAKEN: handleKrakenElements(label, elements); break;
            }
        }
        if (label == "Zed") {
            handleZedElements(elements);
        }
        else if (label == "Communication") {
            handleCommunicationElements(frame, elements);
        }
        else if(label == "Autonomy"){
            handleAutonomyElements(label, elements);
        }
        else if(label == "Drivetrain"){
            handleDrivetrainElements(elements);
        }
        else if (label == "Lidar" && !dumpBot) {
            handleLidarElements(elements);
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
    if ((label == "Talon 3" || label == "Talon 4") && !bucket_init){
        initBucketPos();
        initBucketRot();
        initBucketElevation();
    }
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
    if(label == "Talon 1" || label == "Talon 2" || label == "Talon 3" || label == "Talon 4" || 
       label == "Falcon 1" || label == "Falcon 2" || label == "Falcon 3" || label == "Falcon 4"
       || label == "Neo 1" || label == "Neo 2" || label == "Neo 3" || label == "Neo 4"
       || label == "Kraken 1" || label == "Kraken 2" || label == "Kraken 3" || label == "Kraken 4") {
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

/**
 * @brief Processes an incoming payload, decompressing it only if necessary.
 * * This function reads the first byte of the payload as a flag.
 * - If the flag is '1', it assumes the data is compressed, extracts the
 * original size, and performs zlib decompression.
 * - If the flag is '0', it assumes the data is uncompressed and copies it directly.
 * * @param received_payload The raw data buffer received from the socket.
 * @param processed_data A vector that will be filled with the final, usable data.
 * @return True if processing was successful, false otherwise.
 */
bool process_payload(const std::vector<uint8_t>& received_payload, std::vector<uint8_t>& processed_data) {
    if (received_payload.empty()) {
        return false;
    }

    // Read the first byte as the compression flag.
    uint8_t compression_flag = received_payload[0];

    if (compression_flag == 1) {
        if (received_payload.size() < 5) { // 1-byte flag + 4-byte size
            std::cerr << "Error: Compressed payload is too small." << std::endl;
            return false;
        }

        // Extract the original uncompressed size from the next 4 bytes.
        uLong original_size = 0;
        original_size |= static_cast<uLong>(received_payload[1]) << 24;
        original_size |= static_cast<uLong>(received_payload[2]) << 16;
        original_size |= static_cast<uLong>(received_payload[3]) << 8;
        original_size |= static_cast<uLong>(received_payload[4]) << 0;
        
        processed_data.resize(original_size);
        uLongf dest_len = processed_data.size();

        // Point to the actual compressed data (after flag and size).
        const Bytef* source = received_payload.data() + 5;
        uLong source_len = received_payload.size() - 5;

        // Perform decompression.
        int result = uncompress(processed_data.data(), &dest_len, source, source_len);
        if (result != Z_OK) {
            std::cerr << "Decompression failed with error: " << result << std::endl;
            return false;
        }
        processed_data.resize(dest_len);

    } else {
        // Just copy the data, skipping the '0' flag byte.
        processed_data.assign(received_payload.begin() + 1, received_payload.end());
    }

    return true;
}

// This function populates a binary message with default values for all of the values that are
// associated with the particular info frame
void populateBinaryMessage(const std::string& name, const std::string& prefix, BinaryMessage& message) {
    std::string vector_name = getNameFromPrefix(prefix);
    auto keys_it = get_key_vectors().find(vector_name);
    auto defs_it = get_element_definitions().find(prefix);
    if (keys_it == get_key_vectors().end() || defs_it == get_element_definitions().end()) {
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
        if(primaryBot){
            createMessage("Talon 1", "TALON");
            createMessage("Talon 3", "TALON");
            createMessage("Kraken 1", "KRAKEN");
            createMessage("Kraken 2", "KRAKEN");
            createMessage("Kraken 3", "KRAKEN");
            createMessage("Kraken 4", "KRAKEN");
            createMessage("Linear 1", "LINEAR");
            createMessage("Linear 3", "LINEAR");
            createMessage("Lidar", "LIDAR");
        }
        else if(backupBot){
            createMessage("Talon 1", "TALON");
            createMessage("Talon 3", "TALON");
            createMessage("Falcon 1", "FALCON");
            createMessage("Falcon 2", "FALCON");
            createMessage("Falcon 3", "FALCON");
            createMessage("Falcon 4", "FALCON");
            createMessage("Linear 1", "LINEAR");
            createMessage("Linear 3", "LINEAR");
            createMessage("Lidar", "LIDAR");
        }
        else if(dumpBot){
            createMessage("Neo 1", "NEO");
            createMessage("Neo 2", "NEO");
            createMessage("Neo 3", "NEO");
            createMessage("Neo 4", "NEO");
            createMessage("Falcon 1", "FALCON");
        }
        
        initRoll();
        initPitch();
        if (!dumpBot) {
            initBucketPos();
            initArmPos();
            initBucketElevation();
            initBucketRot();
        }
        
        createMessage("Communication", "COMMUNICATION");
        createMessage("Autonomy", "AUTONOMY");
        createMessage("Zed", "ZED");
        createMessage("Power", "POWER");
        createMessage("Power2", "POWER2");
        createMessage("Drivetrain", "DRIVETRAIN");
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
                updateBackgroundColor(frame, key); 
            }
        }
    }

    initGUI();
}


/*** Helper functions and variables for the video and robot server connections ***/
bool contains(std::vector<std::string>& list, std::string& value){
    for(std::string storedValue: list) if(storedValue==value) return true;
    return false;
}


/*** Functions associated with the server ***/
void resetUIOnDisconnect() {
    for (InfoFrame* frame : infoFrameList) {
        frame->setAllItemsStale();
    }


    // Reset the motor status indicator circles to black
    if(!noVideo) {
        Gdk::RGBA black;
        black.set_rgba(0.0, 0.0, 0.0, 1.0);
        for (auto& kv : motorCircles) {
            updateCircleColor(kv.second, black);
        }
    }
    if (batteryBar) batteryBar->reset_cycle();
}


/*** Functions associated with the Gear Select dial ***/
/* This is intended to show the user the speed multiplier that
the robot is currently using. It should be updated to something
better than the current, rudimentary impelementation. */

Gtk::Stack* create_gear_dial(const std::string& initial_gear,
                             const std::vector<std::string>& gears,
                             std::map<std::string, Gtk::Label*>& gear_labels)
{
    auto* stack = Gtk::manage(new Gtk::Stack());
    stack->set_size_request(80, 120);
    stack->set_transition_type(Gtk::STACK_TRANSITION_TYPE_NONE);

    for (const auto& gear : gears) {
        auto* label = Gtk::manage(new Gtk::Label());
        label->set_margin_top(8);
        label->set_margin_bottom(8);
        label->set_alignment(0.5, 0.5);
        label->set_markup("<span size='20480' weight='bold' foreground='black'>" + gear + "</span>");
        gear_labels[gear] = label;
        label->show();

        stack->add(*label, gear);  // gear string becomes child name
    }

    if (gear_labels.find(initial_gear) != gear_labels.end()) {
        stack->set_visible_child(initial_gear);
    }

    return stack;
}

void highlight_gear(const std::string& current_gear,
                    const std::vector<std::string>& gears,
                    const std::map<std::string, Gtk::Label*>& gear_labels,
                    Gtk::Stack* stack)
{
    for (const auto& gear : gears) {
        auto* label = gear_labels.at(gear);

        if (gear != current_gear) // indicates an error with the gear widget
        {
            label->set_markup("<span size='20480' weight='bold' foreground='red'>" + gear + "</span>");
        }
        else
        {
            label->set_markup("<span size='20480' weight='bold' foreground='black'>" + gear + "</span>");
        }
    }

    stack->set_visible_child(current_gear);
}

std::vector<std::string> gears = {"M", "5", "4", "3", "2", "1"};
std::map<std::string, Gtk::Label*> gear_labels;
Gtk::Stack* gear_dial = nullptr;
std::string currentGear = "3";

void increaseGear(){
    auto it = std::find(gears.begin(), gears.end(), currentGear);
    if (it != gears.begin()) {
        std::string nextGear = *std::prev(it);  // Increase gear
        currentGear = nextGear;
        highlight_gear(currentGear, gears, gear_labels, gear_dial);
    } else {
        std::cout << "Already at highest gear." << std::endl;
    }
}

void decreaseGear(){
    auto it = std::find(gears.begin(), gears.end(), currentGear);
    if (it != gears.end() && std::next(it) != gears.end()) {
        std::string nextGear = *std::next(it);  // Decrease gear
        currentGear = nextGear;
        highlight_gear(currentGear, gears, gear_labels, gear_dial);
    } else {
        std::cout << "Already at lowest gear." << std::endl;
    }
}


/*** Functions associated with the config button and functionality ***/
std::map<std::string, std::string> tooltip_map = {
    {"DISPLAY_SPEED", "Show or hide the speedometer."},
    {"NUMBERS_INSIDE", "Display numbers inside the speedometer ring."},
    {"NUMBER_TICKS", "Align numbers with speedometer tick marks."},
    {"SHOW_FALCON_Device ID", "Show Falcon CAN ID in the telemetry frame."},
    {"SHOW_MOTOR_TELEMETRY", "Show voltage and current under motor status indicators."}
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
    *powerFrame = nullptr, *power2Frame = nullptr, *drivetrainFrame = nullptr;

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
        {"DRIVETRAIN",    drivetrainFrame},
    };
}

std::map<std::string, Gtk::CheckButton*> bool_buttons;
std::vector<std::string> local_talon_keys = get_talon_keys();
std::vector<std::string> local_falcon_keys = get_falcon_keys();
std::vector<std::string> local_neo_keys = get_neo_keys();
std::vector<std::string> local_kraken_keys = get_kraken_keys();
std::vector<std::string> local_linear_keys = get_linear_keys();
std::vector<std::string> local_autonomy_keys = get_autonomy_keys();
std::vector<std::string> local_communication_keys = get_communication_keys();
std::vector<std::string> local_power2_keys = get_power2_keys();
std::vector<std::string> local_power_keys = get_power_keys();
std::vector<std::string> local_zed_keys = get_zed_keys();
std::vector<std::string> local_drivetrain_keys = get_drivetrain_keys();

std::map<std::string, std::vector<std::string>*> local_key_vectors = {};

void setup_local_key_vectors() {
    local_key_vectors.clear();
 
    // Motor-type keys
    if (!activeConfig.getLabelsForType(MotorType::TALON).empty())
        local_key_vectors["Talon"] = &local_talon_keys;
    if (!activeConfig.getLabelsForType(MotorType::FALCON).empty())
        local_key_vectors["Falcon"] = &local_falcon_keys;
    if (!activeConfig.getLabelsForType(MotorType::NEO).empty())
        local_key_vectors["Neo"] = &local_neo_keys;
    if (!activeConfig.getLabelsForType(MotorType::KRAKEN).empty())
        local_key_vectors["Kraken"] = &local_kraken_keys;
 
    local_key_vectors["Linear"] = &local_linear_keys;
    local_key_vectors["Autonomy"] = &local_autonomy_keys;
    local_key_vectors["Communication"] = &local_communication_keys;
    local_key_vectors["Power2"] = &local_power2_keys;
    local_key_vectors["Power"] = &local_power_keys;
    local_key_vectors["Zed"] = &local_zed_keys;
    local_key_vectors["Drivetrain"] = &local_drivetrain_keys;
}


void create_config_editor_window(const std::string& config_file) {
    if (configWindow) {
        configWindow->present();
        return;
    }

    if (!allowConfig) {
        return;
    }
    
    allowConfig = false;
    configWindow = new ConfigEditorWindow(config_file);
    configWindow->signal_hide().connect([]() {
        configWindow = nullptr; // Reset the pointer, as the object is now destroyed.
        allowConfig = true;     // Allow a new window to be created next time.
    });

    configWindow->show();
}


/*** Helper functions for creating GUI windows / binding events ***/
std::string current_ip = "http://192.168.1.8";
void send_servo_command(const std::string& direction) {
    if(wsl)
        return;
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
        case GDK_KEY_o:
        case GDK_KEY_p:
            send_servo_command("stop");
            return false;
            break;
        case GDK_KEY_i:
            if(!wsl){
                send_servo_command("stop");
                return false;
            }
            break;
    }
    sendKeyboardEvent(key_event->keyval, 0); // 0 for key release
    return false;
}

bool on_key_press_event(GdkEventKey* key_event){
    switch (key_event->keyval) {
        case GDK_KEY_u:
            send_servo_command("left");
            return false;
            break;
        case GDK_KEY_i:
            if(!wsl){
                send_servo_command("right");
                return false;
            }   
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
    
    sendKeyboardEvent(key_event->keyval, 1); // 1 for key press
    return false;
}

Gtk::EventBox* create_labeled_box(const Glib::ustring& label_text,
                                   CircleDrawingArea*& out_circle,
                                   bool right = false) {
    auto event_box = Gtk::manage(new Gtk::EventBox());
    auto box = Gtk::manage(new BorderedBox(Gtk::ORIENTATION_HORIZONTAL, 5));
    box->set_size_request(200 * GUI_SCALE, 75 * GUI_SCALE);
 
    auto label_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
    label_box->set_hexpand(true);
    label_box->set_valign(Gtk::ALIGN_CENTER);
 
    auto label = Gtk::manage(new Gtk::Label(label_text));
    Pango::FontDescription font;
    font.set_size(18 * GUI_SCALE * Pango::SCALE);
    label->override_font(font);
    label->set_halign(right ? Gtk::ALIGN_END : Gtk::ALIGN_START);
    label_box->add(*label);
 
    // Telemetry line — hidden when showMotorTelemetry is false
    auto telem_label = Gtk::manage(new Gtk::Label("--V  --A"));
    Pango::FontDescription telem_font;
    telem_font.set_family("monospace");
    telem_font.set_size(9 * GUI_SCALE * Pango::SCALE);
    telem_label->override_font(telem_font);
    telem_label->set_halign(right ? Gtk::ALIGN_END : Gtk::ALIGN_START);
    Gdk::RGBA dim_color;
    dim_color.set_rgba(0.5, 0.5, 0.5, 0.7);
    telem_label->override_color(dim_color);
    telem_label->set_no_show_all(!showMotorTelemetry);
    telem_label->set_visible(showMotorTelemetry);
    label_box->add(*telem_label);
 
    motorTelemetryLabels[label_text] = telem_label;
 
    out_circle = Gtk::manage(new CircleDrawingArea());
    out_circle->set_size_request(75 * GUI_SCALE, 75 * GUI_SCALE);
    out_circle->set_hexpand(false);
    out_circle->set_halign(Gtk::ALIGN_CENTER);
 
    if (right) {
        box->add(*label_box);
        box->add(*out_circle);
    } else {
        box->add(*out_circle);
        box->add(*label_box);
    }
 
    event_box->add(*box);
    event_box->add_events(Gdk::BUTTON_PRESS_MASK);
    event_box->set_visible_window(false);
    return event_box;
}

void setMotorTelemetryVisible(bool visible) {
    showMotorTelemetry = visible;
    for (auto& kv : motorTelemetryLabels) {
        if (kv.second) {
            kv.second->set_visible(visible);
            kv.second->set_no_show_all(!visible);
        }
    }
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
    column->set_size_request(200, 300);
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
    column->set_size_request(200, -1);
    column->set_hexpand(true);
    column->set_vexpand(false);
    column->set_valign(Gtk::ALIGN_END);

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

void on_connection_finished() {
    update_connection_status(server_ui);
}

void on_connection2_finished() {
    update_connection_status2(server_ui);
}

void on_video_connection_finished() {
    update_video_connection_status(video_server_ui);
}

static std::string get_glade_widget_id(Gtk::Widget* widget) {
    if (!widget) return "";

    const gchar* buildable_name = gtk_buildable_get_name(GTK_BUILDABLE(widget->gobj()));
    if (buildable_name && buildable_name[0] != '\0') {
        return std::string(buildable_name);
    }

    const Glib::ustring css_name = widget->get_name();
    if (!css_name.empty()) {
        return css_name.raw();
    }

    const char* type_name = G_OBJECT_TYPE_NAME(widget->gobj());
    return type_name ? std::string(type_name) : std::string("GtkWidget");
}

static void install_glade_debug_overlay(Gtk::Widget* widget) {
    if (!widget) return;

    const std::string widget_id = get_glade_widget_id(widget);

    widget->signal_draw().connect(
        [widget, widget_id](const Cairo::RefPtr<Cairo::Context>& cr) -> bool {
            const int width = widget->get_allocated_width();
            const int height = widget->get_allocated_height();
            if (width <= 2 || height <= 2) return false;

            // Draw a red border around each Glade widget for layout debugging.
            cr->save();
            cr->set_source_rgba(1.0, 0.0, 0.0, 0.9);
            cr->set_line_width(1.0);
            cr->rectangle(0.5, 0.5, width - 1.0, height - 1.0);
            cr->stroke();

            // Draw the widget's Glade ID centered in the widget bounds.
            auto layout = widget->create_pango_layout(widget_id);
            Pango::FontDescription font;
            font.set_family("Monospace");
            font.set_size(8 * Pango::SCALE);
            layout->set_font_description(font);

            int text_w = 0;
            int text_h = 0;
            layout->get_pixel_size(text_w, text_h);

            const double box_w = static_cast<double>(text_w + 6);
            const double box_h = static_cast<double>(text_h + 4);
            const double box_x = std::max(1.0, (static_cast<double>(width) - box_w) * 0.5);
            const double box_y = std::max(1.0, (static_cast<double>(height) - box_h) * 0.5);

            cr->set_source_rgba(1.0, 1.0, 1.0, 0.65);
            cr->rectangle(box_x, box_y, box_w, box_h);
            cr->fill();

            cr->set_source_rgba(1.0, 0.0, 0.0, 1.0);
            cr->move_to(box_x + 3.0, box_y + 2.0);
            layout->show_in_cairo_context(cr);
            cr->restore();
            return false;
        },
        true);

    if (auto* container = dynamic_cast<Gtk::Container*>(widget)) {
        for (auto* child : container->get_children()) {
            install_glade_debug_overlay(child);
        }
    }
}

auto buildMotorColumn = [](PanelPosition pos, bool rightAligned) -> Gtk::Box* {
    auto motors = activeConfig.getMotorsForPanel(pos);
    if (motors.empty()) return Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
 
    std::vector<std::pair<Glib::ustring, CircleDrawingArea**>> items;
    std::vector<std::string> labels;
 
    for (const auto& motor : motors) {
        CircleDrawingArea** circle = getOrCreateCircle(motor.internalLabel);
        items.push_back({motor.displayName, circle});
        labels.push_back(motor.internalLabel);
    }

    if (pos == PanelPosition::LOWER_LEFT || pos == PanelPosition::LOWER_RIGHT) {
        return create_lower_motor_column(items, labels, rightAligned);
    }
    return create_motor_column(items, nullptr, labels, rightAligned);
};

/*** Functions that setup the GUI and windows ***/
/*
Switched to using Glade GUI Designer for the GUI design. This
allows us to very quickly move the various items around in the
layout, removing the need to hardcode all the values. 
HOWEVER, this doesn't allow us to customize for the custom 
GUI elements, which means we need to have placeholders for the
various custom elements that are included in the GUI.
*/
void setupGUI(Glib::RefPtr<Gtk::Application> application) {
    window = nullptr;
    ipAddressEntry = nullptr; connectButton = nullptr; connectionStatusLabel = nullptr;
    ipAddressEntry2 = nullptr; connectButton2 = nullptr; connectionStatusLabel2 = nullptr;
    silentRunButton = nullptr; silentRunButton2 = nullptr; addressListBox = nullptr;
    videoConnectButton = nullptr; videoConnectionStatusLabel = nullptr; videoStreamButton = nullptr;
    videoIPAddressEntry = nullptr; videoAddressListBox = nullptr;
    toggleModeButton = nullptr; settingsButton = nullptr;
    sensorBox = nullptr; innerLeftBox = nullptr; innerRightBox = nullptr;
    armPositionPlaceholder = nullptr; bucketTiltPlaceholder = nullptr; rollImagePlaceholder = nullptr;
    armPositionPlaceholder = nullptr;

    initialize_maps(); 

    auto builder = Gtk::Builder::create();
    try {
        builder->add_from_file("../resources/mainLayout.glade");
    }
    catch(const Glib::Error& ex) {
        std::cerr << "CRITICAL: Failed to load mainLayout.glade: " << ex.what() << std::endl;
        exit(1); 
    }

    builder->get_widget("mainWindow", window);
    if (!window) {
        std::cerr << "FATAL: 'mainWindow' ID not found in mainLayout.glade" << std::endl;
        exit(1);
    }
    window->maximize();

    Gtk::Box* topLevelBox = nullptr;
    builder->get_widget("topLevelBox", topLevelBox);
    if (topLevelBox) {
        window->remove();
        
        // Create video widget
        videoArea = Gtk::manage(new VideoWidget());
        videoArea->set_size_request(1600 * GUI_SCALE, 1000 * GUI_SCALE);
        
        // Create overlay with video as background
        Gtk::Overlay* mainOverlay = Gtk::manage(new Gtk::Overlay());
        mainOverlay->add(*videoArea);  // Video as base
        mainOverlay->add_overlay(*topLevelBox);  // UI on top

        // Proximity bar floating on the video feed, left side
        if (!dumpBot) {
            std::cout << "Initializing proximity bar for dump bot." << std::endl;
            proximityBar = Gtk::manage(new ProximityBar());
            proximityBar->set_size_request(100 * GUI_SCALE, 380 * GUI_SCALE);
            proximityBar->set_light_mode(isLightMode);
            proximityBar->set_warning_threshold(0.6);
            proximityBar->set_optimal_threshold(0.9);
            proximityBar->set_max_distance(1.5);
            proximityBar->set_arm_show_threshold(500);
            proximityBar->set_halign(Gtk::ALIGN_START);
            proximityBar->set_valign(Gtk::ALIGN_CENTER);
            proximityBar->set_margin_left(EDGE_PANEL_WIDTH + 50);
            mainOverlay->add_overlay(*proximityBar);
        }

        // Add overlay to window
        window->add(*mainOverlay);
        
        // Add overlay to window
        window->add(*mainOverlay);
    }
    
    // Get topControlsBox and set its CSS name for styling
    builder->get_widget("topControlsBox", topControlsBox);
    if (topControlsBox) {
        topControlsBox->set_name("topControlsBox");

        batteryBar = Gtk::manage(new BatteryBar());
        batteryBar->set_size_request(-1, 28);
        batteryBar->set_hexpand(true);
        batteryBar->set_warning_voltage(LOW_VOLTAGE);
        batteryBar->set_critical_voltage(LOW_VOLTAGE - 1.0f);
        batteryBar->set_light_mode(isLightMode);
        topControlsBox->pack_end(*batteryBar, Gtk::PACK_SHRINK);
    }

    try {window->set_icon_from_file("../resources/razorbotz.png"); } catch (...) {}

    window->add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    window->signal_key_press_event().connect(sigc::ptr_fun(&on_key_press_event));
    window->signal_key_release_event().connect(sigc::ptr_fun(&on_key_release_event));

    builder->get_widget("list_robot_address", addressListBox);
    builder->get_widget("entry_robot_ip", ipAddressEntry);
    builder->get_widget("btn_robot_connect", connectButton);
    builder->get_widget("lbl_robot_status", connectionStatusLabel);
    builder->get_widget("btn_silent_run", silentRunButton);
    builder->get_widget("entry_robot_ip2", ipAddressEntry2);
    builder->get_widget("btn_robot_connect2", connectButton2);
    builder->get_widget("lbl_robot_status2", connectionStatusLabel2);
    builder->get_widget("btn_silent_run2", silentRunButton2);
    
    builder->get_widget("list_video_address", videoAddressListBox);
    builder->get_widget("entry_video_ip", videoIPAddressEntry);
    builder->get_widget("btn_video_connect", videoConnectButton);
    builder->get_widget("lbl_video_status", videoConnectionStatusLabel);
    builder->get_widget("btn_video_stream", videoStreamButton);
    
    builder->get_widget("btn_toggle_mode", toggleModeButton);
    builder->get_widget("btn_settings", settingsButton);
    
    sensorBox = Gtk::manage(new Gtk::FlowBox());
    sensorBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    
    if (topLevelBox) {
        topLevelBox->pack_end(*sensorBox, Gtk::PACK_SHRINK);
    }

    if(ipAddressEntry) ipAddressEntry->set_text(ORIN_IP);
    if(ipAddressEntry2) ipAddressEntry2->set_text(NANO_IP);
    if(videoIPAddressEntry) videoIPAddressEntry->set_text(ORIN_IP);

    if(connectionStatusLabel) {
        Gdk::RGBA red;
        red.set_rgba(1.0, 0, 0, 1.0);
        connectionStatusLabel->override_background_color(red);
        connectionStatusLabel->set_text("Not Connected");
    }
    if(connectionStatusLabel2) {
        Gdk::RGBA red;
        red.set_rgba(1.0, 0, 0, 1.0);
        connectionStatusLabel2->override_background_color(red);
        connectionStatusLabel2->set_text("Not Connected");
    }
    if(videoConnectionStatusLabel) {
        Gdk::RGBA red;
        red.set_rgba(1.0, 0, 0, 1.0);
        videoConnectionStatusLabel->override_background_color(red);
        videoConnectionStatusLabel->set_text("Not Connected");
    }
    
    if (settingsButton) {
        try {
            auto pixbuf = Gdk::Pixbuf::create_from_file("../resources/SettingsIcon.png");
            auto scaled = pixbuf->scale_simple(24, 24, Gdk::INTERP_BILINEAR);
            auto image = Gtk::manage(new Gtk::Image(scaled));
            settingsButton->set_image(*image);
        } catch (...) {}
    }

    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(generateLightModeString(lightBackgroundColorCSS));
    Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(), css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    try {
        auto font_provider = Gtk::CssProvider::create();
        font_provider->load_from_data("* { font-family: 'Proxima Nova'; }");
        Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(), font_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } catch (...) {}

    server_ui.connectButton = connectButton;
    server_ui.connectionStatusLabel = connectionStatusLabel;
    server_ui.silentRunButton = silentRunButton;
    server_ui.ipAddressEntry = ipAddressEntry;
    server_ui.connectButton2 = connectButton2;
    server_ui.connectionStatusLabel2 = connectionStatusLabel2;
    server_ui.silentRunButton2 = silentRunButton2;
    server_ui.ipAddressEntry2 = ipAddressEntry2;
    server_ui.parentWindow = window;

    video_server_ui.connectButton = videoConnectButton;
    video_server_ui.connectionStatusLabel = videoConnectionStatusLabel;
    video_server_ui.streamButton = videoStreamButton;
    video_server_ui.ipAddressEntry = videoIPAddressEntry;
    video_server_ui.addressListBox = videoAddressListBox;
    video_server_ui.parentWindow = window;

    if (connectButton) connectButton->signal_clicked().connect([&](){ connectOrDisconnect(server_ui, true, connection_finished_dispatcher); });
    if (silentRunButton) silentRunButton->signal_clicked().connect([&](){ silentRun(server_ui); });
    if (connectButton2) connectButton2->signal_clicked().connect([&](){ connectOrDisconnect2(server_ui, false, connection_finished_dispatcher2); });
    if (silentRunButton2) silentRunButton2->signal_clicked().connect([&](){ silentRun2(server_ui); });
    if (addressListBox) addressListBox->signal_row_activated().connect([&](Gtk::ListBoxRow* row){ rowActivated(row, server_ui); });
    if (toggleModeButton) toggleModeButton->signal_clicked().connect(sigc::ptr_fun(&toggleMode));
    if (settingsButton) settingsButton->signal_clicked().connect([&](){ if(allowConfig) create_config_editor_window(get_configFile()); });
    if (videoConnectButton) videoConnectButton->signal_clicked().connect([&](){ videoConnectOrDisconnect(video_server_ui, video_connection_finished_dispatcher); });
    if (videoStreamButton) videoStreamButton->signal_clicked().connect([&](){ videoStream(video_server_ui); });
    if (videoAddressListBox) videoAddressListBox->signal_row_activated().connect([&](Gtk::ListBoxRow* row){ videoRowActivated(row, video_server_ui); });

    if(connectButton) {
        connectButton->set_can_focus(false);
        connectButton->set_focus_on_click(false);
    }
    if(connectButton2){
        connectButton2->set_can_focus(false);
        connectButton2->set_focus_on_click(false);
    }
    if(silentRunButton) {
        silentRunButton->set_can_focus(false);
        silentRunButton->set_focus_on_click(false);
    }
    
    if(videoConnectButton) {
        videoConnectButton->set_can_focus(false);
        videoConnectButton->set_focus_on_click(false);
    }

    if (!noVideo) {
        sensorBox->set_visible(false);

        Gtk::Box* bottomInnerBox = nullptr;
        builder->get_widget("box_bottom_inner", bottomInnerBox); 
        
        builder->get_widget("box_bottom_lower", bottomLowerBox);

        Gtk::Box* leftEdgePanel = nullptr;
        builder->get_widget("left_edge_panel", leftEdgePanel);
        applyEdgePanelStyle(leftEdgePanel);
        if (leftEdgePanel) {
            // width_request in Glade is a minimum; pin panel width at runtime to prevent expansion.
            leftEdgePanel->set_size_request(EDGE_PANEL_WIDTH, -1);
            leftEdgePanel->set_hexpand(false);
            leftEdgePanel->set_halign(Gtk::ALIGN_START);
        }

        Gtk::Box* rightEdgePanel = nullptr;
        builder->get_widget("right_edge_panel", rightEdgePanel);
        applyEdgePanelStyle(rightEdgePanel);
        if (rightEdgePanel) {
            // width_request in Glade is a minimum; pin panel width at runtime to prevent expansion.
            rightEdgePanel->set_size_request(EDGE_PANEL_WIDTH, -1);
            rightEdgePanel->set_hexpand(false);
            rightEdgePanel->set_halign(Gtk::ALIGN_END);
        }

        Gtk::Box* pLeft = nullptr; builder->get_widget("placeholder_inner_left", pLeft);
        if (pLeft) {
            pLeft->set_size_request(EDGE_PANEL_WIDTH, -1);
            std::cout << "Initializing Upper Left column with Falcon indicators by default." << std::endl;
            innerLeftBox = buildMotorColumn(PanelPosition::UPPER_LEFT, true);
            pLeft->add(*innerLeftBox);
            innerLeftBox->set_size_request(200, -1);
            innerLeftBox->set_valign(Gtk::ALIGN_START);
        }

        Gtk::Box* pLeftImages = nullptr; builder->get_widget("placeholder_left_images", pLeftImages);
        if (pLeftImages) {
            auto* leftPanelBottomRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
            leftPanelBottomRow->set_halign(Gtk::ALIGN_CENTER);
            leftPanelBottomRow->set_valign(Gtk::ALIGN_END);
            leftPanelBottomRow->set_hexpand(true);
            leftPanelBottomRow->set_vexpand(false);

            armPositionPlaceholder = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
            armPositionPlaceholder->set_hexpand(true);
            armPositionPlaceholder->set_halign(Gtk::ALIGN_CENTER);
            armPositionPlaceholder->set_valign(Gtk::ALIGN_END);

            bucketTiltPlaceholder = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
            bucketTiltPlaceholder->set_hexpand(true);
            bucketTiltPlaceholder->set_halign(Gtk::ALIGN_CENTER);
            bucketTiltPlaceholder->set_valign(Gtk::ALIGN_END);

            rollImagePlaceholder = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
            rollImagePlaceholder->set_hexpand(true);
            rollImagePlaceholder->set_halign(Gtk::ALIGN_CENTER);
            rollImagePlaceholder->set_valign(Gtk::ALIGN_END);

            leftPanelBottomRow->pack_start(*armPositionPlaceholder, Gtk::PACK_SHRINK);
            leftPanelBottomRow->pack_start(*bucketTiltPlaceholder, Gtk::PACK_SHRINK);
            leftPanelBottomRow->pack_start(*rollImagePlaceholder, Gtk::PACK_SHRINK);
            pLeftImages->pack_end(*leftPanelBottomRow, Gtk::PACK_SHRINK);
        }
        initArmPos();
        initBucketRot();

        Gtk::Box* pRight = nullptr; builder->get_widget("placeholder_inner_right", pRight);
        if (pRight) {
            pRight->set_size_request(EDGE_PANEL_WIDTH, -1);
            std::cout << "Initializing Upper Right motor column with Falcon indicators by default." << std::endl;
            innerRightBox = buildMotorColumn(PanelPosition::UPPER_RIGHT, false);
            pRight->add(*innerRightBox);
            if (!backupBot) {
                initBucketPos();
                initBucketElevation();
            }
        }

        Gtk::Box* pLowerLeft = nullptr; builder->get_widget("placeholder_lower_left", pLowerLeft);
        if (pLowerLeft) {
            pLowerLeft->set_size_request(EDGE_PANEL_WIDTH, -1);
            Gtk::Box* lowerLeftBox = buildMotorColumn(PanelPosition::LOWER_LEFT, false);
            if (lowerLeftBox) {
                pLowerLeft->pack_end(*lowerLeftBox, Gtk::PACK_SHRINK);
            }
        }

        Gtk::Box* pLowerRight = nullptr; builder->get_widget("placeholder_lower_right", pLowerRight);
        if (pLowerRight) {
            pLowerRight->set_size_request(EDGE_PANEL_WIDTH, -1);
            std::cout << "Initializing lower motor column with Falcon indicators by default." << std::endl;
            Gtk::Box* lowerRightBox = buildMotorColumn(PanelPosition::LOWER_RIGHT, false);
            if (lowerRightBox) {
                pLowerRight->pack_end(*lowerRightBox, Gtk::PACK_SHRINK);
            }
        }
        
        Gtk::Box* pSpeedLeft = nullptr;
        builder->get_widget("placeholder_speed_left", pSpeedLeft);
        if (pSpeedLeft) {
            std::cout << "Initializing left speedometer." << std::endl;
            leftSpeedometer = Gtk::manage(new Speedometer("Left Speedometer"));
            leftSpeedometer->set_size_request(300 * GUI_SCALE, 175 * GUI_SCALE);
            leftSpeedometer->set_display_speed(displaySpeed);
            leftSpeedometer->set_numbers_inside(numbersInside);
            leftSpeedometer->set_numbers_on_ticks(numberTicks);
            pSpeedLeft->add(*leftSpeedometer);
        }

        Gtk::Box* pSpeedRight = nullptr; builder->get_widget("placeholder_speed_right", pSpeedRight);
        if(pSpeedRight) {
            std::cout << "Initializing right speedometer." << std::endl;
            rightSpeedometer = Gtk::manage(new Speedometer("Right Speedometer"));
            rightSpeedometer->set_size_request(300 * GUI_SCALE, 175 * GUI_SCALE);
            rightSpeedometer->set_display_speed(displaySpeed);
            rightSpeedometer->set_numbers_inside(numbersInside);
            rightSpeedometer->set_numbers_on_ticks(numberTicks);
            pSpeedRight->add(*rightSpeedometer);
        }

        Gtk::Box* pGear = nullptr; builder->get_widget("placeholder_gear_dial", pGear);
        if(pGear) {
            std::cout << "Initializing gear dial." << std::endl;
            gear_dial = create_gear_dial(currentGear, gears, gear_labels);
            pGear->add(*gear_dial);
            //highlight_gear(currentGear, gears, gear_labels, gear_dial);
        }
        
        if (rollImagePlaceholder) {
            std::cout << "Initializing combined attitude indicator (roll + pitch)." << std::endl;
            attitudeIndicator = Gtk::manage(new ArtificialHorizon("Attitude"));
            attitudeIndicator->set_size_request(ROLL_PITCH_IMAGE_SIZE * GUI_SCALE,
                                                 (ROLL_PITCH_IMAGE_SIZE + 30) * GUI_SCALE);
            attitudeIndicator->set_warning_angles(30.0, -30.0);
            attitudeIndicator->set_light_mode(isLightMode);
            attitudeIndicator->set_halign(Gtk::ALIGN_CENTER);
            attitudeIndicator->set_valign(Gtk::ALIGN_END);
            rollImagePlaceholder->add(*attitudeIndicator);
            roll_init = true;
            pitch_init = true;
        }

        Gdk::RGBA background; background.set(lightBackgroundColor);
        setBackgroundColors(background);

    }
    else {
        Gtk::Box* boxMainContent = nullptr;
        builder->get_widget("box_main_content", boxMainContent);
        if(boxMainContent) boxMainContent->set_visible(false);
        sensorBox->set_visible(true);
        initRoll();
        initPitch();
    }

    if (window) {
        if (debugGladeBounds) {
            std::cout << "Debug mode: drawing Glade widget bounds/IDs." << std::endl;
            install_glade_debug_overlay(window);
            window->queue_draw();
        }

        window->signal_delete_event().connect(sigc::ptr_fun(quit));
        window->show_all();
    }
}

void initSensorsWindow() {
    sensorsWindow = nullptr;

    auto builder = Gtk::Builder::create();
    try {
        builder->add_from_file("../resources/sensorsLayout.glade");
    } catch(const Glib::Error& ex) {
        std::cerr << "Error loading sensors.glade: " << ex.what() << std::endl;
        return;
    }

    builder->get_widget("sensorsWindow", sensorsWindow);
    if (!sensorsWindow) {
        std::cerr << "Error: 'sensorsWindow' ID not found in XML." << std::endl;
        return;
    }

    auto sensors_css = Gtk::CssProvider::create();
    std::string sensors_bg_css = "window { background-color: " + (isLightMode ? lightBackgroundColor : darkBackgroundColor) + "; }";
    sensors_css->load_from_data(sensors_bg_css);
    sensorsWindow->get_style_context()->add_provider(sensors_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    if (monitor_count == 3) {
        auto display = Gdk::Display::get_default();
        if (display) {
            auto third_monitor = display->get_monitor(2);
            if (third_monitor) {
                Gdk::Rectangle geo;
                third_monitor->get_geometry(geo);
                sensorsWindow->set_default_size(geo.get_width(), geo.get_height());
                sensorsWindow->move(geo.get_x(), geo.get_y());
            }
        }
    }
    else {
        sensorsWindow->maximize();
    }

    std::vector<std::string> talonNames = {"Talon 1", "Talon 2", "Talon 3", "Talon 4"};
    std::vector<std::string> falconNames = {"Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4"};
    std::vector<std::string> linearNames = {"Linear 1", "Linear 2"};

    auto inject = [&](const char* id, Gtk::Widget* widget) {
        Gtk::Box* holder = nullptr;
        builder->get_widget(id, holder);
        if (holder && widget) {
            holder->add(*widget);
        }
    };

    talonVoltageGraph = Gtk::manage(new MultiMotorGraph("Talon Bus Voltage", MultiMotorGraph::VOLTAGE, talonNames));
    inject("holder_talon_volt", talonVoltageGraph);

    talonCurrentGraph = Gtk::manage(new MultiMotorGraph("Talon Output Current", MultiMotorGraph::CURRENT, talonNames));
    inject("holder_talon_curr", talonCurrentGraph);

    talonPositionGraph = Gtk::manage(new MultiMotorGraph("Talon Sensor Position", MultiMotorGraph::POSITION, talonNames));
    inject("holder_talon_pos", talonPositionGraph);

    talonOutputGraph = Gtk::manage(new MultiMotorGraph("Talon Output Percentage", MultiMotorGraph::OUTPUT_PERCENT, talonNames));
    inject("holder_talon_out", talonOutputGraph);

    falconVoltageGraph = Gtk::manage(new MultiMotorGraph("Falcon Bus Voltage", MultiMotorGraph::VOLTAGE, falconNames));
    inject("holder_falcon_volt", falconVoltageGraph);

    falconCurrentGraph = Gtk::manage(new MultiMotorGraph("Falcon Output Current", MultiMotorGraph::CURRENT, falconNames));
    inject("holder_falcon_curr", falconCurrentGraph);

    falconPositionGraph = Gtk::manage(new MultiMotorGraph("Falcon Sensor Position", MultiMotorGraph::POSITION, falconNames));
    inject("holder_falcon_pos", falconPositionGraph);

    falconOutputGraph = Gtk::manage(new MultiMotorGraph("Falcon Output Percentage", MultiMotorGraph::OUTPUT_PERCENT, falconNames));
    inject("holder_falcon_out", falconOutputGraph);

    linearSpeedGraph = Gtk::manage(new MultiMotorGraph("Linear Actuator Speed", MultiMotorGraph::SPEED, linearNames));
    inject("holder_linear_speed", linearSpeedGraph);

    linearPotentiometerGraph = Gtk::manage(new MultiMotorGraph("Linear Actuator Position", MultiMotorGraph::POTENTIOMETER, linearNames));
    inject("holder_linear_pot", linearPotentiometerGraph);

    builder->get_widget("sensorBox", sensorBox);
    if (!sensorBox) {
        sensorBox = Gtk::manage(new Gtk::FlowBox()); 
    }

    sensorsWindow->show_all();
}

void initFoxgloveServer() {
    auto logHandler = [](foxglove::WebSocketLogLevel, char const* msg) {
        std::cout << "Foxglove: " << msg << std::endl;
    };

    foxglove::ServerOptions serverOptions;
    
    foxglove_server = std::make_unique<foxglove::Server<foxglove::WebSocketNoTls>>(
        "Razorbotz_Control", logHandler, serverOptions
    );

    foxglove::ChannelWithoutId tf_chan;
    tf_chan.topic = "/tf";
    tf_chan.encoding = "json";
    tf_chan.schemaName = "foxglove.FrameTransforms"; 
    
    auto tfIds = foxglove_server->addChannels({tf_chan});
    tf_channel = tfIds.front();

    foxglove::ServerHandlers<foxglove::ConnHandle> handlers;
    
    handlers.subscribeHandler = [](foxglove::ChannelId chanId, foxglove::ConnHandle clientHandle) {
        std::cout << "Foxglove client subscribed to channel: " << chanId << std::endl;
    };

    handlers.unsubscribeHandler = [](foxglove::ChannelId chanId, foxglove::ConnHandle clientHandle) {
        std::cout << "Foxglove client unsubscribed from channel: " << chanId << std::endl;
    };

    foxglove_server->setHandlers(std::move(handlers));

    foxglove_server->start("0.0.0.0", 8765);
    std::cout << "Foxglove WebSocket Server started on ws://0.0.0.0:8765" << std::endl;
}

#include <glib.h>
#include <fstream>
#include <vector>

std::string getGLBBase64(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open mesh: " << filepath << std::endl;
        return "";
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<guchar> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        gchar* encoded = g_base64_encode(buffer.data(), size);
        std::string result(encoded);
        g_free(encoded); // Free the glib allocated memory
        return result;
    }
    return "";
}

nlohmann::json euler_to_quat(double roll, double pitch, double yaw) {
    double cy = std::cos(yaw * 0.5);
    double sy = std::sin(yaw * 0.5);
    double cp = std::cos(pitch * 0.5);
    double sp = std::sin(pitch * 0.5);
    double cr = std::cos(roll * 0.5);
    double sr = std::sin(roll * 0.5);

    return {
        {"x", sr * cp * cy - cr * sp * sy},
        {"y", cr * sp * cy + sr * cp * sy},
        {"z", cr * cp * sy - sr * sp * cy},
        {"w", cr * cp * cy + sr * sp * sy}
    };
}

void publishRobotTransform() {
    if (!foxglove_server) return;

    auto now = std::chrono::system_clock::now();
    uint64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    uint32_t sec = timestamp_ns / 1000000000;
    uint32_t nsec = timestamp_ns % 1000000000;

    // --- SENSOR OFFSET MATH ---
    const double OFFSET_X = 0.762639;
    const double OFFSET_Y = -0.100614;

    double cos_yaw = std::cos(robot_pitch_rad);
    double sin_yaw = std::sin(robot_pitch_rad);

    // Subtract the rotated offset from the camera's world position 
    // to find the true center of the chassis
    double true_base_x = robot_x_m - (OFFSET_X * cos_yaw - OFFSET_Y * sin_yaw);
    double true_base_y = robot_y_m - (OFFSET_X * sin_yaw + OFFSET_Y * cos_yaw);

    // --- SIMULATED WHEEL SPIN MATH ---
    static double prev_x = true_base_x;
    static double prev_y = true_base_y;
    static double global_wheel_angle_rad = 0.0;

    double dx = true_base_x - prev_x;
    double dy = true_base_y - prev_y;
    double distance = std::sqrt(dx*dx + dy*dy);

    if (distance > 0.001) {
        double movement_angle = std::atan2(dy, dx);
        double angle_diff = movement_angle - robot_pitch_rad;
        
        // Normalize angle difference
        while (angle_diff > M_PI) angle_diff -= 2.0 * M_PI;
        while (angle_diff < -M_PI) angle_diff += 2.0 * M_PI;

        if (std::abs(angle_diff) > M_PI / 2.0) {
            distance = -distance; 
        }

        global_wheel_angle_rad += (distance / 0.210439);
        
        prev_x = true_base_x;
        prev_y = true_base_y;
    }

    // --------------------------

    double safe_arm_deg = arm_angle_deg;
    if (safe_arm_deg < -40.1) safe_arm_deg = -40.1;
    if (safe_arm_deg > 17.1) safe_arm_deg = 17.1;
    
    double arm_pitch_rad = safe_arm_deg * (M_PI / 180.0);

    nlohmann::json tf_update;
    tf_update["transforms"] = nlohmann::json::array();
    
    nlohmann::json transform;
    transform["timestamp"]["sec"] = sec;
    transform["timestamp"]["nsec"] = nsec;
    transform["parent_frame_id"] = "world";
    transform["child_frame_id"] = "base_link";
    
    transform["transform"]["translation"]["x"] = true_base_x;
    transform["transform"]["translation"]["y"] = true_base_y;
    transform["transform"]["translation"]["z"] = 0.0; 
    
    transform["transform"]["rotation"] = euler_to_quat(0.0, 0.0, robot_pitch_rad);
    tf_update["transforms"].push_back(transform);

    nlohmann::json arm_tf;
    arm_tf["timestamp"]["sec"] = sec;
    arm_tf["timestamp"]["nsec"] = nsec;
    arm_tf["parent_frame_id"] = "base_link";
    arm_tf["child_frame_id"] = "Arm";
    
    arm_tf["transform"]["translation"]["x"] = 0.28468;
    arm_tf["transform"]["translation"]["y"] = -0.22263;
    arm_tf["transform"]["translation"]["z"] = 0.30621; 
    
    arm_tf["transform"]["rotation"] = euler_to_quat(0.0, arm_pitch_rad, 0.0);
    
    tf_update["transforms"].push_back(arm_tf);

    double safe_bucket_deg = bucket_angle_deg;
    if (safe_bucket_deg < -25.8) safe_bucket_deg = -25.8;
    if (safe_bucket_deg > 71.6) safe_bucket_deg = 71.6;
    
    // Convert to radians for Foxglove
    double bucket_pitch_rad = safe_bucket_deg * (M_PI / 180.0);

    nlohmann::json bucket_tf;
    bucket_tf["timestamp"]["sec"] = sec;
    bucket_tf["timestamp"]["nsec"] = nsec;
    
    bucket_tf["parent_frame_id"] = "Arm";
    bucket_tf["child_frame_id"] = "Bucket";
    
    bucket_tf["transform"]["translation"]["x"] = 0.82651;
    bucket_tf["transform"]["translation"]["y"] = 0.070738;
    bucket_tf["transform"]["translation"]["z"] = -0.052110; 
    
    bucket_tf["transform"]["rotation"] = euler_to_quat(0.0, bucket_pitch_rad, 0.0);
    tf_update["transforms"].push_back(bucket_tf);

    // Front Left
    nlohmann::json fl_tf;
    fl_tf["timestamp"]["sec"] = sec; fl_tf["timestamp"]["nsec"] = nsec;
    fl_tf["parent_frame_id"] = "base_link"; fl_tf["child_frame_id"] = "FL_Wheel";
    fl_tf["transform"]["translation"]["x"] = 0.8411;
    fl_tf["transform"]["translation"]["y"] = -0.019814;
    fl_tf["transform"]["translation"]["z"] = 0.235883; 
    fl_tf["transform"]["rotation"] = euler_to_quat(0.0, global_wheel_angle_rad, 0.0);
    tf_update["transforms"].push_back(fl_tf);

    // Front Right
    nlohmann::json fr_tf;
    fr_tf["timestamp"]["sec"] = sec; fr_tf["timestamp"]["nsec"] = nsec;
    fr_tf["parent_frame_id"] = "base_link"; fr_tf["child_frame_id"] = "FR_Wheel";
    fr_tf["transform"]["translation"]["x"] = 0.8411;
    fr_tf["transform"]["translation"]["y"] = -0.538167;
    fr_tf["transform"]["translation"]["z"] = 0.235883; 
    fr_tf["transform"]["rotation"] = euler_to_quat(0.0, global_wheel_angle_rad, 0.0);
    tf_update["transforms"].push_back(fr_tf);

    // Back Left
    nlohmann::json bl_tf;
    bl_tf["timestamp"]["sec"] = sec; bl_tf["timestamp"]["nsec"] = nsec;
    bl_tf["parent_frame_id"] = "base_link"; bl_tf["child_frame_id"] = "BL_Wheel";
    bl_tf["transform"]["translation"]["x"] = 0.18387;
    bl_tf["transform"]["translation"]["y"] = 0.0;
    bl_tf["transform"]["translation"]["z"] = 0.23588; 
    bl_tf["transform"]["rotation"] = euler_to_quat(0.0, global_wheel_angle_rad, 0.0);
    tf_update["transforms"].push_back(bl_tf);

    // Back Right
    nlohmann::json br_tf;
    br_tf["timestamp"]["sec"] = sec; br_tf["timestamp"]["nsec"] = nsec;
    br_tf["parent_frame_id"] = "base_link"; br_tf["child_frame_id"] = "BR_Wheel";
    br_tf["transform"]["translation"]["x"] = 0.183875;
    br_tf["transform"]["translation"]["y"] = -0.475667;
    br_tf["transform"]["translation"]["z"] = 0.235883; 
    br_tf["transform"]["rotation"] = euler_to_quat(0.0, global_wheel_angle_rad, -3.1415);
    tf_update["transforms"].push_back(br_tf);

    std::string json_str = tf_update.dump();
    foxglove_server->broadcastMessage(
        tf_channel, 
        timestamp_ns, 
        reinterpret_cast<const uint8_t*>(json_str.data()), 
        json_str.size()
    );
}

void clear_sim_inputs() {
    auto children = simContentBox->get_children();
    for (auto* child : children) {
        simContentBox->remove(*child);
        delete child;
    }
    activeSimWidgets.clear();
    activeSimTypes.clear();
}

void on_sim_type_changed() {
    if (!simTypeCombo || !simContentBox) return;
    
    std::string label = simTypeCombo->get_active_text();
    if (label.empty()) return;

    clear_sim_inputs();

    BinaryMessage dummy(label);
    std::string prefix;
    if (label.find("Talon") != std::string::npos) prefix = "TALON";
    else if (label.find("Falcon") != std::string::npos) prefix = "FALCON";
    else if (label.find("Kraken") != std::string::npos) prefix = "KRAKEN";
    else if (label.find("Neo") != std::string::npos) prefix = "NEO";
    else if (label.find("Linear") != std::string::npos) prefix = "LINEAR";
    else if (label == "Zed") prefix = "ZED";
    else if (label == "Power") prefix = "POWER";
    else if (label == "Power2") prefix = "POWER2";
    else if (label == "Drivetrain") prefix = "DRIVETRAIN";
    else if (label == "Autonomy") prefix = "AUTONOMY";
    else if (label == "Lidar") prefix = "LIDAR";
    else prefix = "COMMUNICATION";

    populateBinaryMessage(label, prefix, dummy);

    for (const auto& element : dummy.getObject().elementList) {
        std::string key = element.label;
        uint8_t type = element.type;
        activeSimTypes[key] = type;

        auto row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
        row->set_margin_bottom(5);
        
        auto lbl = Gtk::manage(new Gtk::Label(key + ":"));
        lbl->set_size_request(120, -1);
        lbl->set_xalign(0.0);
        row->add(*lbl);

        Gtk::Widget* inputWidget = nullptr;

        if (type == TYPE::BOOLEAN) {
            auto check = Gtk::manage(new Gtk::CheckButton());
            check->set_active(element.data.front().boolean);
            inputWidget = check;
        } 
        else if (type == TYPE::STRING) {
            auto entry = Gtk::manage(new Gtk::Entry());
            std::string text;
            for (const auto& c : element.data) text += c.character;
            entry->set_text(text);
            inputWidget = entry;
        } 
        else {
            auto spin = Gtk::manage(new Gtk::SpinButton());
            spin->set_range(-100000.0, 100000.0);
            
            if (type == TYPE::FLOAT32 || type == TYPE::FLOAT64) {
                spin->set_digits(4);
                spin->set_increments(0.1, 1.0);
                float val = (type == TYPE::FLOAT32) ? element.data.front().float32 : (float)element.data.front().float64;
                spin->set_value(val);
            }
            else {
                spin->set_digits(0);
                spin->set_increments(1, 10);
                
                int val = 0;
                if (type == TYPE::UINT16) val = element.data.front().uint16;
                else if (type == TYPE::INT32) val = element.data.front().int32;
                else if (type == TYPE::INT8) val = element.data.front().int8;
                else if (type == TYPE::UINT8) val = element.data.front().uint8;
                
                spin->set_value(val);
            }
            inputWidget = spin;
        }

        row->add(*inputWidget);
        activeSimWidgets[key] = inputWidget;
        simContentBox->add(*row);
    }
    
    simContentBox->show_all();
}

void on_simulate_send() {
    if (!simTypeCombo) return;
    std::string label = simTypeCombo->get_active_text();
    if (label.empty()) return;

    BinaryMessage message(label);
    
    for (auto const& [key, widget] : activeSimWidgets) {
        uint8_t type = activeSimTypes[key];

        if (type == TYPE::BOOLEAN) {
            Gtk::CheckButton* check = dynamic_cast<Gtk::CheckButton*>(widget);
            if(check) message.addElementBoolean(key, check->get_active());
        } 
        else if (type == TYPE::STRING) {
            Gtk::Entry* entry = dynamic_cast<Gtk::Entry*>(widget);
            if(entry) message.addElementString(key, entry->get_text());
        } 
        else if (type == TYPE::FLOAT32) {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(spin) message.addElementFloat32(key, (float)spin->get_value());
        }
        else if (type == TYPE::FLOAT64) {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(spin) message.addElementFloat64(key, (double)spin->get_value());
        }
        else if (type == TYPE::UINT16) {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(key == "Bus Voltage" || key == "Output Current"){
                if(spin) message.addElementUInt16(key, (uint16_t)(spin->get_value_as_int() * 100.0));
            }
            else{
                if(spin) message.addElementUInt16(key, (uint16_t)spin->get_value_as_int());
            }
        }
        else if (type == TYPE::INT32) {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(spin) message.addElementInt32(key, (int32_t)spin->get_value_as_int());
        }
        else if (type == TYPE::UINT8) {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(spin) message.addElementUInt8(key, (uint8_t)spin->get_value_as_int());
        }
        else if (type == TYPE::INT8) {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(spin) message.addElementInt8(key, (int8_t)spin->get_value_as_int());
        }
        else {
            Gtk::SpinButton* spin = dynamic_cast<Gtk::SpinButton*>(widget);
            if(spin) message.addElementInt32(key, (int)spin->get_value_as_int());
        }
    }

    updateGUI(message);
}


static void rebuild_encode_output();
static void on_encode_type_changed();
void initEncodeToolWindow();


// ---------------- Encode Tool Helpers ----------------

static void build_label_to_field_map_once() {
    if (!LABEL_TO_FIELD.empty()) return;
    // Build a reverse lookup using BinaryMessage::decodeFieldValue
    BinaryMessage tmp("tmp");
    for (int i = 1; i < 64; ++i) {
        const Field_Strings f = (Field_Strings)i;
        const std::string label = tmp.decodeFieldValue(f);
        if (!label.empty() && label != "Unknown") {
            LABEL_TO_FIELD[label] = f;
        }
    }
}

static std::string hexDump(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        oss << std::setw(2) << (int)bytes[i];
        if ((i + 1) % 16 == 0) oss << "\n";
        else oss << " ";
    }
    return oss.str();
}

static void set_textview(Gtk::TextView* tv, const std::string& s) {
    if (!tv) return;
    auto buf = tv->get_buffer();
    if (!buf) return;
    buf->set_text(s);
}

// communication_node-style checksum: append placeholder then compute sum%0x2C and write last byte
static void checksum_encode_vec(std::vector<uint8_t>& bytes) {
    uint32_t sum = 0;
    bytes.push_back(0x00);
    for (uint8_t b : bytes) sum += b;
    bytes.back() = (uint8_t)(sum % 0x2C);
}

// Envelope: [flag][orig_size_be32][zlib_compressed...] where flag=1, or flag=0 + raw if no compression.
// We'll mimic the same approach as your communication_node: always compress when envelope enabled.
static std::vector<uint8_t> apply_server_envelope(const std::vector<uint8_t>& raw) {
    uLongf compressed_buffer_size = compressBound((uLong)raw.size());
    std::vector<uint8_t> compressed_bytes(compressed_buffer_size);

    int rc = compress2(
        compressed_bytes.data(), &compressed_buffer_size,
        raw.data(), (uLong)raw.size(),
        Z_BEST_SPEED
    );

    if (rc != Z_OK) {
        // fall back to uncompressed envelope
        std::vector<uint8_t> payload;
        payload.reserve(1 + raw.size());
        payload.push_back(0);
        payload.insert(payload.end(), raw.begin(), raw.end());
        return payload;
    }

    compressed_bytes.resize(compressed_buffer_size);

    std::vector<uint8_t> payload;
    payload.reserve(1 + 4 + compressed_bytes.size());
    payload.push_back(1);

    const uint32_t orig = (uint32_t)raw.size();
    payload.push_back((orig >> 24) & 0xFF);
    payload.push_back((orig >> 16) & 0xFF);
    payload.push_back((orig >>  8) & 0xFF);
    payload.push_back((orig >>  0) & 0xFF);

    payload.insert(payload.end(), compressed_bytes.begin(), compressed_bytes.end());
    return payload;
}

static bool unwrap_server_envelope_vec(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out_raw) {
    if (payload.empty()) return false;
    const uint8_t flag = payload[0];
    if (flag == 0) {
        out_raw.assign(payload.begin() + 1, payload.end());
        return true;
    }
    if (flag != 1) return false;
    if (payload.size() < 1 + 4) return false;

    const uint32_t orig_size =
        (uint32_t(payload[1]) << 24) |
        (uint32_t(payload[2]) << 16) |
        (uint32_t(payload[3]) << 8)  |
        (uint32_t(payload[4]) << 0);

    const uint8_t* comp = payload.data() + 5;
    const size_t comp_len = payload.size() - 5;

    out_raw.resize(orig_size);
    uLongf dest_len = (uLongf)orig_size;
    const int rc = uncompress(out_raw.data(), &dest_len, comp, (uLong)comp_len);
    if (rc != Z_OK || dest_len != (uLongf)orig_size) return false;
    return true;
}

static bool validate_and_strip_checksum(std::vector<uint8_t>& bytes) {
    if (bytes.size() < 2) return false;
    const uint8_t stored = bytes.back();
    bytes.back() = 0x00;
    uint32_t sum = 0;
    for (uint8_t b : bytes) sum += b;
    const uint8_t computed = (uint8_t)(sum % 0x2C);
    bytes.back() = stored;
    if (computed != stored) return false;
    bytes.pop_back();
    return true;
}

// Add element helpers (reads encodeWidgets values)
static void add_value_string_label(BinaryMessage& msg, const std::string& key, uint8_t type) {
    auto it = encodeWidgets.find(key);
    Gtk::Widget* w = (it == encodeWidgets.end()) ? nullptr : it->second;

    switch (type) {
        case TYPE::BOOLEAN: {
            bool v = false;
            if (auto* cb = dynamic_cast<Gtk::CheckButton*>(w)) v = cb->get_active();
            msg.addElementBoolean(key, v);
            break;
        }
        case TYPE::INT32: {
            int v = 0;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = std::stoi(e->get_text());
            msg.addElementInt32(key, v);
            break;
        }
        case TYPE::FLOAT64: {
            double v = 0.0;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = std::stod(e->get_text());
            msg.addElementFloat64(key, v);
            break;
        }
        case TYPE::STRING: {
            std::string v;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = e->get_text();
            msg.addElementString(key, v);
            break;
        }
        default: {
            // Fallback: treat entry text as string
            std::string v;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = e->get_text();
            msg.addElementString(key, v);
            break;
        }
    }
}

static void add_value_field_label(BinaryMessage& msg, const std::string& key, uint8_t type) {
    build_label_to_field_map_once();
    auto fIt = LABEL_TO_FIELD.find(key);
    if (fIt == LABEL_TO_FIELD.end()) {
        // Unknown label -> send as string label
        add_value_string_label(msg, key, type);
        return;
    }
    const Field_Strings field = fIt->second;

    auto it = encodeWidgets.find(key);
    Gtk::Widget* w = (it == encodeWidgets.end()) ? nullptr : it->second;

    switch (type) {
        case TYPE::BOOLEAN: {
            bool v = false;
            if (auto* cb = dynamic_cast<Gtk::CheckButton*>(w)) v = cb->get_active();
            msg.addElementBoolean(field, v);
            break;
        }
        case TYPE::INT32: {
            int v = 0;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = std::stoi(e->get_text());
            msg.addElementInt32(field, v);
            break;
        }
        case TYPE::FLOAT64: {
            double v = 0.0;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = std::stod(e->get_text());
            msg.addElementFloat64(field, v);
            break;
        }
        case TYPE::STRING: {
            std::string v;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = e->get_text();
            msg.addElementString(field, v);
            break;
        }
        default: {
            std::string v;
            if (auto* e = dynamic_cast<Gtk::Entry*>(w)) v = e->get_text();
            msg.addElementString(field, v);
            break;
        }
    }
}

static std::vector<uint8_t> build_variant_bytes(const std::string& name, bool useFieldVariant) {
    BinaryMessage msg(name);

    for (const auto& kv : encodeTypes) {
        const std::string& key = kv.first;
        const uint8_t type = kv.second;

        auto incIt = encodeInclude.find(key);
        if (incIt != encodeInclude.end() && incIt->second && !incIt->second->get_active()) continue;

        if (useFieldVariant) add_value_field_label(msg, key, type);
        else                 add_value_string_label(msg, key, type);
    }

    auto bytesList = msg.getBytes();
    std::vector<uint8_t> raw(bytesList->begin(), bytesList->end());

    if (cbIncludeChecksum && cbIncludeChecksum->get_active()) {
        checksum_encode_vec(raw);
    }
    if (cbApplyEnvelope && cbApplyEnvelope->get_active()) {
        return apply_server_envelope(raw);
    }
    return raw;
}

static std::string element_value_to_string(const Element& e) {
    std::ostringstream oss;
    if (e.data.empty()) return "(empty)";
    switch (e.type) {
        case TYPE::BOOLEAN:  oss << (e.data.front().boolean ? "true" : "false"); break;
        case TYPE::CHARACTER: oss << "'" << e.data.front().character << "'"; break;
        case TYPE::INT8:     oss << (int)e.data.front().int8; break;
        case TYPE::INT16:    oss << e.data.front().int16; break;
        case TYPE::INT32:    oss << e.data.front().int32; break;
        case TYPE::INT64:    oss << e.data.front().int64; break;
        case TYPE::UINT8:    oss << (unsigned)e.data.front().uint8; break;
        case TYPE::UINT16:   oss << e.data.front().uint16; break;
        case TYPE::UINT32:   oss << e.data.front().uint32; break;
        case TYPE::UINT64:   oss << e.data.front().uint64; break;
        case TYPE::FLOAT32:  oss << std::fixed << std::setprecision(4) << e.data.front().float32; break;
        case TYPE::FLOAT64:  oss << std::fixed << std::setprecision(6) << e.data.front().float64; break;
        case TYPE::STRING: {
            std::string s;
            s.reserve(e.data.size());
            for (const auto& d : e.data) s.push_back(d.character);
            oss << "\"" << s << "\"";
            break;
        }
        default:
            oss << "(type " << (int)e.type << ", count " << e.data.size() << ")";
            break;
    }
    if (e.dimensionCount > 0) {
        oss << " dims=" << (size_t)e.dimensionCount << " [";
        for (size_t i = 0; i < e.sizeList.size(); ++i) {
            if (i) oss << ",";
            oss << e.sizeList[i];
        }
        oss << "]";
    }
    return oss.str();
}

static void dump_object_recursive(const Object& obj, std::ostringstream& out, int indent = 0) {
    const std::string pad(indent, ' ');
    out << pad << "Object: " << obj.label << "\n";
    for (const auto& e : obj.elementList) {
        out << pad << "  - " << e.label << " = " << element_value_to_string(e) << "\n";
    }
    // The BinaryMessage Object struct uses `children` (not `objectList`).
    for (const auto& child : obj.children) {
        dump_object_recursive(child, out, indent + 2);
    }
}

static std::string decode_preview_from_payload(const std::vector<uint8_t>& payload,
                                               bool stage_is_enveloped,
                                               bool stage_has_checksum,
                                               bool validate_checksum)
{
    std::vector<uint8_t> raw = payload;

    if (stage_is_enveloped) {
        std::vector<uint8_t> unwrapped;
        if (!unwrap_server_envelope_vec(payload, unwrapped)) {
            return "Decode failed: could not unwrap envelope\n";
        }
        raw.swap(unwrapped);
    }

    if (stage_has_checksum) {
        if (validate_checksum) {
            if (!validate_and_strip_checksum(raw)) {
                return "Decode failed: checksum mismatch\n";
            }
        } else {
            if (!raw.empty()) raw.pop_back();
        }
    }

    try {
        std::list<uint8_t> msgList(raw.begin(), raw.end());
        BinaryMessage decoded(msgList);
        std::ostringstream out;
        dump_object_recursive(decoded.getObject(), out);
        return out.str();
    } catch (const std::exception& e) {
        std::ostringstream out;
        out << "Decode exception: " << e.what() << "\n";
        return out.str();
    } catch (...) {
        return "Decode exception: unknown\n";
    }
}

static void rebuild_encode_output() {
    if (!encodeTypeCombo) return;
    const std::string name = encodeTypeCombo->get_active_text();
    if (name.empty()) return;

    const std::vector<uint8_t> bytesString = build_variant_bytes(name, /*useFieldVariant=*/false);
    const std::vector<uint8_t> bytesField  = build_variant_bytes(name, /*useFieldVariant=*/true);

    set_textview(txtHexStringLabels, hexDump(bytesString));
    set_textview(txtHexFieldLabels,  hexDump(bytesField));

    // Savings summary
    const size_t a = bytesString.size();
    const size_t b = bytesField.size();
    const long saved = (long)a - (long)b;
    double pct = 0.0;
    if (a > 0) pct = 100.0 * ((double)saved / (double)a);

    const double hz = 33.0;
    const double a_kbps = ((double)a * hz * 8.0) / 1000.0;
    const double b_kbps = ((double)b * hz * 8.0) / 1000.0;

    if (encodeSummaryLabel) {
        std::ostringstream oss;
        oss << "String labels: " << a << " B   |   FieldStrings: " << b << " B   |   Saved: "
            << saved << " B (" << std::fixed << std::setprecision(1) << pct << "%)"
            << "   @33Hz: " << std::setprecision(1) << a_kbps << " kbps → " << b_kbps << " kbps";
        encodeSummaryLabel->set_text(oss.str());
    }

    // Decode preview
    if (cbShowDecoded && cbShowDecoded->get_active()) {
        // Decode whatever the tool is currently producing.
        // (This matches what the client would receive on the wire.)
        const bool stage_is_enveloped = (cbApplyEnvelope && cbApplyEnvelope->get_active());
        const bool stage_has_checksum = (cbIncludeChecksum && cbIncludeChecksum->get_active());
        const bool validate_checksum = (cbValidateChecksum && cbValidateChecksum->get_active());

        set_textview(txtDecodedStringLabels,
                     decode_preview_from_payload(bytesString, stage_is_enveloped, stage_has_checksum, validate_checksum));
        set_textview(txtDecodedFieldLabels,
                     decode_preview_from_payload(bytesField, stage_is_enveloped, stage_has_checksum, validate_checksum));
    } else {
        set_textview(txtDecodedStringLabels, "");
        set_textview(txtDecodedFieldLabels, "");
    }
}

static void clear_encode_inputs() {
    if (!encodeContentBox) return;
    auto children = encodeContentBox->get_children();
    for (auto* child : children) {
        encodeContentBox->remove(*child);
        delete child;
    }
    encodeWidgets.clear();
    encodeTypes.clear();
    encodeInclude.clear();
}

static void rebuildEncodeToolFields(const std::string& name, const std::string& prefix) {
    clear_encode_inputs();

    BinaryMessage dummy(name);

    // We reuse your existing simulator UI generator by calling populateBinaryMessage(dummy, prefix)
    // which adds the canonical set of fields for this message type.
    populateBinaryMessage(name, prefix, dummy);

    // The dummy now contains elements; build UI rows from them.
    // We only need label + type; default widget is an Entry for numeric/string, CheckButton for bool.
    for (const auto& el : dummy.getObject().elementList) {
        const std::string key = el.label;
        const uint8_t type = el.type;

        auto row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));

        auto inc = Gtk::manage(new Gtk::CheckButton());
        inc->set_active(true);
        inc->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));
        encodeInclude[key] = inc;
        row->pack_start(*inc, Gtk::PACK_SHRINK);

        auto lbl = Gtk::manage(new Gtk::Label(key));
        lbl->set_xalign(0.0);
        lbl->set_size_request(180, -1);
        row->pack_start(*lbl, Gtk::PACK_SHRINK);

        Gtk::Widget* widget = nullptr;
        if (type == TYPE::BOOLEAN) {
            auto cb = Gtk::manage(new Gtk::CheckButton());
            cb->set_active(false);
            cb->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));
            widget = cb;
        } else {
            auto entry = Gtk::manage(new Gtk::Entry());
            entry->set_text("0");
            entry->signal_changed().connect(sigc::ptr_fun(&rebuild_encode_output));
            widget = entry;
        }

        encodeWidgets[key] = widget;
        encodeTypes[key] = type;
        row->pack_start(*widget, Gtk::PACK_EXPAND_WIDGET);

        encodeContentBox->pack_start(*row, Gtk::PACK_SHRINK);
    }

    encodeContentBox->show_all();
    rebuild_encode_output();
}

static void on_encode_type_changed() {
    if (!encodeTypeCombo || !encodeContentBox) return;
    std::string label = encodeTypeCombo->get_active_text();
    if (label.empty()) return;

    std::string prefix;
    if (label.find("Talon") != std::string::npos) prefix = "TALON";
    else if (label.find("Falcon") != std::string::npos) prefix = "FALCON";
    else if (label.find("Kraken") != std::string::npos) prefix = "KRAKEN";
    else if (label.find("Neo") != std::string::npos) prefix = "NEO";
    else if (label.find("Linear") != std::string::npos) prefix = "LINEAR";
    else if (label == "Zed") prefix = "ZED";
    else if (label == "Power") prefix = "POWER";
    else if (label == "Communication") prefix = "COMMS";
    else if (label == "Autonomy") prefix = "AUTO";
    else prefix = "GEN";

    rebuildEncodeToolFields(label, prefix);
}

void initEncodeToolWindow() {
    encodeToolWindow = new Gtk::Window();
    encodeToolWindow->set_title("BinaryMessage Encode Tool");
    encodeToolWindow->set_default_size(1100, 800);
    encodeToolWindow->set_keep_above(true);

    auto mainVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10));
    mainVBox->set_border_width(10);

    // Controls
    auto ctrlRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
    cbUseFieldStrings = Gtk::manage(new Gtk::CheckButton("Use FieldStrings labels"));
    cbIncludeChecksum = Gtk::manage(new Gtk::CheckButton("Include checksum"));
    cbApplyEnvelope   = Gtk::manage(new Gtk::CheckButton("Apply server envelope (compression)"));

    cbUseFieldStrings->set_active(true);
    cbIncludeChecksum->set_active(true);
    cbApplyEnvelope->set_active(true);

    ctrlRow->pack_start(*cbUseFieldStrings, Gtk::PACK_SHRINK);
    ctrlRow->pack_start(*cbIncludeChecksum, Gtk::PACK_SHRINK);
    ctrlRow->pack_start(*cbApplyEnvelope, Gtk::PACK_SHRINK);

    cbUseFieldStrings->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));
    cbIncludeChecksum->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));
    cbApplyEnvelope->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));

    mainVBox->pack_start(*ctrlRow, Gtk::PACK_SHRINK);

    encodeSummaryLabel = Gtk::manage(new Gtk::Label(""));
    encodeSummaryLabel->set_xalign(0.0);
    mainVBox->pack_start(*encodeSummaryLabel, Gtk::PACK_SHRINK);

    // Decode preview controls
    auto decodeCtrlRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
    cbShowDecoded = Gtk::manage(new Gtk::CheckButton("Show decoded values (client preview)"));
    cbShowDecoded->set_active(false);
    cbShowDecoded->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));
    decodeCtrlRow->pack_start(*cbShowDecoded, Gtk::PACK_SHRINK);

    decodeStageCombo = Gtk::manage(new Gtk::ComboBoxText());
    decodeStageCombo->append("Raw BinaryMessage (no checksum, no envelope)");
    decodeStageCombo->append("After checksum (raw + checksum)");
    decodeStageCombo->append("After server envelope (compression)");
    decodeStageCombo->set_active(2);
    decodeStageCombo->signal_changed().connect(sigc::ptr_fun(&rebuild_encode_output));
    decodeCtrlRow->pack_start(*Gtk::manage(new Gtk::Label("Decode input:")), Gtk::PACK_SHRINK);
    decodeCtrlRow->pack_start(*decodeStageCombo, Gtk::PACK_SHRINK);

    cbValidateChecksum = Gtk::manage(new Gtk::CheckButton("Validate checksum"));
    cbValidateChecksum->set_active(true);
    cbValidateChecksum->signal_toggled().connect(sigc::ptr_fun(&rebuild_encode_output));
    decodeCtrlRow->pack_start(*cbValidateChecksum, Gtk::PACK_SHRINK);

    mainVBox->pack_start(*decodeCtrlRow, Gtk::PACK_SHRINK);

    // Type dropdown
    mainVBox->pack_start(*Gtk::manage(new Gtk::Label("Select Message Type:")), Gtk::PACK_SHRINK);
    encodeTypeCombo = Gtk::manage(new Gtk::ComboBoxText());
    std::vector<std::string> targets = {
        "Talon 1", "Talon 2", "Talon 3", "Talon 4",
        "Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4",
        "Linear 1", "Linear 2", "Zed", "Drivetrain",
        "Power", "Communication", "Autonomy"
    };
    for (const auto& t : targets) encodeTypeCombo->append(t);
    encodeTypeCombo->signal_changed().connect(sigc::ptr_fun(&on_encode_type_changed));
    encodeTypeCombo->signal_changed().connect(sigc::ptr_fun(&rebuild_encode_output));
    mainVBox->pack_start(*encodeTypeCombo, Gtk::PACK_SHRINK);

    // Scrollable fields
    auto scrolled = Gtk::manage(new Gtk::ScrolledWindow());
    scrolled->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scrolled->set_vexpand(true);
    encodeContentBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    scrolled->add(*encodeContentBox);
    mainVBox->pack_start(*scrolled, Gtk::PACK_EXPAND_WIDGET);

    // Output panes: hex
    auto panes = Gtk::manage(new Gtk::Paned(Gtk::ORIENTATION_HORIZONTAL));
    txtHexStringLabels = Gtk::manage(new Gtk::TextView());
    txtHexFieldLabels  = Gtk::manage(new Gtk::TextView());
    txtHexStringLabels->set_editable(false);
    txtHexFieldLabels->set_editable(false);

    auto leftScroll = Gtk::manage(new Gtk::ScrolledWindow());
    auto rightScroll = Gtk::manage(new Gtk::ScrolledWindow());
    leftScroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    rightScroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    leftScroll->add(*txtHexStringLabels);
    rightScroll->add(*txtHexFieldLabels);

    panes->add1(*leftScroll);
    panes->add2(*rightScroll);
    panes->set_position(550);
    mainVBox->pack_start(*panes, Gtk::PACK_EXPAND_WIDGET);

    // Output panes: decoded
    auto decodedLabel = Gtk::manage(new Gtk::Label("Decoded (String labels)  |  Decoded (FieldStrings labels)"));
    decodedLabel->set_xalign(0.0);
    mainVBox->pack_start(*decodedLabel, Gtk::PACK_SHRINK);

    auto decodedPanes = Gtk::manage(new Gtk::Paned(Gtk::ORIENTATION_HORIZONTAL));
    txtDecodedStringLabels = Gtk::manage(new Gtk::TextView());
    txtDecodedFieldLabels  = Gtk::manage(new Gtk::TextView());
    txtDecodedStringLabels->set_editable(false);
    txtDecodedFieldLabels->set_editable(false);

    auto decLeftScroll = Gtk::manage(new Gtk::ScrolledWindow());
    auto decRightScroll = Gtk::manage(new Gtk::ScrolledWindow());
    decLeftScroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    decRightScroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    decLeftScroll->set_size_request(-1, 220);
    decRightScroll->set_size_request(-1, 220);
    decLeftScroll->add(*txtDecodedStringLabels);
    decRightScroll->add(*txtDecodedFieldLabels);

    decodedPanes->add1(*decLeftScroll);
    decodedPanes->add2(*decRightScroll);
    decodedPanes->set_position(550);
    mainVBox->pack_start(*decodedPanes, Gtk::PACK_SHRINK);

    auto btnRebuild = Gtk::manage(new Gtk::Button("Rebuild Hex Output"));
    btnRebuild->signal_clicked().connect(sigc::ptr_fun(&rebuild_encode_output));
    mainVBox->pack_start(*btnRebuild, Gtk::PACK_SHRINK);

    encodeToolWindow->add(*mainVBox);
    encodeToolWindow->show_all();

    encodeTypeCombo->set_active_text("Talon 1");
    rebuild_encode_output();
}

// ---------------- End Encode Tool Helpers ----------------

void initSimulatorWindow() {
    simulatorWindow = new Gtk::Window();
    simulatorWindow->set_title("Network Simulator");
    simulatorWindow->set_default_size(400, 600);
    simulatorWindow->set_keep_above(true);

    auto mainVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10));
    mainVBox->set_border_width(10);

    mainVBox->add(*Gtk::manage(new Gtk::Label("Select Message Type:")));
    simTypeCombo = Gtk::manage(new Gtk::ComboBoxText());
    
    std::vector<std::string> targets = {
        "Talon 1", "Talon 2", "Talon 3", "Talon 4",
        "Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4",
        "Kraken 1", "Kraken 2", "Kraken 3", "Kraken 4",
        "Neo 1", "Neo 2", "Neo 3", "Neo 4",
        "Linear 1", "Linear 2", "Zed", "Drivetrain", 
        "Power", "Communication", "Autonomy", "Lidar"
    };
    if(primaryBot){
        targets = {
            "Talon 1", "Talon 3",
            "Kraken 1", "Kraken 2", "Kraken 3", "Kraken 4",
            "Linear 1", "Linear 3", "Zed", "Drivetrain", 
            "Power", "Communication", "Autonomy", "Lidar"
        };
    }
    else if(dumpBot){
        targets = {
            "Falcon 1", "Neo 1", "Neo 2", "Neo 3", "Neo 4",
            "Linear 1", "Linear 2", "Zed", "Drivetrain", 
            "Power", "Communication", "Autonomy"
        };
    }
    else if(backupBot){
        targets = {
            "Talon 1", "Talon 2", "Talon 3", "Talon 4",
            "Falcon 1", "Falcon 2", "Falcon 3", "Falcon 4",
            "Linear 1", "Linear 2", "Zed", "Drivetrain", 
            "Power", "Communication", "Autonomy", "Lidar"
        };
    }


    for(const auto& t : targets) simTypeCombo->append(t);
    
    simTypeCombo->signal_changed().connect(sigc::ptr_fun(&on_sim_type_changed));
    mainVBox->add(*simTypeCombo);

    auto scrolled = Gtk::manage(new Gtk::ScrolledWindow());
    scrolled->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scrolled->set_vexpand(true);
    
    simContentBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    scrolled->add(*simContentBox);
    mainVBox->add(*scrolled);

    auto btnSend = Gtk::manage(new Gtk::Button("Send Message"));
    btnSend->signal_clicked().connect(sigc::ptr_fun(&on_simulate_send));
    mainVBox->add(*btnSend);

    simulatorWindow->add(*mainVBox);
    
    // Apply light background color to simulator window
    auto sim_css = Gtk::CssProvider::create();
    std::string sim_bg_css = "window { background-color: " + lightBackgroundColor + "; }";
    sim_css->load_from_data(sim_bg_css);
    simulatorWindow->get_style_context()->add_provider(sim_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    simulatorWindow->show_all();
    
    simTypeCombo->set_active_text("Talon 1");
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
                std::cout << "--backup_bot: Sets the backup bot" << std::endl;
                std::cout << "--dump_bot: Sets the dump bot" << std::endl;
                std::cout << "--debug_glade_bounds: Draws red bounds and Glade IDs on widgets" << std::endl;
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
            else if(!strcmp("--wsl", argv[i])){
                wsl = true;
                ORIN_IP = "127.0.0.1";
                NANO_IP = "127.0.0.2";
                
                std::cout << "WSL Mode: defaulting to Localhost (" << ORIN_IP << ")" << std::endl;
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
            else if(!strcmp("--simulate", argv[i])){
                simulateNetwork = true;
                initVals = true; 
            }
            else if(!strcmp("--encode_tool", argv[i])){
                start_encode_tool = true;
                initVals = true;
            }
            else if(!strcmp("--backup_bot", argv[i])){
                activeConfig = configs::backupBot();
                backupBot = true;
                primaryBot = false;
            }
            else if(!strcmp("--dump_bot", argv[i])){
                activeConfig = configs::dumpBot();
                dumpBot = true;
                primaryBot = false;
            }
            else if(!strcmp("--debug_glade_bounds", argv[i])){
                debugGladeBounds = true;
            }
        }
    }
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
        
        // Calculate scale relative to the target 2560px display
        GUI_SCALE = (double)width / 2560.0;

        if(GUI_SCALE < 0.5) GUI_SCALE = 0.5;
        
        std::cout << "Detected Width: " << width << " | Applying GUI Scale: " << GUI_SCALE << std::endl;

        if(width < 1920){
            smallLaptop = true;
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
    rebuildConfigDerivedGlobals();
    setup_local_key_vectors();
    checkSize();
    setupGUI(application);
    if(!noVideo)
        initSensorsWindow();
    if(simulateNetwork) {
        initSimulatorWindow();
    }
    if(start_encode_tool) {
        initEncodeToolWindow();
    }
    moveWindows();
    initGUI();
    initFoxgloveServer();
    
    //Start a thread to listen to updates from the robot
    videoDisconnectDispatcher.connect([&]() {
        if (shouldVideoDisconnect) {
            handleVideoDisconnect(video_server_ui);
            shouldVideoDisconnect = false;
        }
    });

    connection_finished_dispatcher.connect(sigc::ptr_fun(&on_connection_finished));
    connection_finished_dispatcher2.connect(sigc::ptr_fun(&on_connection2_finished));
    video_connection_finished_dispatcher.connect(sigc::ptr_fun(&on_video_connection_finished));
    
    std::thread broadcastListenThread(broadcastListen);
    broadcastListenThread.detach();

    std::thread videoBroadcastListenThread(videoBroadcastListen);
    videoBroadcastListenThread.detach();

    std::thread videoMainThread(videoMain, std::ref(latestFrame), std::ref(frameMutex), 
        std::ref(newFrameAvailable), std::ref(videoDisconnectDispatcher), 
        std::ref(shouldVideoDisconnect));
    videoMainThread.detach();

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
    else {
        axisEventList = new std::vector<std::vector<AxisEvent*>*>(0);
    }

    SDL_Event event;
    char buffer[16384] = {0}; 
    int bytesRead=0;

    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastTransmitTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastReceiveOrin = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastReceiveNano = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastHeartbeatTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastVideoHeartbeatTime = std::chrono::high_resolution_clock::now();
    now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTransmitTime);
    double deltaTime = time_span.count();
    
    std::list<uint8_t> messageBytesList; //List to store incoming bytes
    uint8_t message[256];
    bool running=true;
    while(running){
        adjustRobotList(addressListBox);
        adjustVideoRobotList(videoAddressListBox);

        while(Gtk::Main::events_pending()){
            Gtk::Main::iteration();
        }

        if (newFrameAvailable) {
            if (videoArea) {
                videoArea->setFrame(latestFrame);
            }
            newFrameAvailable = false;
        }

        now = std::chrono::high_resolution_clock::now();
        time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastFoxgloveTransmit);
        if (time_span.count() > 0.016) {
            lastFoxgloveTransmit = now;
            publishRobotTransform();
        }

        if(!testInput && !isServerInitialized() && !isServerInitialized2() && !isVideoStreamActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::vector<uint8_t> data_buffer;
        bytesRead = receiveRobotData(data_buffer);
        if(isSilentRunning())
            lastReceiveOrin = std::chrono::high_resolution_clock::now();
        else{
            lastReceiveOrin = lastPacketOrinMs();
        }
        if(isSilentRunning2())
            lastReceiveNano = std::chrono::high_resolution_clock::now();
        else{
            lastReceiveNano = lastPacketNanoMs();
        }
        if (bytesRead > 0) {
            std::vector<uint8_t> processed_buffer;
            now = std::chrono::high_resolution_clock::now();
            if (process_payload(data_buffer, processed_buffer)) { 
                for(uint8_t byte : processed_buffer) {
                    messageBytesList.push_back(byte);
                }
            }
        }
        now = std::chrono::high_resolution_clock::now();
        if (isServerConnected()) {
            double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastReceiveOrin).count();
            if (dt > 5.0) {
                std::cout << "Orin connection timed out.\n";
                setDisconnectedState(server_ui);
            }
        }

        if (isServerConnected2()) {
            double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastReceiveNano).count();
            if (dt > 5.0) {
                std::cout << "Nano connection timed out.\n";
                setDisconnectedState2(server_ui);
            }
        }

        if(!isServerConnected() && !isServerConnected2()){
            resetUIOnDisconnect();
        }
        
        while(BinaryMessage::hasMessage(messageBytesList)){
            if (checksum_decode(messageBytesList) == 1) {
                BinaryMessage message(messageBytesList);
                updateGUI(message);
                uint64_t size = BinaryMessage::decodeSizeBytes(messageBytesList);
                for(int count=0; count < size + 1; count++){
                    messageBytesList.pop_front();
                }
            }
            else {
                break; 
            }
        }

        now = std::chrono::high_resolution_clock::now();
        time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastHeartbeatTime);
        deltaTime = time_span.count();
        if(deltaTime > 1.0 && (isServerConnected() || isServerConnected2())){
            lastHeartbeatTime = now;
            sendHeartbeat();
        }

        time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastVideoHeartbeatTime);
        if (time_span.count() > 1.0 && isVideoConnected()) {
            lastVideoHeartbeatTime = now;
            sendVideoHeartbeat();
        }


        /******************************Handle control events******************************/
        while(SDL_PollEvent(&event)){
            switch(event.type){
                case SDL_JOYHATMOTION:{
                    sendJoystickHat(event.jhat.which, event.jhat.hat, event.jhat.value);
                    break;
                }
                case SDL_JOYBUTTONDOWN:{
                    sendJoystickButton(event.jbutton.which, event.jbutton.button, event.jbutton.state);
                    break;
                }
                case SDL_JOYBUTTONUP:{
                    sendJoystickButton(event.jbutton.which, event.jbutton.button, event.jbutton.state);
                    break;
                }
                case SDL_JOYAXISMOTION: {
                    int deadZone=4000;
                    if(event.jaxis.value < -deadZone || deadZone < event.jaxis.value ) {
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->isSet = true;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->value = event.jaxis.value;
                    }
                    else{
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->isSet = true;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->value = 0;
                    }
                    break;
                }
                default:
                    break;
            }
        }
        // Two ways we might be able to decrease bandwidth usage here:
        // 1. Introduce delta threshold and only send values over certain delta
        // 2. Send all axes together, not individually
        now = std::chrono::high_resolution_clock::now();
        time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTransmitTime);
        deltaTime = time_span.count();
        if(deltaTime > 0.05 ){
            lastTransmitTime = std::chrono::high_resolution_clock::now();
            for(int joystickIndex=0; joystickIndex < axisEventList->size(); joystickIndex++){
                for(int axisIndex=0; axisIndex < axisEventList->at(joystickIndex)->size(); axisIndex++){
                    if(axisEventList->at(joystickIndex)->at(axisIndex)->isSet){
                        axisEventList->at(joystickIndex)->at(axisIndex)->isSet = false;
                        float value = ((float)axisEventList->at(joystickIndex)->at(axisIndex)->value) / -32768.0;
                        uint8_t which = joystickIndex;
                        uint8_t axis  = axisIndex;
                        remapJoystickInputs(&which, &axis);
                        sendJoystickAxis(which, axis, value);
                    }
                }
            }
        }
    }
    return 0; 
}