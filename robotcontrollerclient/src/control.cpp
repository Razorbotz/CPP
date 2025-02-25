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

#include <glibmm/ustring.h>
#include <SDL2/SDL.h>
#include <gtkmm.h>
#include <gtkmm/window.h>
//#include <gdk-pixbuf/gdk-pixbuf.h>

#include "InfoFrame.hpp"
#include "BinaryMessage.hpp"

#define PORT 31337 

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
  
Gtk::FlowBox* sensorBox;

Gtk::Window* window;
int sock = 0; 
bool connected=false;
double roll_rotation_angle = 0.0;
Glib::RefPtr<Gdk::Pixbuf> roll_pixbuf;
Gtk::Image* roll_image;

double pitch_rotation_angle = 45.0;
Glib::RefPtr<Gdk::Pixbuf> pitch_pixbuf;
Gtk::Image* pitch_image;

std::vector<InfoFrame*> infoFrameList;

struct AxisEvent{
    bool isSet=false;
    uint8_t which;
    uint8_t axis;  //0-roll 1-pitch 2-throttle 3-yaw
    int value;
};
std::vector<std::vector<AxisEvent*>*>* axisEventList;


Glib::RefPtr<Gdk::Pixbuf> rotate_image(Glib::RefPtr<Gdk::Pixbuf> pixbuf, double angle_deg, int target_width, int target_height) {
    // Convert degrees to radians
    double angle_rad = angle_deg * M_PI / 180.0;

    // Get the original width and height
    int width = pixbuf->get_width();
    int height = pixbuf->get_height();

    // Calculate new width and height after rotation
    int new_width = static_cast<int>(std::abs(width * std::cos(angle_rad)) + std::abs(height * std::sin(angle_rad)));
    int new_height = static_cast<int>(std::abs(width * std::sin(angle_rad)) + std::abs(height * std::cos(angle_rad)));

    // Create a new pixbuf to hold the rotated image
    Glib::RefPtr<Gdk::Pixbuf> rotated_pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, new_width, new_height);

    // Access the raw pixel data of the original image
    unsigned char* pixels = pixbuf->get_pixels();
    int rowstride = pixbuf->get_rowstride();
    int channels = pixbuf->get_n_channels();

    // Access the raw pixel data of the rotated image
    unsigned char* rotated_pixels = rotated_pixbuf->get_pixels();
    int rotated_rowstride = rotated_pixbuf->get_rowstride();

    // Calculate the center of the original image and the rotated image
    double center_x = width / 2.0;
    double center_y = height / 2.0;
    double new_center_x = new_width / 2.0;
    double new_center_y = new_height / 2.0;

    // Loop through the new pixbuf and calculate where each pixel from the original image will go
    for (int y = 0; y < new_height; ++y) {
        for (int x = 0; x < new_width; ++x) {
            // Translate (x, y) to center-based coordinates
            double new_x = x - new_center_x;
            double new_y = y - new_center_y;

            // Apply the rotation matrix (inverse transformation)
            double old_x = new_x * std::cos(angle_rad) + new_y * std::sin(angle_rad) + center_x;
            double old_y = -new_x * std::sin(angle_rad) + new_y * std::cos(angle_rad) + center_y;

            // Check if the old coordinates are within the bounds of the original image
            if (old_x >= 0 && old_x < width && old_y >= 0 && old_y < height) {
                // Compute pixel position in the original image
                int old_pixel_x = static_cast<int>(old_x);
                int old_pixel_y = static_cast<int>(old_y);

                // Get the pixel data from the original image
                unsigned char* original_pixel = pixels + old_pixel_y * rowstride + old_pixel_x * channels;

                // Get the pixel data for the rotated image
                unsigned char* rotated_pixel = rotated_pixels + y * rotated_rowstride + x * channels;

                // Copy the pixel data to the rotated image
                for (int c = 0; c < channels; ++c) {
                    rotated_pixel[c] = original_pixel[c];
                }
            }
            else {
                // Set the background pixel to white (255 for RGB)
                unsigned char* rotated_pixel = rotated_pixels + y * rotated_rowstride + x * channels;
                rotated_pixel[0] = 255;  // Red
                rotated_pixel[1] = 255;  // Green
                rotated_pixel[2] = 255;  // Blue
                if (channels == 4) {
                    rotated_pixel[3] = 255;  // Alpha (fully opaque)
                }
            }
        }
    }

    Glib::RefPtr<Gdk::Pixbuf> resized_pixbuf = rotated_pixbuf->scale_simple(target_width, target_height, Gdk::InterpType::INTERP_BILINEAR);

	unsigned char* new_pixels = resized_pixbuf->get_pixels();
	int new_rowstride = resized_pixbuf->get_rowstride();
	int new_channels = resized_pixbuf->get_n_channels();
		
	for (int y = 0; y < new_height; ++y) {
    	for (int x = 0; x < new_width; ++x) {
    	unsigned char* new_pixel = new_pixels + y * new_rowstride + x * new_channels;
        	if(angle_deg > 30 || angle_deg < -30){
        		if(new_pixel[0] == 255 && new_pixel[1] == 255 && new_pixel[2] == 255){
	                new_pixel[0] = 255;  // Red
    	            new_pixel[1] = 0;  // Green
    	            new_pixel[2] = 0;  // Blue
    	            if (channels == 4) {
    	                new_pixel[3] = 255;  // Alpha (fully opaque)
    	            }
	            }
            }
            if(y == 98 || y == 99 || y == 100 || y == 101){
    			if(x <= 15 || x == 199 || x == 198 || x == 197 || x == 196
    			|| x == 195|| x == 194|| x == 193|| x == 192|| x == 191 || x == 190
    			|| x == 189|| x == 188|| x == 187|| x == 186 || x == 185){
    				new_pixel[0] = 0;  // Red
		            new_pixel[1] = 0;  // Green
		            new_pixel[2] = 0;  // Blue
		            if (channels == 4) {
		                new_pixel[3] = 255;  // Alpha (fully opaque)
		            }
    			}
    		}
        }
	}
    return resized_pixbuf;
}


void updateGUI (BinaryMessage& message){

    for(int frameIndex=0; frameIndex < infoFrameList.size(); frameIndex++){
	    InfoFrame* infoFrame = infoFrameList[frameIndex]; 
        if(infoFrame->get_label() == message.getLabel()){
            if(message.getLabel() == "Zed"){
                for(int elementIndex=0; elementIndex<message.getObject().elementList.size(); elementIndex++){
                    Element element=message.getObject().elementList[elementIndex];
                    if(element.type == TYPE::FLOAT32){
                        if(element.label == "roll"){
                            roll_rotation_angle = std::round(element.data.front().float32);
                            Glib::RefPtr<Gdk::Pixbuf> newrollpixbuf = rotate_image(roll_pixbuf, -roll_rotation_angle, 200, 200);
                            roll_image->set(newrollpixbuf);
                        }
                        if(element.label == "yaw"){
                            pitch_rotation_angle = std::round(element.data.front().float32);
                            Glib::RefPtr<Gdk::Pixbuf> newpitchpixbuf = rotate_image(pitch_pixbuf, pitch_rotation_angle, 200, 200);
                            pitch_image->set(newpitchpixbuf);
                        }
                    }
                }
            }
            for(int elementIndex=0; elementIndex<message.getObject().elementList.size(); elementIndex++){
                Element element=message.getObject().elementList[elementIndex];
                if(element.type == TYPE::BOOLEAN){
                    infoFrame->setItem(element.label, element.data.front().boolean );
               }
                if(element.type == TYPE::UINT8){
                	infoFrame->setItem(element.label, element.data.front().uint8 );
                }
                if(element.type == TYPE::INT8){
                    infoFrame->setItem(element.label, element.data.front().int8 );
                }
                if(element.type == TYPE::UINT16){
                	if(element.label == "Bus Voltage" || element.label == "Output Current"){
						// Change this over to float
						float voltage = (element.data.begin()->uint16 / 100.0);
						infoFrame->setItem(element.label, voltage);
		    		}
		    		else{
	                    infoFrame->setItem(element.label, element.data.front().uint16 );
					}
                }
                if(element.type == TYPE::INT16){
                    infoFrame->setItem(element.label, element.data.front().int16 );
                }
                if(element.type == TYPE::UINT32){
                    infoFrame->setItem(element.label, element.data.front().uint32 );
                }
                if(element.type == TYPE::INT32){
                    infoFrame->setItem(element.label, element.data.front().int32 );
                }
                if(element.type == TYPE::UINT64){
                    infoFrame->setItem(element.label, element.data.front().uint64 );
                }
                if(element.type == TYPE::INT64){
                    infoFrame->setItem(element.label, element.data.front().int64 );
                }
                if(element.type == TYPE::FLOAT32){
                    infoFrame->setItem(element.label, element.data.front().float32 );
                }
                if(element.type == TYPE::FLOAT64){
                    infoFrame->setItem(element.label, element.data.front().float64 );
                }
                if(element.type == TYPE::STRING){
                    std::string theString= "";
                    for(auto iterator=element.data.begin(); iterator != element.data.end(); iterator++ ){
                        theString.push_back(iterator->character);
                    }
                    infoFrame->setItem(element.label, theString );
                }
            }
            return;
        }
    }

    std::string label = message.getLabel();
	if(label == "Falcon 1" || label == "Falcon 2" || label == "Falcon 3" || label == "Falcon 4"
	|| label == "Talon 1" || label == "Talon 2" || label == "Talon 3" || label == "Talon 4"
	|| label == "Linear 1" || label == "Linear 2" || label == "Linear 3" || label == "Linear 4"
	|| label == "Zed" || label == "Autonomy" || label == "Communication"){
        InfoFrame* infoFrame=Gtk::manage( new InfoFrame(message.getLabel()) );
        infoFrameList.push_back(infoFrame);

        for(int index=0;index<message.getObject().elementList.size() ; index++){
            Element element=message.getObject().elementList[index];
            if(element.type == TYPE::BOOLEAN){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->boolean );
            }
            if(element.type == TYPE::INT8){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->int8 );
            }
            if(element.type == TYPE::UINT8){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->uint8 );
            }
            if(element.type == TYPE::INT16){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->int16 );
            }
            if(element.type == TYPE::UINT16){
                if(element.label == "Bus Voltage" || element.label == "Output Current"){
                    // Change this over to float
                    infoFrame->addItem(element.label);
                    float voltage = (element.data.begin()->uint16 / 100.0);
                    infoFrame->setItem(element.label, voltage);
                }
                else{
                    infoFrame->addItem(element.label);
                    infoFrame->setItem(element.label, element.data.begin()->uint16 );
                }
            }
            if(element.type == TYPE::INT32){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->int32 );
            }
            if(element.type == TYPE::UINT32){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->uint32 );
            }
            if(element.type == TYPE::INT64){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->int64 );
            }
            if(element.type == TYPE::UINT64){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->uint64 );
            }
            if(element.type == TYPE::FLOAT32){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->float32 );
            }
            if(element.type == TYPE::FLOAT64){
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, element.data.begin()->float64 );
            }
            if(element.type == TYPE::STRING){
                std::string theString= "";
                for(auto iterator=element.data.begin(); iterator != element.data.end(); iterator++ ){
                    theString.push_back(iterator->character);
                }
                infoFrame->addItem(element.label);
                infoFrame->setItem(element.label, theString );
            }
        }

        sensorBox->add(*infoFrame);
        infoFrame->show();
    }
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

    Glib::ListHandle<Gtk::Widget*> childList = sensorBox->get_children();
    Glib::ListHandle<Gtk::Widget*>::iterator it = childList.begin();
    while (it != childList.end()) {
        sensorBox->remove(*(*it));
        it++;
    }

    infoFrameList.clear();
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


void connectToServer(){
    if(connected==true)return;
    struct sockaddr_in address; 
    int bytesRead; 
    struct sockaddr_in serv_addr; 
    std::string hello("Hello Robot"); 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT);

    char buffer[2048] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 

        printf("\n Socket creation error \n");

        setDisconnectedState();
        return; 
    } 
    if(inet_pton(AF_INET, ipAddressEntry->get_text().c_str(), &serv_addr.sin_addr)<=0)  { 

        printf("\nInvalid address/ Address not supported \n");

        Gtk::MessageDialog dialog(*window,"Invalid Address",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK);
        int result=dialog.run();

        setDisconnectedState();
        return;
    } 
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");

        Gtk::MessageDialog dialog(*window,"Connection Failed",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK);
        int result=dialog.run();

        setDisconnectedState();
    }
    else{
        send(sock , hello.c_str() , strlen(hello.c_str()) , 0 );
        bytesRead = read( sock , buffer, 2048);
        fcntl(sock,F_SETFL, O_NONBLOCK);

        setConnectedState();
    }
}


void disconnectFromServer(){
    Gtk::MessageDialog dialog(*window,"Disconnect now?",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_OK_CANCEL);
    //dialog.set_secondary_text("Do you want to shutdown now?");
    int result=dialog.run();

    switch(result) {
        case (Gtk::RESPONSE_OK): 
            if(shutdown(sock,SHUT_RDWR)==-1){
                Gtk::MessageDialog dialog(*window,"Failed Shutdown",false,Gtk::MESSAGE_ERROR,Gtk::BUTTONS_OK);
                int result=dialog.run();
            }
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
        send(sock, message, messageSize, 0); 

        silentRunButton->set_label("Not Silent Running");
    }
    else{
        int messageSize=3;
        uint8_t command=7;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=1;
        send(sock, message, messageSize, 0); 

        silentRunButton->set_label("Silent Running");
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
    send(sock, message, messageSize, 0);
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


bool on_key_release_event(GdkEventKey* key_event){
    int messageSize=5;
    uint8_t command=2;// keyboard
    uint8_t message[messageSize];
    message[0]=messageSize;
    message[1]=command;
    message[2]=(uint8_t)(((key_event->keyval)>>8)& 0xff);
    message[3]=(uint8_t)(((key_event->keyval)>>0)& 0xff);
    message[4]=0;
    send(sock, message, messageSize, 0);

    return false;
}


bool on_key_press_event(GdkEventKey* key_event){
    int messageSize=5;
    uint8_t command=2;// keyboard
    uint8_t message[messageSize];
    message[0]=messageSize;
    message[1]=command;
    message[2]=(uint8_t)(((key_event->keyval)>>8)& 0xff);
    message[3]=(uint8_t)(((key_event->keyval)>>0)& 0xff);
    message[4]=1;
    send(sock, message, messageSize, 0);

    return false;
}


void setupGUI(Glib::RefPtr<Gtk::Application> application){

    // Create windows instance
    window=new Gtk::Window();

    // Location of icon and set icon
    //auto icon = "../resources/razorbotz.png";
    //window->set_icon_from_file(icon);

    try{
        auto icon = "../resources/razorbotz.png";
        window->set_icon_from_file(icon);
    }
    catch(const Glib::FileError& e){
        g_print("Failed to load image: %s\n", e.what().c_str());
        return;
    }



    // Handles key press and release events  
    window->add_events(Gdk::KEY_PRESS_MASK);
    window->add_events(Gdk::KEY_RELEASE_MASK);
    window->signal_key_press_event().connect(sigc::ptr_fun(&on_key_press_event));
    window->signal_key_release_event().connect(sigc::ptr_fun(&on_key_release_event));

    // Create verticle box to hold top level widgets 
    Gtk::Box* topLevelBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));

    // Create horizontal box to hold control widgets
    Gtk::Box* controlsBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));

    // Create scolled window instance and list of addresses 
    Gtk::ScrolledWindow* scrolledList=Gtk::manage(new Gtk::ScrolledWindow());
    addressListBox=Gtk::manage(new Gtk::ListBox());
    addressListBox->signal_row_activated().connect(sigc::ptr_fun(&rowActivated));

    // Create Verticle box on right of screen to house controls 
    Gtk::Box* controlsRightBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));

    // Create box to hold connection information (IP, connect button, etc.)
    Gtk::Box* connectBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));
    
    Gtk::Label* ipAddressLabel=Gtk::manage(new Gtk::Label(" IP Address "));

    // Create entry box for IP connection
    ipAddressEntry=Gtk::manage(new Gtk::Entry());
    ipAddressEntry->set_can_focus(true);
    ipAddressEntry->set_editable(true);
    ipAddressEntry->set_text("192.168.1.6");

    // Create connection button, single click logic to connectOrDisconnect function
    connectButton=Gtk::manage(new Gtk::Button("Connect"));
    connectButton->signal_clicked().connect(sigc::ptr_fun(&connectOrDisconnect));
    connectionStatusLabel=Gtk::manage(new Gtk::Label("Not Connected"));
    // Disconnect graphics for connect button
    Gdk::RGBA red;
    red.set_rgba(1.0,0,0,1.0);
    connectionStatusLabel->override_background_color(red);
    
    // Create horizontal box to hold silent run functionality
    Gtk::Box* stateBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2));
    silentRunButton=Gtk::manage(new Gtk::Button("Silent Running"));
    silentRunButton->signal_clicked().connect(sigc::ptr_fun(&silentRun));

    // Create horizontal box to hold remote control functionality
    Gtk::Box* remoteControlBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2));

    // Create button to shutdown robot
    Gtk::Button* shutdownRobotButton=Gtk::manage(new Gtk::Button("Shutdown Robot"));
    shutdownRobotButton->signal_clicked().connect(sigc::bind<Gtk::Window*>(sigc::ptr_fun(&shutdownDialog),window));

    // Create horizontal flow box to hold sensor widgets
    sensorBox=Gtk::manage(new Gtk::FlowBox());
    sensorBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);

    // Set size for address list box
    addressListBox->set_size_request(200,100);
    scrolledList->set_size_request(200,100);

    // Add widgets to connect box
    connectBox->add(*ipAddressLabel);
    connectBox->add(*ipAddressEntry);
    connectBox->add(*connectButton);
    connectBox->add(*connectionStatusLabel);

    // Add widgets to silent run box
    stateBox->add(*silentRunButton);

    // Add widgets to shut down robot box
    remoteControlBox->add(*shutdownRobotButton);

    // Add wigets to controls box
    controlsRightBox->add(*connectBox);
    controlsRightBox->add(*stateBox);
    controlsRightBox->add(*remoteControlBox);

    // Add address list to scrollable list
    scrolledList->add(*addressListBox);

    // Add widgets to controls box
    controlsBox->add(*scrolledList);
    controlsBox->add(*controlsRightBox);

    roll_image = Gtk::manage(new Gtk::Image());
    sensorBox->add(*roll_image);
    
    try{
        roll_pixbuf = Gdk::Pixbuf::create_from_file("../resources/RobotSide.png");
    }
    catch(const Glib::FileError& e){
        g_print("Failed to load image: %s\n", e.what().c_str());
        return;
    }
    
    Glib::RefPtr<Gdk::Pixbuf> newrollpixbuf = rotate_image(roll_pixbuf, roll_rotation_angle, 200, 200);
    roll_image->set(newrollpixbuf);
    
    pitch_image = Gtk::manage(new Gtk::Image());
    sensorBox->add(*pitch_image);
    
    try{
        pitch_pixbuf = Gdk::Pixbuf::create_from_file("../resources/RobotBack.png");
    }
    catch(const Glib::FileError& e){
        g_print("Failed to load image: %s\n", e.what().c_str());
        return;
    }
    
    Glib::RefPtr<Gdk::Pixbuf> newpitchpixbuf = rotate_image(pitch_pixbuf, pitch_rotation_angle, 200, 200);
    pitch_image->set(newpitchpixbuf);

    // Add wigets to top level box
    topLevelBox->add(*controlsBox);
    topLevelBox->add(*sensorBox);
    window->add(*topLevelBox);

    window->signal_delete_event().connect(sigc::ptr_fun(quit));
    window->show_all();

}


struct RemoteRobot{
    std::string tag;
    time_t lastSeenTime;
};
std::vector<RemoteRobot> robotList;


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
    localSock.sin_port = htons(4321);
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

    char databuf[2048];
    int datalen = sizeof(databuf);
    while(true){
        if(read(sd, databuf, datalen) >= 0) {
            std::string message(databuf);
            if(!contains(robotList,message)) {
                RemoteRobot remoteRobot;
                remoteRobot.tag=message; 
                time(&remoteRobot.lastSeenTime);
                robotList.push_back(remoteRobot);
            }
            update(robotList,message);
        }
    }
}


void adjustRobotList(){
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
        if(!match){
            addressListBox->remove(*listBoxRow);
            --index;
        }
    }
}

 
void initGUI(){
    BinaryMessage talonMessage1("Talon 1");
    talonMessage1.addElementUInt8("Device ID",(uint8_t)0);
    talonMessage1.addElementUInt16("Bus Voltage",0);
    talonMessage1.addElementUInt16("Output Current",0);
    talonMessage1.addElementFloat32("Output Percent",0.0);
    talonMessage1.addElementUInt8("Temperature",(uint8_t)0);
    talonMessage1.addElementUInt16("Sensor Position",(uint8_t)0);
    talonMessage1.addElementInt8("Sensor Velocity",(uint8_t)0);
    talonMessage1.addElementFloat32("Max Current", 0.0);
    updateGUI(talonMessage1);

    BinaryMessage talonMessage2("Talon 2");
    talonMessage2.addElementUInt8("Device ID",(uint8_t)0);
    talonMessage2.addElementUInt16("Bus Voltage",0);
    talonMessage2.addElementUInt16("Output Current",0);
    talonMessage2.addElementFloat32("Output Percent",0.0);
    talonMessage2.addElementUInt8("Temperature",(uint8_t)0);
    talonMessage2.addElementUInt16("Sensor Position",(uint8_t)0);
    talonMessage2.addElementInt8("Sensor Velocity",(uint8_t)0);
    talonMessage2.addElementFloat32("Max Current", 0.0);
    updateGUI(talonMessage2);

    BinaryMessage talonMessage3("Talon 3");
    talonMessage3.addElementUInt8("Device ID",(uint8_t)0);
    talonMessage3.addElementUInt16("Bus Voltage",0);
    talonMessage3.addElementUInt16("Output Current",0);
    talonMessage3.addElementFloat32("Output Percent",0.0);
    talonMessage3.addElementUInt8("Temperature",(uint8_t)0);
    talonMessage3.addElementUInt16("Sensor Position",(uint8_t)0);
    talonMessage3.addElementInt8("Sensor Velocity",(uint8_t)0);
    talonMessage3.addElementFloat32("Max Current", 0.0);
    updateGUI(talonMessage3);

    BinaryMessage talonMessage4("Talon 4");
    talonMessage4.addElementUInt8("Device ID",(uint8_t)0);
    talonMessage4.addElementUInt16("Bus Voltage",0);
    talonMessage4.addElementUInt16("Output Current",0);
    talonMessage4.addElementFloat32("Output Percent",0.0);
    talonMessage4.addElementUInt8("Temperature",(uint8_t)0);
    talonMessage4.addElementUInt16("Sensor Position",(uint8_t)0);
    talonMessage4.addElementInt8("Sensor Velocity",(uint8_t)0);
    talonMessage4.addElementFloat32("Max Current", 0.0);
    updateGUI(talonMessage4);

    BinaryMessage falconMessage1("Falcon 1");
    falconMessage1.addElementUInt8("Device ID",(uint8_t)0);
    falconMessage1.addElementUInt16("Bus Voltage",0);
    falconMessage1.addElementUInt16("Output Current",0);
    falconMessage1.addElementFloat32("Output Percent",0.0);
    falconMessage1.addElementUInt8("Temperature",(uint8_t)0);
    falconMessage1.addElementUInt16("Sensor Position",(uint8_t)0);
    falconMessage1.addElementInt8("Sensor Velocity",(uint8_t)0);
    falconMessage1.addElementFloat32("Max Current", 0.0);
    updateGUI(falconMessage1);

    BinaryMessage falconMessage2("Falcon 2");
    falconMessage2.addElementUInt8("Device ID",(uint8_t)0);
    falconMessage2.addElementUInt16("Bus Voltage",0);
    falconMessage2.addElementUInt16("Output Current",0);
    falconMessage2.addElementFloat32("Output Percent",0.0);
    falconMessage2.addElementUInt8("Temperature",(uint8_t)0);
    falconMessage2.addElementUInt16("Sensor Position",(uint8_t)0);
    falconMessage2.addElementInt8("Sensor Velocity",(uint8_t)0);
    falconMessage2.addElementFloat32("Max Current", 0.0);
    updateGUI(falconMessage2);

    BinaryMessage falconMessage3("Falcon 3");
    falconMessage3.addElementUInt8("Device ID",(uint8_t)0);
    falconMessage3.addElementUInt16("Bus Voltage",0);
    falconMessage3.addElementUInt16("Output Current",0);
    falconMessage3.addElementFloat32("Output Percent",0.0);
    falconMessage3.addElementUInt8("Temperature",(uint8_t)0);
    falconMessage3.addElementUInt16("Sensor Position",(uint8_t)0);
    falconMessage3.addElementInt8("Sensor Velocity",(uint8_t)0);
    falconMessage3.addElementFloat32("Max Current", 0.0);
    updateGUI(falconMessage3);

    BinaryMessage falconMessage4("Falcon 4");
    falconMessage4.addElementUInt8("Device ID",(uint8_t)0);
    falconMessage4.addElementUInt16("Bus Voltage",0);
    falconMessage4.addElementUInt16("Output Current",0);
    falconMessage4.addElementFloat32("Output Percent",0.0);
    falconMessage4.addElementUInt8("Temperature",(uint8_t)0);
    falconMessage4.addElementUInt16("Sensor Position",(uint8_t)0);
    falconMessage4.addElementInt8("Sensor Velocity",(uint8_t)0);
    falconMessage4.addElementFloat32("Max Current", 0.0);
    updateGUI(falconMessage4);

    std::shared_ptr<std::list<uint8_t>> byteList = falconMessage4.getBytes();

    std::vector<uint8_t> bytes(byteList->size());
    int index = 0;
    for(auto byteIterator = byteList->begin(); byteIterator != byteList->end(); byteIterator++, index++){
        bytes.at(index) = *byteIterator;
    }
    for(std::uint8_t byte : bytes){
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;
}

// void initGUI(){
// 	BinaryMessage talonMessage1("Talon 1");
//     talonMessage1.addElementUInt8("Device ID",(uint8_t)0);
//     talonMessage1.addElementUInt16("Bus Voltage",0);
//     talonMessage1.addElementUInt16("Output Current",9);
//     talonMessage1.addElementFloat32("Output Percent",0.0);
//     talonMessage1.addElementUInt8("Temperature",(uint8_t)0);
//     talonMessage1.addElementUInt16("Sensor Position",(uint8_t)0);
//     talonMessage1.addElementInt8("Sensor Velocity",(uint8_t)0);
//     talonMessage1.addElementFloat32("Max Current", 0.0);
//     updateGUI(talonMessage1);
    
//     BinaryMessage talonMessage2("Talon 2");
//     talonMessage2.addElementUInt8("Device ID",(uint8_t)0);
//     talonMessage2.addElementUInt16("Bus Voltage",0);
//     talonMessage2.addElementUInt16("Output Current",9);
//     talonMessage2.addElementFloat32("Output Percent",0.0);
//     talonMessage2.addElementUInt8("Temperature",(uint8_t)0);
//     talonMessage2.addElementUInt16("Sensor Position",(uint8_t)0);
//     talonMessage2.addElementInt8("Sensor Velocity",(uint8_t)0);
//     talonMessage2.addElementFloat32("Max Current", 0.0);
//     updateGUI(talonMessage2);
    
//     BinaryMessage talonMessage3("Talon 3");
//     talonMessage3.addElementUInt8("Device ID",(uint8_t)0);
//     talonMessage3.addElementUInt16("Bus Voltage",0);
//     talonMessage3.addElementUInt16("Output Current",9);
//     talonMessage3.addElementFloat32("Output Percent",0.0);
//     talonMessage3.addElementUInt8("Temperature",(uint8_t)0);
//     talonMessage3.addElementUInt16("Sensor Position",(uint8_t)0);
//     talonMessage3.addElementInt8("Sensor Velocity",(uint8_t)0);
//     talonMessage3.addElementFloat32("Max Current", 0.0);
//     updateGUI(talonMessage3);
    
//     BinaryMessage talonMessage4("Talon 4");
//     talonMessage4.addElementUInt8("Device ID",(uint8_t)0);
//     talonMessage4.addElementUInt16("Bus Voltage",0);
//     talonMessage4.addElementUInt16("Output Current",9);
//     talonMessage4.addElementFloat32("Output Percent",0.0);
//     talonMessage4.addElementUInt8("Temperature",(uint8_t)0);
//     talonMessage4.addElementUInt16("Sensor Position",(uint8_t)0);
//     talonMessage4.addElementInt8("Sensor Velocity",(uint8_t)0);
//     talonMessage4.addElementFloat32("Max Current", 0.0);
//     updateGUI(talonMessage4);
    
//     BinaryMessage falconMessage1("Falcon 1");
//     falconMessage1.addElementUInt8("Device ID",(uint8_t)0);
//     falconMessage1.addElementUInt16("Bus Voltage",0);
//     falconMessage1.addElementUInt16("Output Current",9);
//     falconMessage1.addElementFloat32("Output Percent",0.0);
//     falconMessage1.addElementUInt8("Temperature",(uint8_t)0);
//     falconMessage1.addElementUInt16("Sensor Position",(uint8_t)0);
//     falconMessage1.addElementInt8("Sensor Velocity",(uint8_t)0);
//     falconMessage1.addElementFloat32("Max Current", 0.0);
//     updateGUI(falconMessage1);
    
//     BinaryMessage falconMessage2("Falcon 2");
//     falconMessage2.addElementUInt8("Device ID",(uint8_t)0);
//     falconMessage2.addElementUInt16("Bus Voltage",0);
//     falconMessage2.addElementUInt16("Output Current",9);
//     falconMessage2.addElementFloat32("Output Percent",0.0);
//     falconMessage2.addElementUInt8("Temperature",(uint8_t)0);
//     falconMessage2.addElementUInt16("Sensor Position",(uint8_t)0);
//     falconMessage2.addElementInt8("Sensor Velocity",(uint8_t)0);
//     falconMessage2.addElementFloat32("Max Current", 0.0);
//     updateGUI(falconMessage2);
    
//     BinaryMessage falconMessage3("Falcon 3");
//     falconMessage3.addElementUInt8("Device ID",(uint8_t)0);
//     falconMessage3.addElementUInt16("Bus Voltage",0);
//     falconMessage3.addElementUInt16("Output Current",9);
//     falconMessage3.addElementFloat32("Output Percent",0.0);
//     falconMessage3.addElementUInt8("Temperature",(uint8_t)0);
//     falconMessage3.addElementUInt16("Sensor Position",(uint8_t)0);
//     falconMessage3.addElementInt8("Sensor Velocity",(uint8_t)0);
//     falconMessage3.addElementFloat32("Max Current", 0.0);
//     updateGUI(falconMessage3);
    
//     BinaryMessage falconMessage4("Falcon 4");
//     falconMessage4.addElementUInt8("Device ID",(uint8_t)0);
//     falconMessage4.addElementUInt16("Bus Voltage",0);
//     falconMessage4.addElementUInt16("Output Current",9);
//     falconMessage4.addElementFloat32("Output Percent",0.0);
//     falconMessage4.addElementUInt8("Temperature",(uint8_t)0);
//     falconMessage4.addElementUInt16("Sensor Position",(uint8_t)0);
//     falconMessage4.addElementInt8("Sensor Velocity",(uint8_t)0);
//     falconMessage4.addElementFloat32("Max Current", 0.0);
//     updateGUI(falconMessage4);
    
//     BinaryMessage linearMessage1("Linear 1");
//     linearMessage1.addElementUInt8("Motor Number", (uint8_t)0);
//     linearMessage1.addElementFloat32("Speed", 0.0);
//     linearMessage1.addElementUInt16("Potentiometer", (uint16_t)0);
//     linearMessage1.addElementUInt8("Time Without Change", (uint8_t)0);
//     linearMessage1.addElementUInt16("Max", (uint16_t)0);
//     linearMessage1.addElementUInt16("Min", (uint16_t)0);
//     linearMessage1.addElementString("Error", "No Error");
//     linearMessage1.addElementBoolean("At Min", false);
//     linearMessage1.addElementBoolean("At Max", false);
//     linearMessage1.addElementFloat32("Distance", 0.0);
//     linearMessage1.addElementBoolean("Sensorless", false);
//     updateGUI(linearMessage1);
    
//     BinaryMessage linearMessage2("Linear 2");
//     linearMessage2.addElementUInt8("Motor Number", (uint8_t)0);
//     linearMessage2.addElementFloat32("Speed", 0.0);
//     linearMessage2.addElementUInt16("Potentiometer", (uint16_t)0);
//     linearMessage2.addElementUInt8("Time Without Change", (uint8_t)0);
//     linearMessage2.addElementUInt16("Max", (uint16_t)0);
//     linearMessage2.addElementUInt16("Min", (uint16_t)0);
//     linearMessage2.addElementString("Error", "No Error");
//     linearMessage2.addElementBoolean("At Min", false);
//     linearMessage2.addElementBoolean("At Max", false);
//     linearMessage2.addElementFloat32("Distance", 0.0);
//     linearMessage2.addElementBoolean("Sensorless", false);
//     updateGUI(linearMessage2);
    
//     BinaryMessage linearMessage3("Linear 3");
//     linearMessage3.addElementUInt8("Motor Number", (uint8_t)0);
//     linearMessage3.addElementFloat32("Speed", 0.0);
//     linearMessage3.addElementUInt16("Potentiometer", (uint16_t)0);
//     linearMessage3.addElementUInt8("Time Without Change", (uint8_t)0);
//     linearMessage3.addElementUInt16("Max", (uint16_t)0);
//     linearMessage3.addElementUInt16("Min", (uint16_t)0);
//     linearMessage3.addElementString("Error", "No Error");
//     linearMessage3.addElementBoolean("At Min", false);
//     linearMessage3.addElementBoolean("At Max", false);
//     linearMessage3.addElementFloat32("Distance", 0.0);
//     linearMessage3.addElementBoolean("Sensorless", false);
//     updateGUI(linearMessage3);
    
//     BinaryMessage linearMessage4("Linear 4");
//     linearMessage4.addElementUInt8("Motor Number", (uint8_t)0);
//     linearMessage4.addElementFloat32("Speed", 0.0);
//     linearMessage4.addElementUInt16("Potentiometer", (uint16_t)0);
//     linearMessage4.addElementUInt8("Time Without Change", (uint8_t)0);
//     linearMessage4.addElementUInt16("Max", (uint16_t)0);
//     linearMessage4.addElementUInt16("Min", (uint16_t)0);
//     linearMessage4.addElementString("Error", "No Error");
//     linearMessage4.addElementBoolean("At Min", false);
//     linearMessage4.addElementBoolean("At Max", false);
//     linearMessage4.addElementFloat32("Distance", 0.0);
//     linearMessage4.addElementBoolean("Sensorless", false);
//     updateGUI(linearMessage4);
    
//     BinaryMessage communicationMessage("Communication");
//     communicationMessage.addElementString("Wi-Fi", "NORMAL OPERATION");
//     communicationMessage.addElementString("CAN BUS", "NON-FUNCTIONAL OPERATION");
//     updateGUI(communicationMessage);
    
//     BinaryMessage autonomyMessage("Autonomy");
//     autonomyMessage.addElementString("Robot State", "No");
//     autonomyMessage.addElementString("Excavation State", "No");
//     autonomyMessage.addElementString("Error State", "No");
//     autonomyMessage.addElementString("Diagnostics State", "No");
//     updateGUI(autonomyMessage);
    
//     BinaryMessage zedMessage("Zed");
//     zedMessage.addElementFloat32("X", 0.0);
//     zedMessage.addElementFloat32("Y", 0.0);
//     zedMessage.addElementFloat32("Z", 0.0);
//     zedMessage.addElementFloat32("roll", 0.0);
//     zedMessage.addElementFloat32("pitch", 0.0);
//     zedMessage.addElementFloat32("yaw", 0.0);
//     zedMessage.addElementFloat32("aruco roll", 0.0);
//     zedMessage.addElementFloat32("aruco pitch", 0.0);
//     zedMessage.addElementFloat32("aruco yaw", 0.0);
//     zedMessage.addElementBoolean("aruco", false);
//     updateGUI(zedMessage);
// }


int main(int argc, char** argv) { 
    Glib::RefPtr<Gtk::Application> application = Gtk::Application::create(argc, argv, "edu.uark.razorbotz");
    setupGUI(application);
    initGUI();

    std::thread broadcastListenThread(broadcastListen);

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    int joystickCount=SDL_NumJoysticks();
    std::cout << "number of joysticks " << joystickCount << std::endl;
    SDL_Joystick* joystickList[joystickCount];

    if(joystickCount>0){
        axisEventList = new std::vector<std::vector<AxisEvent*>*>(joystickCount);
        for(int joystickIndex=0;joystickIndex<joystickCount;joystickIndex++) {

            joystickList[joystickIndex]=SDL_JoystickOpen(joystickIndex);

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
    char buffer[2048] = {0}; 
    int bytesRead=0;

    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point lastTransmitTime = std::chrono::high_resolution_clock::now();
    std::list<uint8_t> messageBytesList;
    uint8_t message[256];
    bool running=true;
    while(running){
        adjustRobotList();

        while(Gtk::Main::events_pending()){
            Gtk::Main::iteration();
        }

        if(!connected){
            if(messageBytesList.size() > 0){
                messageBytesList.clear();
            }
            continue;
        }

        std::cout << "Before Read" << std::endl;

        bytesRead = read(sock, buffer, 2048);
        if(bytesRead==0){
            //std::cout << "Lost Connection" << std::endl;
            setDisconnectedState();
            if(messageBytesList.size() > 0){
                messageBytesList.clear();
            }
            continue;
        }

        std::cout << "After Read" << std::endl;

        for(int index=0;index<bytesRead;index++){
            messageBytesList.push_back(buffer[index]);
        }

        std::cout << "Before hasMessage check" << std::endl;
        while(BinaryMessage::hasMessage(messageBytesList)){
            std::cout << "Before message create" << std::endl;
            BinaryMessage message(messageBytesList);
            std::cout << "Before GUI update" << std::endl;
            updateGUI(message);
            std::cout << "Before size decode" << std::endl;
            uint64_t size=BinaryMessage::decodeSizeBytes(messageBytesList);
            for(int count=0; count < size; count++){
                //std::cout << messageBytesList.front();
                messageBytesList.pop_front();
            }
            std::cout << std::endl;
        }

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

                    send(sock, message, length, 0);

                    break;
                }
                case SDL_JOYBUTTONDOWN:{
                    std::cout << "Joystick button down" << std::endl;
                    uint8_t command=5;
                    int length=5;
                    uint8_t message[length];
                    message[0]=length;
                    message[1]=command;
                    message[2]=event.jbutton.which;
                    message[3]=event.jbutton.button;
                    message[4]=event.jbutton.state;

                    send(sock, message, length, 0);

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

                    send(sock, message, length, 0);

                    break;
                }
                case SDL_JOYAXISMOTION: {
                    std::cout << "Joystick axis motion" << std::endl;
                    int deadZone=4000;
                    if(event.jaxis.value < -deadZone || deadZone < event.jaxis.value ) {
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->isSet = true;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->which = event.jaxis.which;
                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->axis  = event.jaxis.axis;

                        int value = event.jaxis.value;
                        if(value < -deadZone)   value+=deadZone;
                        if(deadZone < value) value-=deadZone;

                        axisEventList->at(event.jaxis.which)->at(event.jaxis.axis)->value = value;
                    }

                    break;
                }

                default:
                    break;
            }
        }

        now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTransmitTime);
        double deltaTime = time_span.count();
        if(deltaTime > 0.05 ){
            lastTransmitTime = std::chrono::high_resolution_clock::now();
            for(int joystickIndex=0; joystickIndex < axisEventList->size(); joystickIndex++){
                for(int axisIndex=0; axisIndex < axisEventList->at(joystickIndex)->size(); axisIndex++){
                    if(axisEventList->at(joystickIndex)->at(axisIndex)->isSet){
                        std::cout << joystickIndex << " " << axisIndex << " " << axisEventList->at(joystickIndex)->at(axisIndex)->value << std::endl;
                        axisEventList->at(joystickIndex)->at(axisIndex)->isSet = false;

                        uint8_t command = 1;
                        int length = 8;
                        float value = ((float)axisEventList->at(joystickIndex)->at(axisIndex)->value) / -32768.0;
                        uint8_t message[length];
                        message[0] = length;
                        message[1] = command;
                        message[2] = axisEventList->at(joystickIndex)->at(axisIndex)->which;
                        message[3] = axisEventList->at(joystickIndex)->at(axisIndex)->axis;//0-roll 1-pitch 2-throttle 3-yaw
                        insert(value, &message[4]);

                        send(sock, message, length, 0);
                    }
                }
            }
        }
    }
    return 0; 
}