#include "NetworkHandler.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <list>
#include <string>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <unordered_map>
#include <deque>
#include <cstring>
#include <cerrno>
#include <poll.h>
#include <thread>

// GTK and System includes
#include <gtkmm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>

// OpenCV / FFmpeg
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

extern double GUI_SCALE;

static std::mutex g_cap_mtx;
static std::deque<CapturedUdpPacket> g_cap_q;
static constexpr size_t g_cap_max = 50;

static inline uint64_t steady_now_ms() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void capture_udp_payload(const uint8_t* data, size_t len, bool from_orin) {
    if (!data || len == 0) return;
    CapturedUdpPacket p;
    p.t_ms = steady_now_ms();
    p.from_orin = from_orin;
    p.bytes.assign(data, data + len);
    std::lock_guard<std::mutex> lk(g_cap_mtx);
    g_cap_q.push_back(std::move(p));
    while (g_cap_q.size() > g_cap_max) g_cap_q.pop_front();
}

// Accessors used by the --encode_tool window
bool getLatestCapturedUdpPacket(CapturedUdpPacket& out) {
    std::lock_guard<std::mutex> lk(g_cap_mtx);
    if (g_cap_q.empty()) return false;
    out = g_cap_q.back();
    return true;
}

std::vector<CapturedUdpPacket> getCapturedUdpPacketsSnapshot() {
    std::lock_guard<std::mutex> lk(g_cap_mtx);
    return std::vector<CapturedUdpPacket>(g_cap_q.begin(), g_cap_q.end());
}

// --- Helpers ---
static inline void set_nonblocking(int fd) {
    if (fd <= 0) return;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static inline void close_udp_socket(int& fd) {
    if (fd > 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        fd = -1;
    }
}

// --- Forwarding / Flight Engineer Globals ---
int forwardSock = -1;
int forwardVideoSock = -1; 
struct sockaddr_in fe_addr;
struct sockaddr_in fe_video_addr; 
bool isForwarding = false;
bool isFlightEngineerMode = false;

// Split screen matrices for FE Mode
cv::Mat fe_left_frame;
cv::Mat fe_right_frame;

void setupForwarding(const std::string& fe_ip, int fe_port, int fe_video_port) {
    // 1. Setup Telemetry Forwarding
    forwardSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (forwardSock < 0) {
        perror("Forwarding telemetry socket creation failed");
    } else {
        memset(&fe_addr, 0, sizeof(fe_addr));
        fe_addr.sin_family = AF_INET;
        fe_addr.sin_port = htons(fe_port);
        inet_pton(AF_INET, fe_ip.c_str(), &fe_addr.sin_addr);
    }
    
    // 2. Setup Video Forwarding
    forwardVideoSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (forwardVideoSock < 0) {
        perror("Forwarding video socket creation failed");
    } else {
        memset(&fe_video_addr, 0, sizeof(fe_video_addr));
        fe_video_addr.sin_family = AF_INET;
        fe_video_addr.sin_port = htons(fe_video_port);
        inet_pton(AF_INET, fe_ip.c_str(), &fe_video_addr.sin_addr);
    }

    isForwarding = true;
    std::cout << "Forwarding telemetry to " << fe_ip << ":" << fe_port 
              << " and video to " << fe_ip << ":" << fe_video_port << "\n";
}

// --- Main Robot Server Globals & Implementation ---
int sock = -1;
int sock2 = -1;
bool connected = false;
bool silentRunning = true;
bool initialized = false;
bool connected2 = false;
bool silentRunning2 = true;
bool initialized2 = false;

struct sockaddr_in serv_addr;
struct sockaddr_in serv_addr2;
socklen_t addr_len = sizeof(serv_addr);
socklen_t addr_len2 = sizeof(serv_addr2);

void setupPassiveListening(int port1, int port2, int video_port1, int video_port2) {
    isFlightEngineerMode = true;

    auto bind_port = [](int& fd, int port) {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) return;
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        set_nonblocking(fd);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("FE bind failed");
        } else {
            std::cout << "FE passively listening on port " << port << "\n";
        }
    };

    // Close any existing sockets before rebinding
    close_udp_socket(sock);
    close_udp_socket(sock2);
    extern int videoSock; 
    extern int videoSock2;
    close_udp_socket(videoSock);
    close_udp_socket(videoSock2);

    // Bind Telemetry Ports
    bind_port(sock, port1);
    bind_port(sock2, port2);
    
    // Bind Video Ports
    bind_port(videoSock, video_port1);
    bind_port(videoSock2, video_port2);
    
    // Trick the state machines into allowing the receive loops to run freely
    connected = true;
    connected2 = true;
    initialized = true;
    initialized2 = true;
    
    extern bool videoConnected;
    extern bool isStreamingActive;
    videoConnected = true;
    isStreamingActive = true; 
}


std::atomic<std::chrono::high_resolution_clock::time_point> last_rx_orin_ms{};
std::atomic<std::chrono::high_resolution_clock::time_point> last_rx_nano_ms{};

std::atomic<bool> orin_ip_known{false};
std::atomic<bool> nano_ip_known{false};
in_addr orin_ip{};
in_addr nano_ip{};

std::chrono::high_resolution_clock::time_point lastPacketOrinMs() {
    return last_rx_orin_ms.load(std::memory_order_relaxed);
}

std::chrono::high_resolution_clock::time_point lastPacketNanoMs() {
    return last_rx_nano_ms.load(std::memory_order_relaxed);
}

std::chrono::high_resolution_clock::time_point lastHeartbeatTime;
std::chrono::high_resolution_clock::time_point lastHeartbeatTime2;

struct RemoteRobot {
    std::string tag;
    time_t lastSeenTime;
};
std::vector<RemoteRobot> robotList;
std::mutex robotListMutex;

#define PORT 31337
#define ORIN_IP "192.168.1.6"
#define NANO_IP "192.168.1.5"

// State Accessors
bool isServerConnected() { return connected; }
bool isServerInitialized() { return initialized; }
bool isSilentRunning() { return silentRunning; }

bool isServerConnected2() { return connected2; }
bool isServerInitialized2() { return initialized2; }
bool isSilentRunning2() { return silentRunning2; }

static inline void send_to_both(const uint8_t* buf, size_t len) {
    if (isFlightEngineerMode) return; // FE never transmits control data

    if (connected && sock > 0) {
        sendto(sock, buf, len, 0, (struct sockaddr *)&serv_addr, addr_len);
    }
    if (connected2 && sock2 > 0) {
        sendto(sock2, buf, len, 0, (struct sockaddr *)&serv_addr2, addr_len2);
    }
}

void setDisconnectedState(ServerUI& ui) {
    ui.connectButton->set_label("Connect");
    ui.connectionStatusLabel->set_text("Not Connected");
    ui.silentRunButton->set_label("Silent Running");
    Gdk::RGBA red;
    red.set_rgba(1.0, 0, 0, 1.0);
    ui.connectionStatusLabel->override_background_color(red);
    ui.ipAddressEntry->set_can_focus(true);
    ui.ipAddressEntry->set_editable(true);

    connected = false;
    silentRunning = true;
    initialized = false;
}

void setConnectedState(ServerUI& ui) {
    ui.connectButton->set_label("Disconnect");
    ui.connectionStatusLabel->set_text("Connected");
    Gdk::RGBA green;
    green.set_rgba(0, 1.0, 0, 1.0);
    ui.connectionStatusLabel->override_background_color(green);
    ui.ipAddressEntry->set_can_focus(false);
    ui.ipAddressEntry->set_editable(false);
    connected = true;
}

void disconnectFromServer(ServerUI& ui) {
    Gtk::MessageDialog dialog(*ui.parentWindow, "Disconnect now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
    if (dialog.run() == Gtk::RESPONSE_OK) {
        close_udp_socket(sock);
        setDisconnectedState(ui);
    }
}

void setDisconnectedState2(ServerUI& ui) {
    ui.connectButton2->set_label("Connect");
    ui.connectionStatusLabel2->set_text("Not Connected");
    ui.silentRunButton2->set_label("Silent Running");
    Gdk::RGBA red;
    red.set_rgba(1.0, 0, 0, 1.0);
    ui.connectionStatusLabel2->override_background_color(red);
    ui.ipAddressEntry2->set_can_focus(true);
    ui.ipAddressEntry2->set_editable(true);

    connected2 = false;
    silentRunning2 = true;
    initialized2 = false;
}

void setConnectedState2(ServerUI& ui) {
    ui.connectButton2->set_label("Disconnect");
    ui.connectionStatusLabel2->set_text("Connected");
    Gdk::RGBA green;
    green.set_rgba(0, 1.0, 0, 1.0);
    ui.connectionStatusLabel2->override_background_color(green);
    ui.ipAddressEntry2->set_can_focus(false);
    ui.ipAddressEntry2->set_editable(false);
    connected2 = true;
}

void disconnectFromServer2(ServerUI& ui) {
    Gtk::MessageDialog dialog(*ui.parentWindow, "Disconnect now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
    if (dialog.run() == Gtk::RESPONSE_OK) {
        close_udp_socket(sock2);
        setDisconnectedState2(ui);
    }
}

enum class ConnStatus { PENDING, SUCCESS, FAILURE };
std::atomic<ConnStatus> connection_status = ConnStatus::PENDING;
std::atomic<ConnStatus> connection_status2 = ConnStatus::PENDING;

void connectToServer(ServerUI& ui, bool useOrin, Glib::Dispatcher& dispatcher) {
    if (isFlightEngineerMode) return; // Block manual connections in FE mode
    const bool is_orin = useOrin;

    int& sock_ref = is_orin ? sock : sock2;
    sockaddr_in& addr_ref = is_orin ? serv_addr : serv_addr2;
    socklen_t& addr_len_ref = is_orin ? addr_len : addr_len2;
    Gtk::Entry* ip_entry = is_orin ? ui.ipAddressEntry : ui.ipAddressEntry2;

    auto fail = [&]() {
        close_udp_socket(sock_ref);
        if (is_orin) connection_status = ConnStatus::FAILURE;
        else         connection_status2 = ConnStatus::FAILURE;
        dispatcher.emit();
    };

    if (!ip_entry) {
        fail();
        return;
    }

    std::memset(&addr_ref, 0, sizeof(addr_ref));
    addr_ref.sin_family = AF_INET;
    addr_ref.sin_port   = htons(PORT);

    const std::string ip_str = ip_entry->get_text();
    if (::inet_pton(AF_INET, ip_str.c_str(), &addr_ref.sin_addr) != 1) {
        std::cerr << "Invalid IP Address: " << ip_str << "\n";
        fail();
        return;
    }

    if (is_orin) {
        orin_ip = addr_ref.sin_addr;
        orin_ip_known.store(true, std::memory_order_relaxed);
    }
    else {
        nano_ip = addr_ref.sin_addr;
        nano_ip_known.store(true, std::memory_order_relaxed);
    }

    addr_len_ref = sizeof(addr_ref);

    close_udp_socket(sock_ref);

    sock_ref = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ref < 0) {
        perror("Socket creation error");
        fail();
        return;
    }

    set_nonblocking(sock_ref);

    // Send hello to the destination
    const char* hello = "Hello Robot";
    (void)::sendto(sock_ref, hello, std::strlen(hello), 0,
                   (struct sockaddr*)&addr_ref, addr_len_ref);

    // --- Wait up to 2 seconds for a reply ---
    const auto start = std::chrono::high_resolution_clock::now();
    char buffer[1024];

    while (std::chrono::high_resolution_clock::now() - start < std::chrono::seconds(2)) {
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);

        int n = ::recvfrom(sock_ref, buffer, sizeof(buffer), 0,
                           (struct sockaddr*)&from, &from_len);

        if (n > 0) {
            if (from.sin_addr.s_addr != addr_ref.sin_addr.s_addr) {
                char from_ip[INET_ADDRSTRLEN]{};
                ::inet_ntop(AF_INET, &from.sin_addr, from_ip, sizeof(from_ip));
                std::cerr << "Warning: reply from unexpected IP: " << from_ip << "\n";
            }

            std::cout << "Received reply from server. Connection established.\n";

            const auto now = std::chrono::high_resolution_clock::now();

            if (is_orin) {
                connection_status = ConnStatus::SUCCESS;
                lastHeartbeatTime = now;                    
                last_rx_orin_ms    = now;                   
            }
            else {
                connection_status2 = ConnStatus::SUCCESS;
                lastHeartbeatTime2 = now;
                last_rx_nano_ms    = now;
            }

            dispatcher.emit();
            return;
        }

        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "recvfrom(connect) error: " << std::strerror(errno) << "\n";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cerr << "Connection timed out.\n";
    fail();
}

void update_connection_status(ServerUI& ui) {
    ConnStatus status = connection_status;
    if (status == ConnStatus::SUCCESS) {
        setConnectedState(ui);
        initialized = true;
        lastHeartbeatTime = std::chrono::high_resolution_clock::now();
    }
    else if (status == ConnStatus::FAILURE) {
        close_udp_socket(sock);
        setDisconnectedState(ui);
    }
    ui.connectButton->set_sensitive(true);
}

void update_connection_status2(ServerUI& ui) {
    ConnStatus status = connection_status2;
    if (status == ConnStatus::SUCCESS) {
        setConnectedState2(ui);
        initialized2 = true;
        lastHeartbeatTime2 = std::chrono::high_resolution_clock::now();
    }
    else if (status == ConnStatus::FAILURE) {
        close_udp_socket(sock2);
        setDisconnectedState2(ui);
    }
    ui.connectButton2->set_sensitive(true);
}

void connectOrDisconnect(ServerUI& ui, bool useOrin, Glib::Dispatcher& dispatcher) {
    if (isFlightEngineerMode) return;
    const bool is_orin = useOrin;

    Gtk::Button* btn = is_orin ? ui.connectButton : ui.connectButton2;
    Gtk::Label* lbl = is_orin ? ui.connectionStatusLabel : ui.connectionStatusLabel2;

    if (!btn || !lbl) return;

    if (btn->get_label() == "Connect") {
        btn->set_sensitive(false);
        lbl->set_text("Connecting...");

        std::thread conn_thread(connectToServer, std::ref(ui), is_orin, std::ref(dispatcher));
        conn_thread.detach();
    }
    else {
        if (is_orin) disconnectFromServer(ui);
        else        disconnectFromServer2(ui);
    }
}

void connectOrDisconnect2(ServerUI& ui, bool useOrin, Glib::Dispatcher& dispatcher) {
    connectOrDisconnect(ui, useOrin, dispatcher);
}

namespace {
    bool contains(const std::vector<std::string>& list, const std::string& value) {
        for (const std::string& storedValue : list) {
            if (storedValue == value) return true;
        }
        return false;
    }

    std::vector<std::string> getAddressList() {
        std::vector<std::string> addressList;
        ifaddrs* interfaceAddresses = nullptr;
        if (getifaddrs(&interfaceAddresses) == 0) {
            for (ifaddrs* interface = interfaceAddresses; interface != nullptr; interface = interface->ifa_next) {
                if (interface->ifa_addr != nullptr && interface->ifa_addr->sa_family == AF_INET) {
                    sockaddr_in* socketAddress = reinterpret_cast<sockaddr_in*>(interface->ifa_addr);
                    std::string addressString(inet_ntoa(socketAddress->sin_addr));
                    if (addressString != "0.0.0.0" && addressString != "127.0.0.1" && !contains(addressList, addressString)) {
                        addressList.push_back(addressString);
                    }
                }
            }
            freeifaddrs(interfaceAddresses);
        }
        return addressList;
    }
}

void silentRun(ServerUI& ui) {
    if (!connected || !ui.silentRunButton || isFlightEngineerMode) return;

    std::string currentButtonState = ui.silentRunButton->get_label();

    uint8_t message[3];
    message[0] = 3;  // messageSize
    message[1] = 7;  // command (silence)

    if (currentButtonState == "Silent Running") {
        message[2] = 0; // Not silent
        sendto(sock, message, sizeof(message), 0, (struct sockaddr *)&serv_addr, addr_len);
        ui.silentRunButton->set_label("Not Silent Running");
        silentRunning = false;
    }
    else {
        message[2] = 1; // Silent
        sendto(sock, message, sizeof(message), 0, (struct sockaddr *)&serv_addr, addr_len);
        ui.silentRunButton->set_label("Silent Running");
        silentRunning = true;
    }
}

void silentRun2(ServerUI& ui) {
    if (!connected2 || !ui.silentRunButton2 || isFlightEngineerMode) return;

    std::string currentButtonState = ui.silentRunButton2->get_label();

    uint8_t message[3];
    message[0] = 3;  // messageSize
    message[1] = 7;  // command (silence)

    if (currentButtonState == "Silent Running") {
        message[2] = 0; // Not silent
        sendto(sock2, message, sizeof(message), 0, (struct sockaddr *)&serv_addr2, addr_len2);
        ui.silentRunButton2->set_label("Not Silent Running");
        silentRunning2 = false;
    }
    else {
        message[2] = 1; // Silent
        sendto(sock2, message, sizeof(message), 0, (struct sockaddr *)&serv_addr2, addr_len2);
        ui.silentRunButton2->set_label("Silent Running");
        silentRunning2 = true;
    }
}

void rowActivated(Gtk::ListBoxRow* listBoxRow, ServerUI& ui) {
    auto label = static_cast<Gtk::Label*>(listBoxRow->get_child());
    Glib::ustring connectionString(label->get_text());

    size_t index = connectionString.rfind('@');
    if (index == Glib::ustring::npos) return;

    Glib::ustring addressString = connectionString.substr(index + 1);
    ui.ipAddressEntry->set_text(addressString);
}

static void shutdownRobot() {
    if (isFlightEngineerMode) return;
    uint8_t message[2];
    message[0] = 2; // messageSize
    message[1] = 8; // command (shutdown)
    send_to_both(message, sizeof(message));
}

void shutdownDialog(Gtk::Window* parentWindow) {
    Gtk::MessageDialog dialog(*parentWindow, "Shutdown now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
    if (dialog.run() == Gtk::RESPONSE_OK) {
        shutdownRobot();
    }
}

void broadcastListen() {
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        perror("Opening datagram socket error");
        return;
    }

    int reuse = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR error");
        close(sd);
        return;
    }

    struct sockaddr_in localSock;
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(4321);
    localSock.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
        perror("Binding datagram socket error");
        close(sd);
        return;
    }

    std::vector<std::string> addressList = getAddressList();
    for (const std::string& addressString : addressList) {
        struct ip_mreq group;
        group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
        group.imr_interface.s_addr = inet_addr(addressString.c_str());
        if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
            perror("Adding multicast group error");
        }
    }

    char databuf[2048];
    while (true) {
        ssize_t bytesRead = read(sd, databuf, sizeof(databuf));
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
                robotList.push_back({message, time(nullptr)});
            }
        }
    }
}

void adjustRobotList(Gtk::ListBox* addressListBox) {
    if (!addressListBox) return;
    std::lock_guard<std::mutex> lock(robotListMutex);
    time_t now = time(nullptr);
    std::vector<std::string> robots_in_gui;
    
    for (auto* child : addressListBox->get_children()) {
        if (auto* row = dynamic_cast<Gtk::ListBoxRow*>(child)) {
            auto* label = static_cast<Gtk::Label*>(row->get_child());
            robots_in_gui.push_back(label->get_text());
        }
    }

    robotList.erase(std::remove_if(robotList.begin(), robotList.end(),
        [&](const RemoteRobot& robot) {
            if (now - robot.lastSeenTime > 12) {
                for (auto* child : addressListBox->get_children()) {
                     if (auto* row = dynamic_cast<Gtk::ListBoxRow*>(child)) {
                        auto* label = static_cast<Gtk::Label*>(row->get_child());
                        if (label->get_text() == robot.tag) {
                            addressListBox->remove(*row);
                            break;
                        }
                    }
                }
                return true; 
            }
            return false;
        }),
        robotList.end());

    for (const auto& robot : robotList) {
        if (std::find(robots_in_gui.begin(), robots_in_gui.end(), robot.tag) == robots_in_gui.end()) {
            addressListBox->append(*Gtk::manage(new Gtk::Label(robot.tag)));
        }
    }
    addressListBox->show_all();
}

// --- Video Server Globals & Implementation ---

int videoSock = -1;
int videoSock2 = -1; 
bool videoConnected = false;
bool isStreamingActive = false;
struct sockaddr_in video_serv_addr;
socklen_t video_addr_len = sizeof(video_serv_addr);
std::chrono::high_resolution_clock::time_point last_packet_time;

std::vector<RemoteRobot> videoRobotList;
std::mutex videoRobotListMutex;

#define VIDEO_PORT 31338

// State Accessors
bool isVideoConnected() { return videoConnected; }
bool isVideoStreamActive() { return isStreamingActive; }

// --- State Update Functions ---
void setVideoConnectedState(VideoServerUI& ui) {
    ui.connectButton->set_label("Disconnect");
    ui.connectionStatusLabel->set_text("Connected");
    Gdk::RGBA green;
    green.set_rgba(0, 1.0, 0, 1.0);
    ui.connectionStatusLabel->override_background_color(green);
    ui.ipAddressEntry->set_can_focus(false);
    ui.ipAddressEntry->set_editable(false);
    videoConnected = true;
}

void setVideoDisconnectedState(VideoServerUI& ui) {
    ui.connectButton->set_label("Connect");
    ui.connectionStatusLabel->set_text("Not Connected");
    ui.streamButton->set_label("Not Video Streaming");
    Gdk::RGBA red;
    red.set_rgba(1.0, 0, 0, 1.0);
    ui.connectionStatusLabel->override_background_color(red);
    ui.ipAddressEntry->set_can_focus(true);
    ui.ipAddressEntry->set_editable(true);
    videoConnected = false;
    isStreamingActive = false;
}

void handleVideoDisconnect(VideoServerUI& ui) {
    setVideoDisconnectedState(ui);
}

std::atomic<ConnStatus> video_connection_status = ConnStatus::PENDING;

// --- Connection Logic ---
static void connectToVideoServer(VideoServerUI& ui, Glib::Dispatcher& dispatcher) {
    if (videoConnected || isFlightEngineerMode) return;

    memset(&video_serv_addr, 0, sizeof(video_serv_addr));
    video_serv_addr.sin_family = AF_INET;
    video_serv_addr.sin_port   = htons(VIDEO_PORT);

    if (inet_pton(AF_INET, ui.ipAddressEntry->get_text().c_str(), &video_serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid Video IP Address" << std::endl;
        video_connection_status = ConnStatus::FAILURE;
        dispatcher.emit();
        return;
    }

    close_udp_socket(videoSock);

    if ((videoSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("\n Video UDP socket creation error \n");
        video_connection_status = ConnStatus::FAILURE;
        dispatcher.emit();
        return;
    }

    set_nonblocking(videoSock);

    std::string hello("Hello Robot");
    sendto(videoSock, hello.c_str(), hello.length(), 0, (struct sockaddr *)&video_serv_addr, video_addr_len);

    auto startTime = std::chrono::high_resolution_clock::now();
    char buffer[1024];

    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::high_resolution_clock::now() - startTime).count() < 4) {

        socklen_t from_len = video_addr_len;
        ssize_t n = recvfrom(videoSock, buffer, sizeof(buffer), 0, (struct sockaddr *)&video_serv_addr, &from_len);

        if (n > 0) {
            std::cout << "Received reply from video server. Connection established." << std::endl;
            video_connection_status = ConnStatus::SUCCESS;
            dispatcher.emit();
            videoConnected = true;
            return;
        }

        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "recvfrom(video connect) error: " << strerror(errno) << "\n";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "Connection to video server failed (timeout)." << std::endl;
    close_udp_socket(videoSock);
    video_connection_status = ConnStatus::FAILURE;
    dispatcher.emit();
}

void update_video_connection_status(VideoServerUI& ui) {
    ConnStatus status = video_connection_status;
    if (status == ConnStatus::SUCCESS) {
        setVideoConnectedState(ui);
    }
    else if (status == ConnStatus::FAILURE) {
        setVideoDisconnectedState(ui);
    }
    ui.connectButton->set_sensitive(true);
}

static void disconnectFromVideoServer(VideoServerUI& ui) {
    Gtk::MessageDialog dialog(*ui.parentWindow, "Disconnect from video server?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
    if (dialog.run() == Gtk::RESPONSE_OK) {
        close_udp_socket(videoSock);
        setVideoDisconnectedState(ui);
    }
}

void videoConnectOrDisconnect(VideoServerUI& ui, Glib::Dispatcher& dispatcher) {
    if (isFlightEngineerMode) return;
    if (ui.connectButton->get_label() == "Connect") {
        ui.connectButton->set_sensitive(false);
        ui.connectionStatusLabel->set_text("Connecting...");

        std::thread conn_thread(connectToVideoServer, std::ref(ui), std::ref(dispatcher));
        conn_thread.detach();
    }
    else {
        disconnectFromVideoServer(ui);
    }
}

// --- UI Interaction Functions ---
void videoStream(VideoServerUI& ui) {
    if (!videoConnected || isFlightEngineerMode) return; // Guard FE
    
    uint8_t message[3];
    message[0] = 3;
    message[1] = 1;

    if (ui.streamButton->get_label() == "Not Video Streaming") {
        message[2] = 1;
        ui.streamButton->set_label("Video Streaming");
        isStreamingActive = true;
        last_packet_time = std::chrono::high_resolution_clock::now();
    }
    else {
        message[2] = 0;
        ui.streamButton->set_label("Not Video Streaming");
        isStreamingActive = false;
    }
    sendto(videoSock, message, sizeof(message), 0, (struct sockaddr *)&video_serv_addr, video_addr_len);
}

void videoRowActivated(Gtk::ListBoxRow* listBoxRow, VideoServerUI& ui) {
    auto label = static_cast<Gtk::Label*>(listBoxRow->get_child());
    Glib::ustring connectionString(label->get_text());

    size_t index = connectionString.rfind('@');
    if (index == Glib::ustring::npos) return;

    Glib::ustring addressString = connectionString.substr(index + 1);
    ui.ipAddressEntry->set_text(addressString);
}

// --- Background Threads ---

void videoBroadcastListen() {
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        perror("Opening video datagram socket error");
        return;
    }

    int reuse = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR for video error");
        close(sd);
        return;
    }

    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    struct sockaddr_in localSock;
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(4322); // Video broadcast port
    localSock.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
        perror("Binding video datagram socket error");
        close(sd);
        return;
    }

    /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */
    std::vector<std::string> addressList = getAddressList(); 
    for (const std::string& addressString : addressList) {
        struct ip_mreq group;
        group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
        group.imr_interface.s_addr = inet_addr(addressString.c_str());
        if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
            perror("Adding multicast group for video error");
        }
    }

    char databuf[1024];
    while (true) {
        ssize_t bytesRead = read(sd, databuf, sizeof(databuf));
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
                videoRobotList.push_back({message, time(nullptr)});
            }
        }
    }
}

void adjustVideoRobotList(Gtk::ListBox* videoAddressListBox) {
    if (!videoAddressListBox) return;
    std::lock_guard<std::mutex> lock(videoRobotListMutex);
    time_t now = time(nullptr);
    std::vector<std::string> robots_in_gui;

    for (auto* child : videoAddressListBox->get_children()) {
        if (auto* row = dynamic_cast<Gtk::ListBoxRow*>(child)) {
            robots_in_gui.push_back(static_cast<Gtk::Label*>(row->get_child())->get_text());
        }
    }

    videoRobotList.erase(std::remove_if(videoRobotList.begin(), videoRobotList.end(),
        [&](const RemoteRobot& robot) {
            if (now - robot.lastSeenTime > 12) {
                for (auto* child : videoAddressListBox->get_children()) {
                     if (auto* row = dynamic_cast<Gtk::ListBoxRow*>(child)) {
                        if (static_cast<Gtk::Label*>(row->get_child())->get_text() == robot.tag) {
                            videoAddressListBox->remove(*row);
                            break;
                        }
                    }
                }
                return true;
            }
            return false;
        }),
        videoRobotList.end());

    for (const auto& robot : videoRobotList) {
        if (std::find(robots_in_gui.begin(), robots_in_gui.end(), robot.tag) == robots_in_gui.end()) {
            videoAddressListBox->append(*Gtk::manage(new Gtk::Label(robot.tag)));
        }
    }
    videoAddressListBox->show_all();
}

struct FrameChunkHeader {
    uint16_t frame_id;
    uint16_t chunk_index;
    uint16_t total_chunks;
} __attribute__((packed));


// Decoded separately for each stream to prevent frame merging issues
void videoDecoderThread(int& targetSock, int side, cv::Mat& latestFrame, std::mutex& frameMutex, std::atomic<bool>& newFrameAvailable, Glib::Dispatcher& videoDisconnectDispatcher, std::atomic<bool>& shouldVideoDisconnect) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "H.264 decoder not found" << std::endl;
        return;
    }

    AVCodecParserContext* parser = av_parser_init(codec->id);
    if (!parser) {
        std::cerr << "Failed to initialize H.264 parser" << std::endl;
        return;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        av_parser_close(parser);
        return;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        avcodec_free_context(&codec_ctx);
        av_parser_close(parser);
        return;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* bgr_frame = av_frame_alloc();
    SwsContext* sws_ctx = nullptr;
    uint8_t* bgr_buffer = nullptr;

    std::vector<uint8_t> frameDataBuffer(1000000);
    
    // Kept local to thread to prevent collision between multiple streams
    std::unordered_map<uint16_t, std::vector<std::vector<uint8_t>>> frameChunks;

    bool running = true;
    while (running) {
        if ((!videoConnected || !isStreamingActive) && !isFlightEngineerMode) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_packet_time).count() >= 3 && !isFlightEngineerMode) {
            std::cerr << "Video stream timed out." << std::endl;
            isStreamingActive = false;
            shouldVideoDisconnect = true;
            videoDisconnectDispatcher.emit();
            continue;
        }

        ssize_t bytesRead = recvfrom(targetSock, frameDataBuffer.data(), frameDataBuffer.size(), 0, NULL, NULL);
        if (bytesRead > 0) {
            last_packet_time = std::chrono::high_resolution_clock::now();
            
            // --- FE VIDEO PACKET FORWARDING (Only executes on Pilot client) ---
            if (isForwarding && forwardVideoSock > 0) {
                sendto(forwardVideoSock, frameDataBuffer.data(), bytesRead, 0, 
                       (struct sockaddr*)&fe_video_addr, sizeof(fe_video_addr));
            }
            // ------------------------------------------------------------------
        }
        if (bytesRead < (ssize_t)sizeof(FrameChunkHeader))
            continue;

        FrameChunkHeader hdr;
        memcpy(&hdr, frameDataBuffer.data(), sizeof(hdr));
        hdr.frame_id = ntohs(hdr.frame_id);
        hdr.chunk_index = ntohs(hdr.chunk_index);
        hdr.total_chunks = ntohs(hdr.total_chunks);

        std::vector<uint8_t> chunk(frameDataBuffer.begin() + sizeof(hdr),
                                  frameDataBuffer.begin() + bytesRead);

        frameChunks[hdr.frame_id].resize(hdr.total_chunks);
        frameChunks[hdr.frame_id][hdr.chunk_index] = std::move(chunk);

        bool complete = true;
        for (size_t i = 0; i < hdr.total_chunks; ++i) {
            if (frameChunks[hdr.frame_id][i].empty()) {
                complete = false;
                break;
            }
        }

        if (complete) {
            std::vector<uint8_t> fullFrame;
            for (auto &c : frameChunks[hdr.frame_id])
                fullFrame.insert(fullFrame.end(), c.begin(), c.end());

            frameChunks.erase(hdr.frame_id);

            uint8_t* data_ptr = fullFrame.data();
            size_t data_size = fullFrame.size();

            while (data_size > 0) {
                int ret = av_parser_parse2(parser, codec_ctx, &pkt->data, &pkt->size,
                                          data_ptr, data_size,
                                          AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
                if (ret < 0) break;
                data_ptr += ret;
                data_size -= ret;

                if (pkt->size && avcodec_send_packet(codec_ctx, pkt) >= 0) {
                    while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                        if (!sws_ctx) {
                            sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                                     codec_ctx->width, codec_ctx->height, AV_PIX_FMT_GRAY8,
                                                     SWS_BILINEAR, NULL, NULL, NULL);
                            int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, codec_ctx->width, codec_ctx->height, 32);
                            bgr_buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
                            av_image_fill_arrays(bgr_frame->data, bgr_frame->linesize, bgr_buffer, AV_PIX_FMT_GRAY8, codec_ctx->width, codec_ctx->height, 32);
                        }

                        sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize, 0, codec_ctx->height,
                                  bgr_frame->data, bgr_frame->linesize);

                        cv::Mat decoded_mat(codec_ctx->height, codec_ctx->width, CV_8UC1, bgr_frame->data[0], bgr_frame->linesize[0]);

                        // If in FE mode, resize each frame to half width for split screen
                        int target_width = (isFlightEngineerMode) ? (800 * GUI_SCALE) : (1600 * GUI_SCALE);
                        int target_height = 1000 * GUI_SCALE;

                        cv::Mat display_img;
                        cv::resize(decoded_mat, display_img, cv::Size(target_width, target_height), 0, 0, cv::INTER_LINEAR);

                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            
                            // FE Mode Split Screen Sticher
                            if (isFlightEngineerMode) {
                                if (side == 1) fe_left_frame = display_img.clone();
                                else fe_right_frame = display_img.clone();

                                if (!fe_left_frame.empty() && !fe_right_frame.empty()) {
                                    cv::hconcat(fe_left_frame, fe_right_frame, latestFrame);
                                } else if (!fe_left_frame.empty()) {
                                    latestFrame = fe_left_frame.clone();
                                } else if (!fe_right_frame.empty()) {
                                    latestFrame = fe_right_frame.clone();
                                }
                            } 
                            // Pilot Mode Full Screen
                            else {
                                latestFrame = display_img.clone();
                            }
                            
                            newFrameAvailable = true;
                        }
                    }
                }
            }
            av_packet_unref(pkt);
        }
    }

    if (sws_ctx) sws_freeContext(sws_ctx);
    if (bgr_buffer) av_freep(&bgr_buffer);
    av_frame_free(&bgr_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
}

void videoMain(cv::Mat& latestFrame, std::mutex& frameMutex, std::atomic<bool>& newFrameAvailable, Glib::Dispatcher& videoDisconnectDispatcher, std::atomic<bool>& shouldVideoDisconnect) {
    if (isFlightEngineerMode) {
        // Spawn two decoder threads for both FE video sockets
        std::thread t1(videoDecoderThread, std::ref(videoSock), 1, std::ref(latestFrame), std::ref(frameMutex), std::ref(newFrameAvailable), std::ref(videoDisconnectDispatcher), std::ref(shouldVideoDisconnect));
        std::thread t2(videoDecoderThread, std::ref(videoSock2), 2, std::ref(latestFrame), std::ref(frameMutex), std::ref(newFrameAvailable), std::ref(videoDisconnectDispatcher), std::ref(shouldVideoDisconnect));
        t1.join();
        t2.join();
    } else {
        // Standard Pilot decoding
        videoDecoderThread(videoSock, 0, latestFrame, frameMutex, newFrameAvailable, videoDisconnectDispatcher, shouldVideoDisconnect);
    }
}

// --- Functions to send data  ---
void insert_float(float value, uint8_t* array) {
    uint32_t as_int;
    static_assert(sizeof(float) == sizeof(uint32_t), "float size unexpected");
    memcpy(&as_int, &value, sizeof(uint32_t));
    array[0] = (as_int >> 24) & 0xff;
    array[1] = (as_int >> 16) & 0xff;
    array[2] = (as_int >> 8) & 0xff;
    array[3] = (as_int >> 0) & 0xff;
}

void sendJoystickAxis(uint8_t which, uint8_t axis, float value) {
    if (!connected && !connected2) return;
    uint8_t command = 1;
    int length = 8;
    uint8_t message[8];
    message[0] = length;
    message[1] = command;
    message[2] = which;
    message[3] = axis;
    insert_float(value, &message[4]);
    send_to_both(message, length);
}

int receiveRobotData(std::vector<uint8_t>& buffer) {
    if (!connected && !connected2) return -1;

    char recv_buffer[16384];

    struct pollfd fds[2];
    int nfds = 0;

    auto add_fd = [&](int fd){
        fds[nfds].fd = fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    };

    if (connected && sock > 0)  add_fd(sock);
    if (connected2 && sock2 > 0) add_fd(sock2);
    if (nfds == 0) return -1;

    int pr = ::poll(fds, nfds, 1);
    if (pr <= 0) return 0;

    bool got_any = false;
    int got_n = 0;

    auto recv_and_stamp = [&](int fd) {
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);

        int n = ::recvfrom(fd, recv_buffer, sizeof(recv_buffer), 0,
                           (struct sockaddr*)&from, &from_len);
        if (n <= 0) return;

        // --- FE PACKET FORWARDING ---
        if (isForwarding && forwardSock > 0) {
            sendto(forwardSock, recv_buffer, n, 0, (struct sockaddr*)&fe_addr, sizeof(fe_addr));
        }
        // ----------------------------

        std::chrono::high_resolution_clock::time_point t = std::chrono::high_resolution_clock::now();
        if (from.sin_addr.s_addr == orin_ip.s_addr) last_rx_orin_ms.store(t, std::memory_order_relaxed);
        else if (from.sin_addr.s_addr == nano_ip.s_addr) last_rx_nano_ms.store(t, std::memory_order_relaxed);

        buffer.assign(recv_buffer, recv_buffer + n);
        
        bool from_orin = false;
        if (orin_ip_known.load(std::memory_order_relaxed) && from.sin_addr.s_addr == orin_ip.s_addr) from_orin = true;
        else if (nano_ip_known.load(std::memory_order_relaxed) && from.sin_addr.s_addr == nano_ip.s_addr) from_orin = false;
        else from_orin = (fd == sock);
        capture_udp_payload(reinterpret_cast<const uint8_t*>(recv_buffer), (size_t)n, from_orin);
        got_any = true;
        got_n = n;
    };

    for (int i = 0; i < nfds; ++i) {
        if (fds[i].revents & POLLIN) {
            recv_and_stamp(fds[i].fd);
        }
    }

    return got_any ? got_n : 0;
}

void sendKeyboardEvent(uint32_t keyval, uint8_t state) {
    if (!connected && !connected2) return;
    uint8_t message[5];
    message[0] = 5;
    message[1] = 2;
    message[2] = (uint8_t)((keyval >> 8) & 0xff);
    message[3] = (uint8_t)((keyval >> 0) & 0xff);
    message[4] = state;
    send_to_both(message, sizeof(message));
}

void sendJoystickButton(uint8_t which, uint8_t button, uint8_t state) {
    if (!connected && !connected2) return;
    uint8_t command = 5;
    int length = 5;
    uint8_t message[length];
    message[0] = length;
    message[1] = command;
    message[2] = which;
    message[3] = button;
    message[4] = state;
    send_to_both(message, length);
}

void sendJoystickHat(uint8_t which, uint8_t hat, uint8_t value) {
    if (!connected && !connected2) return;
    uint8_t command = 6;
    int length = 5;
    uint8_t message[length];
    message[0] = length;
    message[1] = command;
    message[2] = which;
    message[3] = hat;
    message[4] = value;
    send_to_both(message, length);
}

void sendHeartbeat() {
    if (!connected && !connected2) return;
    if (isFlightEngineerMode) return;

    auto t = std::chrono::high_resolution_clock::now();
    lastHeartbeatTime = t;
    lastHeartbeatTime2 = t;

    uint8_t message[2];
    message[0] = 2;
    message[1] = 0;
    send_to_both(message, sizeof(message));
}

void sendVideoHeartbeat() {
    if (!videoConnected) return;
    if (isFlightEngineerMode) return;
    uint8_t message[2];
    message[0] = 2;
    message[1] = 0;
    sendto(videoSock, message, sizeof(message), 0, (struct sockaddr *)&video_serv_addr, video_addr_len);
}