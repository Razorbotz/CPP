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

int sock = 0; 
bool connected=false;

int main(int argc, char** argv) { 
    char buffer[1024] = {0}; 
    int bytesRead=0;

    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::list<uint8_t> messageBytesList;
    uint8_t message[256];
    struct sockaddr_in serv_addr; 
    std::string hello("Hello Robot"); 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY; 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 

        printf("\n Socket creation error \n");
    } 
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
    }
    else{
        send(sock , hello.c_str() , strlen(hello.c_str()) , 0 );
        bytesRead = read( sock , buffer, 1024);
        fcntl(sock,F_SETFL, O_NONBLOCK);

    }
    int height = 720;
    int width = 640;
    cv::Mat img = cv::Mat::zeros(height,width, CV_8UC3);
    int  imgSize = img.total()*img.elemSize();
    uint8_t sockData[imgSize];
    int bytes = 0;
    while(true){
        for (int i = 0; i < imgSize; i += bytes) {
            if((bytes = recv(sock, sockData+i, imgSize-1, 0))==-1)
                std::cout << "ERROR" << std::endl;
        }

        // Assign pixel value to img
        int ptr=0;
        for (int i = 0;  i < img.rows; i++) {
            for (int j = 0; j < img.cols; j++) {                                     
                img.at<cv::Vec3b>(i,j) = cv::Vec3b(sockData[ptr+ 0],sockData[ptr+1],sockData[ptr+2]);
                ptr=ptr+3;
            }
        }
        cv::imshow("Image", img);
        cv::waitKey(50);
    }
    return 0; 
}

