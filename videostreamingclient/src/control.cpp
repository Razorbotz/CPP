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

#define VIDEO_PORT 31338


bool quit(GdkEventAny* event){
    exit(0);
}

Gtk::ListBox* videoAddressListBox;
Gtk::Entry* videoIPAddressEntry;
Gtk::Label* videoConnectionStatusLabel;
  
Gtk::Button* videoStreamButton;
Gtk::Button* videoConnectButton;
bool isStreamingActive = false;
bool isGray = true;
  
Gtk::Window* window;
int videoSock = 0; 
bool videoConnected=false;

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


void setupGUI(Glib::RefPtr<Gtk::Application> application){

    window=new Gtk::Window();

    window->add_events(Gdk::KEY_PRESS_MASK);
    window->add_events(Gdk::KEY_RELEASE_MASK);

    Gtk::Box* videoTopLevelBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));

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
    videoIPAddressEntry->set_text("192.168.1.6");
    videoConnectButton=Gtk::manage(new Gtk::Button("Connect"));
    videoConnectButton->signal_clicked().connect(sigc::ptr_fun(&videoConnectOrDisconnect));
    videoConnectionStatusLabel=Gtk::manage(new Gtk::Label("Not Connected"));
    Gdk::RGBA red;
    red.set_rgba(1.0,0,0,1.0);
    videoConnectionStatusLabel->override_background_color(red);
    
    Gtk::Box* videoStateBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2));
    videoStreamButton=Gtk::manage(new Gtk::Button("Not Video Streaming"));
    videoStreamButton->signal_clicked().connect(sigc::ptr_fun(&videoStream));

    videoAddressListBox->set_size_request(200,100);
    videoScrolledList->set_size_request(200,100);

    videoConnectBox->add(*videoIPAddress);
    videoConnectBox->add(*videoIPAddressEntry);
    videoConnectBox->add(*videoConnectButton);
    videoConnectBox->add(*videoConnectionStatusLabel);

    videoStateBox->add(*videoStreamButton);

    videoControlsRightBox->add(*videoConnectBox);
    videoControlsRightBox->add(*videoStateBox);

    videoScrolledList->add(*videoAddressListBox);

    videoControlsBox->add(*videoScrolledList);
    videoControlsBox->add(*videoControlsRightBox);

    videoTopLevelBox->add(*videoControlsBox);
    window->add(*videoTopLevelBox);

    window->signal_delete_event().connect(sigc::ptr_fun(quit));
    window->show_all();

}


struct RemoteRobot{
    std::string tag;
    time_t lastSeenTime;
};
std::vector<RemoteRobot> videoRobotList;


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
        if(read(sd, databuf, datalen) >= 0) {
            std::string message(databuf);
            if(!contains(videoRobotList,message)) {
                RemoteRobot remoteRobot;
                remoteRobot.tag=message; 
                time(&remoteRobot.lastSeenTime);
                videoRobotList.push_back(remoteRobot);
            }
            update(videoRobotList,message);
        }
    }
}


void adjustVideoRobotList(){
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
            videoAddressListBox->remove(*listBoxRow);
            --index;
        }
    }
}

 
int main(int argc, char** argv) { 
    Glib::RefPtr<Gtk::Application> application = Gtk::Application::create(argc, argv, "edu.uark.razorbotz");
    setupGUI(application);

    std::thread broadcastListenThread(videoBroadcastListen);

    cv::Mat img = cv::Mat::zeros(376, 672, CV_8UC1);
    int imgSize = img.total() * img.elemSize();
    uchar sockData[imgSize];
    int bytesRead=0, total = 0;

    bool running=true;
    while(running){
        adjustVideoRobotList();
    
        while(Gtk::Main::events_pending()){
            Gtk::Main::iteration();
        }
    
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
                setVideoDisconnectedState();
                isStreamingActive = false;
                break;
            }
            else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(1));
                     while(Gtk::Main::events_pending()) { Gtk::Main::iteration(); }
                     continue;
                }
                else {
                    perror("recv header error");
                    setVideoDisconnectedState();
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
            setVideoDisconnectedState();
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
                setVideoDisconnectedState();
                isStreamingActive = false;
                break;
            }
            else {
                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(1));
                     while(Gtk::Main::events_pending()) { Gtk::Main::iteration(); }
                     continue;
                 }
                 else {
                    perror("recv frame error");
                    setVideoDisconnectedState();
                    isStreamingActive = false;
                    break;
                }
            }
        }
    
    
        if (!videoConnected || !isStreamingActive) {
            continue;
        }
    
    
        if (totalFrameRead == (376 * 672 * 1) && isGray) {
            cv::Mat received_img(376, 672, CV_8UC1, frameDataBuffer.data());
            cv::Mat display_img;
            cv::resize(received_img, display_img, cv::Size(1400, 800), cv::INTER_LINEAR);
            cv::imshow("Video", display_img);
            cv::waitKey(10); 
        }
        else if (totalFrameRead == (376 * 672 * 3) && !isGray) {
            cv::Mat received_img(376, 672, CV_8UC3, frameDataBuffer.data());
            cv::Mat display_img;
            cv::resize(received_img, display_img, cv::Size(1400, 800), cv::INTER_LINEAR);
            cv::imshow("Video", display_img);
            cv::waitKey(10); 
        }
        else {
            std::cerr << "Frame size mismatch. Expected " << (376*672*1) << ", Got " << totalFrameRead << std::endl;
            setVideoDisconnectedState();
            isStreamingActive = false;
        }
    }
    return 0; 
}

