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
#include <opencv2/opencv.hpp>

#define PORT 31338


bool quit(GdkEventAny* event){
    exit(0);
}

Gtk::ListBox* addressListBox;
Gtk::Entry* ipAddressEntry;
Gtk::Label* connectionStatusLabel;
  
Gtk::Button* videoStreamButton;
Gtk::Button* connectButton;
  
Gtk::FlowBox* sensorBox;

Gtk::Window* window;
int sock = 0; 
bool connected=false;

void setDisconnectedState(){
    connectButton->set_label("Connect");
    connectionStatusLabel->set_text("Not Connected");
    videoStreamButton->set_label("Not Video Streaming");
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

    char buffer[1024] = {0}; 
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
        bytesRead = read( sock , buffer, 1024);
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


void videoStream(){
    if(!connected)return;
    std::string currentButtonState=videoStreamButton->get_label();
    if(currentButtonState=="Not Video Streaming"){
        int messageSize=3;
        uint8_t command=1;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=1;
        send(sock, message, messageSize, 0); 

        videoStreamButton->set_label("Video Streaming");
    }
    else{
        int messageSize=3;
        uint8_t command=1;// silence 
        uint8_t message[messageSize];
        message[0]=messageSize;
        message[1]=command;
        message[2]=0;
        send(sock, message, messageSize, 0); 

        videoStreamButton->set_label("Not Video Streaming");
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


void setupGUI(Glib::RefPtr<Gtk::Application> application){

    window=new Gtk::Window();

    window->add_events(Gdk::KEY_PRESS_MASK);
    window->add_events(Gdk::KEY_RELEASE_MASK);

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
    videoStreamButton=Gtk::manage(new Gtk::Button("Not Video Streaming"));
    videoStreamButton->signal_clicked().connect(sigc::ptr_fun(&videoStream));

    sensorBox=Gtk::manage(new Gtk::FlowBox());
    sensorBox->set_orientation(Gtk::ORIENTATION_HORIZONTAL);

    addressListBox->set_size_request(200,100);
    scrolledList->set_size_request(200,100);

    connectBox->add(*ipAddressLabel);
    connectBox->add(*ipAddressEntry);
    connectBox->add(*connectButton);
    connectBox->add(*connectionStatusLabel);

    stateBox->add(*videoStreamButton);

    controlsRightBox->add(*connectBox);
    controlsRightBox->add(*stateBox);

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

    cv::Mat img = cv::Mat::zeros(376, 672, CV_8UC1);
    int imgSize = img.total() * img.elemSize();
    uchar sockData[imgSize];
    int bytesRead=0, total = 0;

    bool running=true;
    while(running){
        adjustRobotList();

        while(Gtk::Main::events_pending()){
            Gtk::Main::iteration();
        }

        if(!connected)continue;
        if(!videoStream)continue;
        total = 0;
        while(total < imgSize){
            bytesRead = recv(sock, &sockData[total], imgSize-total, 0);
            if(bytesRead==0){
                //std::cout << "Lost Connection" << std::endl;
                setDisconnectedState();
                continue;
            }
            if(bytesRead != -1){
                total += bytesRead;
            }
        }
        
        cv::Mat img(376, 672, CV_8UC1, sockData);
        cv::resize(img, img, cv::Size(1400, 800), cv::INTER_LINEAR);
        cv::imshow("Video", img);
        cv::waitKey(10);


    }
    return 0; 
}

