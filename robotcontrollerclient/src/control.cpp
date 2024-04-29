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
#include <gdkmm.h>

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

std::vector<InfoFrame*> infoFrameList;

struct AxisEvent{
    bool isSet=false;
    uint8_t which;
    uint8_t axis;  //0-roll 1-pitch 2-throttle 3-yaw
    int value;
};
std::vector<std::vector<AxisEvent*>*>* axisEventList;

void updateGUI (BinaryMessage& message){

    for(int frameIndex=0; frameIndex < infoFrameList.size(); frameIndex++){
	InfoFrame* infoFrame = infoFrameList[frameIndex];    
        if(infoFrame->get_label() == message.getLabel()){
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

    window=new Gtk::Window();

    window->add_events(Gdk::KEY_PRESS_MASK);
    window->add_events(Gdk::KEY_RELEASE_MASK);

    window->signal_key_press_event().connect(sigc::ptr_fun(&on_key_press_event));
    window->signal_key_release_event().connect(sigc::ptr_fun(&on_key_release_event));

    Gtk::Box* topLevelBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));

    Gtk::Box* controlsBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));

    Gtk::ScrolledWindow* scrolledList=Gtk::manage(new Gtk::ScrolledWindow());
    addressListBox=Gtk::manage(new Gtk::ListBox());
    addressListBox->signal_row_activated().connect(sigc::ptr_fun(&rowActivated));

    Gtk::Box* controlsRightBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));

    Gtk::Box* connectBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,5));
    Gtk::Label* ipAddressLabel=Gtk::manage(new Gtk::Label(" IP Address "));
    ipAddressEntry=Gtk::manage(new Gtk::Entry());
    ipAddressEntry->set_can_focus(true);
    ipAddressEntry->set_editable(true);
    ipAddressEntry->set_text("192.168.1.6");
    connectButton=Gtk::manage(new Gtk::Button("Connect"));
    connectButton->signal_clicked().connect(sigc::ptr_fun(&connectOrDisconnect));
    connectionStatusLabel=Gtk::manage(new Gtk::Label("Not Connected"));
    Gdk::RGBA red;
    red.set_rgba(1.0,0,0,1.0);
    connectionStatusLabel->override_background_color(red);
    
    Gtk::Box* stateBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2));
    silentRunButton=Gtk::manage(new Gtk::Button("Silent Running"));
    silentRunButton->signal_clicked().connect(sigc::ptr_fun(&silentRun));

    Gtk::Box* remoteControlBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2));
    Gtk::Button* shutdownRobotButton=Gtk::manage(new Gtk::Button("Shutdown Robot"));
    shutdownRobotButton->signal_clicked().connect(sigc::bind<Gtk::Window*>(sigc::ptr_fun(&shutdownDialog),window));


    sensorBox=Gtk::manage(new Gtk::FlowBox());
    sensorBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);

    addressListBox->set_size_request(200,100);
    scrolledList->set_size_request(200,100);

    connectBox->add(*ipAddressLabel);
    connectBox->add(*ipAddressEntry);
    connectBox->add(*connectButton);
    connectBox->add(*connectionStatusLabel);

    stateBox->add(*silentRunButton);

    remoteControlBox->add(*shutdownRobotButton);

    controlsRightBox->add(*connectBox);
    controlsRightBox->add(*stateBox);
    controlsRightBox->add(*remoteControlBox);

    scrolledList->add(*addressListBox);

    controlsBox->add(*scrolledList);
    controlsBox->add(*controlsRightBox);

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

 
int main(int argc, char** argv) { 
    Glib::RefPtr<Gtk::Application> application = Gtk::Application::create(argc, argv, "edu.uark.razorbotz");
    setupGUI(application);

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

        if(!connected)continue;

        bytesRead = read(sock, buffer, 2048);
        if(bytesRead==0){
            //std::cout << "Lost Connection" << std::endl;
            setDisconnectedState();
            continue;
        }

        for(int index=0;index<bytesRead;index++){
            messageBytesList.push_back(buffer[index]);
        }

        while(BinaryMessage::hasMessage(messageBytesList)){
            BinaryMessage message(messageBytesList);
            updateGUI(message);
            uint64_t size=BinaryMessage::decodeSizeBytes(messageBytesList);
            for(int count=0; count < size; count++){
                messageBytesList.pop_front();
            }
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
                        //std::cout << joystickIndex << " " << axisIndex << " " << axisEventList->at(joystickIndex)->at(axisIndex)->value << std::endl;
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

