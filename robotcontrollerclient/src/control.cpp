#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <ifaddrs.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <SDL2/SDL.h>
#include <gdkmm.h>
#include <glibmm/ustring.h>
#include <gtkmm.h>

#include "BoxFactory.hpp"
#include "GuiBox.hpp"
#include "InfoFrame.hpp"
#include "WindowFactory.hpp"

// Port for communicating with the robot
constexpr unsigned int PORT = 31337;

// Takes a BE type as a byte array and returns that type. Will truncate if array is longer than uint32_t (4 bytes)
// This code may produce undefined behavior on specific machines/compilers since it is not standards compliant and uses bitwise/casting trickery (endianness and type size)
// TODO: Make this standards compliant and platform independent
template <typename T>
T parseType(const uint8_t* array) {
	constexpr size_t size = sizeof(uint32_t);

	uint32_t result = 0;
	for(size_t i = 0; i < size; i++)
		result |= uint32_t(array[i]) << ((size - 1 - i) * 8);

	return (T) * (static_cast<T*>(static_cast<void*>(&result)));
}

// Takes a data type and fills byte array with its data in BE form
// This code may produce undefined behavior on specific machines/compilers since it is not standards compliant and uses bitwise/casting trickery (endianness and int size)
// TODO: Make this standards compliant and platform independent; Make this have an array bounds check
template <typename T>
void insert(T value, uint8_t* array) {
	constexpr size_t valueSize = sizeof(value);
	for(size_t i = 0; i < valueSize; i++)
		array[i] = uint8_t((uint32_t(*(static_cast<uint32_t*>(static_cast<void*>(&value)))) >> ((valueSize - 1 - i) * 8)));
}

// Callback for quitting the program
bool quit(GdkEventAny* event) {
	exit(0);
	return true;
}

// GUI Element Variables
Gtk::Label* voltageLabel;
Gtk::Label* current0Label;
Gtk::Label* current1Label;
Gtk::Label* current2Label;
Gtk::Label* current3Label;
Gtk::Label* current4Label;
Gtk::Label* current5Label;
Gtk::Label* current6Label;
Gtk::Label* current7Label;
Gtk::Label* current8Label;
Gtk::Label* current9Label;
Gtk::Label* current10Label;
Gtk::Label* current11Label;
Gtk::Label* current12Label;
Gtk::Label* current13Label;
Gtk::Label* current14Label;
Gtk::Label* current15Label;

Gtk::ListBox* addressListBox;
Gtk::Entry* ipAddressEntry;
Gtk::Label* connectionStatusLabel;

Gtk::Button* silentRunButton;
Gtk::Button* connectButton;
Gtk::Label* controlModeLabel;

InfoFrame* talon1InfoFrame;
InfoFrame* talon2InfoFrame;
InfoFrame* victor1InfoFrame;
InfoFrame* victor2InfoFrame;
InfoFrame* victor3InfoFrame;

Gtk::Window* window;
int sock = 0;
bool connected = false;

// Helper function to set GUI info when robot disconnects/is not connected
void setDisconnectedState() {
	connectButton->set_label("Connect");
	connectionStatusLabel->set_text("Not Connected");
	Gdk::RGBA red;
	red.set_rgba(1.0, 0, 0, 1.0);
	connectionStatusLabel->override_background_color(red);
	ipAddressEntry->set_can_focus(true);
	ipAddressEntry->set_editable(true);
	connected = false;
}

// Helper function to set GUI info when robot connects/is connected
void setConnectedState() {
	connectButton->set_label("Disconnect");
	connectionStatusLabel->set_text("Connected");
	Gdk::RGBA green;
	green.set_rgba(0, 1.0, 0, 1.0);
	connectionStatusLabel->override_background_color(green);
	ipAddressEntry->set_can_focus(false);
	ipAddressEntry->set_editable(false);
	connected = true;
}

void connectToServer() {
	if(connected) return;
	struct sockaddr_in address {};
	size_t bytesRead;
	struct sockaddr_in serv_addr {};
	std::string hello = "Hello Robot";

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	char buffer[1024] = {0};
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");

		setDisconnectedState();
		//		  connectionStatusLabel->set_text("Socket Creation Error");
		//		  Gdk::RGBA red;
		//		  red.set_rgba(1.0,0,0,1.0);
		//		  connectionStatusLabel->override_background_color(red);
		//        ipAddressEntry->set_can_focus(true);
		//        ipAddressEntry->set_editable(true);
		//        connected=false;
		return;
	}
	if(inet_pton(AF_INET, ipAddressEntry->get_text().c_str(), &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");

		Gtk::MessageDialog dialog(*window, "Invalid Address", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK);
		int result = dialog.run();

		setDisconnectedState();
		//        connectionStatusLabel->set_text("Invalid Address");
		//		  Gdk::RGBA red;
		//		  red.set_rgba(1.0,0,0,1.0);
		//		  connectionStatusLabel->override_background_color(red);
		//        ipAddressEntry->set_can_focus(true);
		//        ipAddressEntry->set_editable(true);
		//        connected=false;
		return;
	}
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");

		Gtk::MessageDialog dialog(*window, "Connection Failed", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK);
		int result = dialog.run();

		setDisconnectedState();
		//        connectionStatusLabel->set_text("Connection Failed");
		//		  Gdk::RGBA red;
		//		  red.set_rgba(1.0,0,0,1.0);
		//		  connectionStatusLabel->override_background_color(red);
		//        ipAddressEntry->set_can_focus(true);
		//        ipAddressEntry->set_editable(true);
		//        connected=false;
	} else {
		send(sock, hello.c_str(), strlen(hello.c_str()), 0);
		bytesRead = read(sock, buffer, 1024);
		fcntl(sock, F_SETFL, O_NONBLOCK);

		setConnectedState();
		//        connectButton->set_label("Disconnect");
		//		  connectionStatusLabel->set_text("Connected");
		//		  Gdk::RGBA green;
		//		  green.set_rgba(0,1.0,0,1.0);
		//		  connectionStatusLabel->override_background_color(green);
		//        ipAddressEntry->set_can_focus(false);
		//        ipAddressEntry->set_editable(false);
		//        connected=true;
	}
}

void disconnectFromServer() {
	Gtk::MessageDialog dialogDisconnect(*window, "Disconnect now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
	int resultDisconnect = dialogDisconnect.run();

	switch(resultDisconnect) {
		case(Gtk::RESPONSE_OK):
			// Shutdown failed
			if(shutdown(sock, SHUT_RDWR) == -1) {
				Gtk::MessageDialog dialogShutdown(*window, "Failed Shutdown", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
				dialogShutdown.run();
			}
			// Shutdown successful
			if(close(sock) == 0) {
				setDisconnectedState();
			} else {
				Gtk::MessageDialog dialogClose(*window, "Failed Close", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
				dialogClose.run();
			}

			break;
		case(Gtk::RESPONSE_CANCEL):
		case(Gtk::RESPONSE_NONE):
		default:
			break;
	}
}

void connectOrDisconnect() {
	Glib::ustring string = connectButton->get_label();
	//std::cout << "connect" << string << std::endl;
	if(string == "Connect") {
		connectToServer();
	} else {
		disconnectFromServer();
	}
}

void silentRun() {
	if(!connected) return;
	std::string currentButtonState = silentRunButton->get_label();
	if(currentButtonState == "Silent Running") {
		int messageSize = 3;
		uint8_t command = 7; // silence
		uint8_t message[messageSize];
		message[0] = messageSize;
		message[1] = command;
		message[2] = 0;
		send(sock, message, messageSize, 0);

		silentRunButton->set_label("Not Silent Running");
	} else {
		int messageSize = 3;
		uint8_t command = 7; // silence
		uint8_t message[messageSize];
		message[0] = messageSize;
		message[1] = command;
		message[2] = 1;
		send(sock, message, messageSize, 0);

		silentRunButton->set_label("Silent Running");
	}
}

void rowActivated(Gtk::ListBoxRow* listBoxRow) {
	//std::cout << "rowActivated" << std::endl;
	auto* label = dynamic_cast<Gtk::Label*>(listBoxRow->get_child());
	//std::cout << label->get_text() << std::endl;
	Glib::ustring connectionString(label->get_text());
	//std::cout << connectionString << std::endl;
	size_t index = connectionString.rfind('@');
	if(index == -1) return;
	++index;
	Glib::ustring addressString = connectionString.substr(index, connectionString.length() - index);
	ipAddressEntry->set_text(addressString);
}

void shutdownRobot() {
	//std::cout << "shutdownRobot" << std::endl;

	int messageSize = 2;
	uint8_t command = 8; // shutdown
	uint8_t message[messageSize];
	message[0] = messageSize;
	message[1] = command;
	send(sock, message, messageSize, 0);
}

void shutdownDialog(Gtk::Window* parentWindow) {
	Gtk::MessageDialog dialog(*parentWindow, "Shutdown now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
	//dialog.set_secondary_text("Do you want to shutdown now?");
	int result = dialog.run();

	switch(result) {
		case(Gtk::RESPONSE_OK):
			shutdownRobot();
			break;
		case(Gtk::RESPONSE_CANCEL):
		case(Gtk::RESPONSE_NONE):
		default:
			break;
	}
}

bool on_key_release_event(GdkEventKey* key_event) {
	//std::cout << "key released" << std::endl;
	//std::cout << std::hex << key_event->keyval << "  " << key_event->state << std::dec << std::endl;

	int messageSize = 5;
	uint8_t command = 2; // keyboard
	uint8_t message[messageSize];
	message[0] = messageSize;
	message[1] = command;
	message[2] = (uint8_t)(((key_event->keyval) >> 8) & 0xff);
	message[3] = (uint8_t)(((key_event->keyval) >> 0) & 0xff);
	message[4] = 0;
	send(sock, message, messageSize, 0);
	//std::cout << std::hex << (uint)message[2] << "  " << (uint)message[3] << std::dec << std::endl;

	return false;
}

bool on_key_press_event(GdkEventKey* key_event) {
	//std::cout << "key pressed" << std::endl;
	//std::cout << std::hex << key_event->keyval << "  " << key_event->state << std::dec << std::endl;

	int messageSize = 5;
	uint8_t command = 2; // keyboard
	uint8_t message[messageSize];
	message[0] = messageSize;
	message[1] = command;
	message[2] = (uint8_t)(((key_event->keyval) >> 8) & 0xff);
	message[3] = (uint8_t)(((key_event->keyval) >> 0) & 0xff);
	message[4] = 1;
	send(sock, message, messageSize, 0);
	//std::cout << std::hex << (uint)message[2] << "  " << (uint)message[3] << std::dec << std::endl;

	return false;
}

// GUI Boilerplate
// TODO: Refactor to reduce code reuse
void setupGui(const Glib::RefPtr<Gtk::Application>& application) {
	auto controlsBox = makeBox(Gtk::ORIENTATION_HORIZONTAL, 5);

	auto scrolledList = Gtk::manage(new Gtk::ScrolledWindow());
	addressListBox = Gtk::manage(new Gtk::ListBox());
	//	addressListBox->signal_row_selected().connect(sigc::ptr_fun(&rowSelected));
	addressListBox->signal_row_activated().connect(sigc::ptr_fun(&rowActivated));

	auto controlsRightBox = makeBox(Gtk::ORIENTATION_VERTICAL, 5);

	auto connectBox = makeBox(Gtk::ORIENTATION_HORIZONTAL, 5);
	auto ipAddressLabel = Gtk::manage(new Gtk::Label(" IP Address "));
	ipAddressEntry = Gtk::manage(new Gtk::Entry());
	ipAddressEntry->set_can_focus(true);
	ipAddressEntry->set_editable(true);
	//ipAddressEntry->set_text("192.168.1.2");
	connectButton = Gtk::manage(new Gtk::Button("Connect"));
	connectButton->signal_clicked().connect(sigc::ptr_fun(&connectOrDisconnect));
	connectionStatusLabel = Gtk::manage(new Gtk::Label("Not Connected"));
	Gdk::RGBA red;
	red.set_rgba(1.0, 0, 0, 1.0);
	connectionStatusLabel->override_background_color(red);

	auto stateBox = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	silentRunButton = Gtk::manage(new Gtk::Button("Silent Running"));
	silentRunButton->signal_clicked().connect(sigc::ptr_fun(&silentRun));
	Gtk::Label* modeLabel = Gtk::manage(new Gtk::Label("  Control Mode: "));
	controlModeLabel = Gtk::manage(new Gtk::Label("Drive "));

	auto remoteControlBox = makeBox(Gtk::ORIENTATION_HORIZONTAL, 2);
	auto shutdownRobotButton = Gtk::manage(new Gtk::Button("Shutdown Robot"));
	shutdownRobotButton->signal_clicked().connect(sigc::bind<Gtk::Window*>(sigc::ptr_fun(&shutdownDialog), window));

	auto sensorBox = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto talonBox = makeBox(Gtk::ORIENTATION_VERTICAL);
	auto victor1Box = makeBox(Gtk::ORIENTATION_VERTICAL);
	auto victor2Box = makeBox(Gtk::ORIENTATION_VERTICAL);

	auto powerDistributionPanelFrame = Gtk::manage(new Gtk::Frame("Power Distribution Panel"));
	auto powerDistributionPanelBox = makeBox(Gtk::ORIENTATION_VERTICAL, 5);

	auto voltageBox = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto voltageTextLabel = Gtk::manage(new Gtk::Label("Voltage:"));

	voltageLabel = Gtk::manage(new Gtk::Label("0"));

	auto current0Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current0TextLabel = Gtk::manage(new Gtk::Label("Current 0: "));
	current0Label = Gtk::manage(new Gtk::Label("0"));

	auto current1Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current1TextLabel = Gtk::manage(new Gtk::Label("Current 1:"));
	current1Label = Gtk::manage(new Gtk::Label("0"));

	auto current2Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current2TextLabel = Gtk::manage(new Gtk::Label("Current 2:"));
	current2Label = Gtk::manage(new Gtk::Label("0"));

	auto current3Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current3TextLabel = Gtk::manage(new Gtk::Label("Current 3:"));
	current3Label = Gtk::manage(new Gtk::Label("0"));

	auto current4Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current4TextLabel = Gtk::manage(new Gtk::Label("Current 4:"));
	current4Label = Gtk::manage(new Gtk::Label("0"));

	auto current5Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current5TextLabel = Gtk::manage(new Gtk::Label("Current 5:"));
	current5Label = Gtk::manage(new Gtk::Label("0"));

	auto current6Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current6TextLabel = Gtk::manage(new Gtk::Label("Current 6:"));
	current6Label = Gtk::manage(new Gtk::Label("0"));

	auto current7Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	auto current7TextLabel = Gtk::manage(new Gtk::Label("Current 7:"));
	current7Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current8Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current8TextLabel = Gtk::manage(new Gtk::Label("Current 8:"));
	current8Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current9Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current9TextLabel = Gtk::manage(new Gtk::Label("Current 9:"));
	current9Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current10Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current10TextLabel = Gtk::manage(new Gtk::Label("Current 10:"));
	current10Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current11Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current11TextLabel = Gtk::manage(new Gtk::Label("Current 11:"));
	current11Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current12Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current12TextLabel = Gtk::manage(new Gtk::Label("Current 12:"));
	current12Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current13Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current13TextLabel = Gtk::manage(new Gtk::Label("Current 13:"));
	current13Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current14Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current14TextLabel = Gtk::manage(new Gtk::Label("Current 14:"));
	current14Label = Gtk::manage(new Gtk::Label("0"));

	Gtk::Box* current15Box = makeBox(Gtk::ORIENTATION_HORIZONTAL);
	Gtk::Label* current15TextLabel = Gtk::manage(new Gtk::Label("Current 15:"));
	current15Label = Gtk::manage(new Gtk::Label("0"));

	voltageBox->pack_start(*voltageTextLabel, false, true, 10);
	voltageBox->pack_end(*voltageLabel, false, true, 10);

	current0Box->pack_start(*current0TextLabel, false, true, 10);
	current0Box->pack_end(*current0Label, false, true, 10);

	current1Box->pack_start(*current1TextLabel, false, true, 10);
	current1Box->pack_end(*current1Label, false, true, 10);

	current2Box->pack_start(*current2TextLabel, false, true, 10);
	current2Box->pack_end(*current2Label, false, true, 10);

	current3Box->pack_start(*current3TextLabel, false, true, 10);
	current3Box->pack_end(*current3Label, false, true, 10);

	current4Box->pack_start(*current4TextLabel, false, true, 10);
	current4Box->pack_end(*current4Label, false, true, 10);

	current5Box->pack_start(*current5TextLabel, false, true, 10);
	current5Box->pack_end(*current5Label, false, true, 10);

	current6Box->pack_start(*current6TextLabel, false, true, 10);
	current6Box->pack_end(*current6Label, false, true, 10);

	current7Box->pack_start(*current7TextLabel, false, true, 10);
	current7Box->pack_end(*current7Label, false, true, 10);

	current8Box->pack_start(*current8TextLabel, false, true, 10);
	current8Box->pack_end(*current8Label, false, true, 10);

	current9Box->pack_start(*current9TextLabel, false, true, 10);
	current9Box->pack_end(*current9Label, false, true, 10);

	current10Box->pack_start(*current10TextLabel, false, true, 10);
	current10Box->pack_end(*current10Label, false, true, 10);

	current11Box->pack_start(*current11TextLabel, false, true, 10);
	current11Box->pack_end(*current11Label, false, true, 10);

	current12Box->pack_start(*current12TextLabel, false, true, 10);
	current12Box->pack_end(*current12Label, false, true, 10);

	current13Box->pack_start(*current13TextLabel, false, true, 10);
	current13Box->pack_end(*current13Label, false, true, 10);

	current14Box->pack_start(*current14TextLabel, false, true, 10);
	current14Box->pack_end(*current14Label, false, true, 10);

	current15Box->pack_start(*current15TextLabel, false, true, 10);
	current15Box->pack_end(*current15Label, false, true, 10);

	powerDistributionPanelBox->add(*voltageBox);
	powerDistributionPanelBox->add(*current0Box);
	powerDistributionPanelBox->add(*current1Box);
	powerDistributionPanelBox->add(*current2Box);
	powerDistributionPanelBox->add(*current3Box);
	powerDistributionPanelBox->add(*current4Box);
	powerDistributionPanelBox->add(*current5Box);
	powerDistributionPanelBox->add(*current6Box);
	powerDistributionPanelBox->add(*current7Box);
	powerDistributionPanelBox->add(*current8Box);
	powerDistributionPanelBox->add(*current9Box);
	powerDistributionPanelBox->add(*current10Box);
	powerDistributionPanelBox->add(*current11Box);
	powerDistributionPanelBox->add(*current12Box);
	powerDistributionPanelBox->add(*current13Box);
	powerDistributionPanelBox->add(*current14Box);
	powerDistributionPanelBox->add(*current15Box);

	powerDistributionPanelFrame->add(*powerDistributionPanelBox);

	talon1InfoFrame = Gtk::manage(new InfoFrame("Talon 1"));
	talon1InfoFrame->addItem("Device ID");
	talon1InfoFrame->addItem("Bus Voltage");
	talon1InfoFrame->addItem("Output Current");
	talon1InfoFrame->addItem("Output Voltage");
	talon1InfoFrame->addItem("Output Percent");
	talon1InfoFrame->addItem("Temperature");
	//	talon1InfoFrame->addItem("Sensor Position");
	talon1InfoFrame->addItem("Sensor Velocity");
	talon1InfoFrame->addItem("Closed Loop Error");
	talon1InfoFrame->addItem("Integral Accumulator");
	talon1InfoFrame->addItem("Error Derivative");

	talon2InfoFrame = Gtk::manage(new InfoFrame("Talon 2"));
	talon2InfoFrame->addItem("Device ID");
	talon2InfoFrame->addItem("Bus Voltage");
	talon2InfoFrame->addItem("Output Current");
	talon2InfoFrame->addItem("Output Voltage");
	talon2InfoFrame->addItem("Output Percent");
	talon2InfoFrame->addItem("Temperature");
	//	talon2InfoFrame->addItem("Sensor Position");
	talon2InfoFrame->addItem("Sensor Velocity");
	talon2InfoFrame->addItem("Closed Loop Error");
	talon2InfoFrame->addItem("Integral Accumulator");
	talon2InfoFrame->addItem("Error Derivative");

	victor1InfoFrame = Gtk::manage(new InfoFrame("Victor 1"));
	victor1InfoFrame->addItem("Device ID");
	victor1InfoFrame->addItem("Bus Voltage");
	victor1InfoFrame->addItem("Output Voltage");
	victor1InfoFrame->addItem("Output Percent");

	victor2InfoFrame = Gtk::manage(new InfoFrame("Victor 2"));
	victor2InfoFrame->addItem("Device ID");
	victor2InfoFrame->addItem("Bus Voltage");
	victor2InfoFrame->addItem("Output Voltage");
	victor2InfoFrame->addItem("Output Percent");

	victor3InfoFrame = Gtk::manage(new InfoFrame("Victor 3"));
	victor3InfoFrame->addItem("Device ID");
	victor3InfoFrame->addItem("Bus Voltage");
	victor3InfoFrame->addItem("Output Voltage");
	victor3InfoFrame->addItem("Output Percent");

	addressListBox->set_size_request(200, 100);
	scrolledList->set_size_request(200, 100);

	connectBox->add(*ipAddressLabel);
	connectBox->add(*ipAddressEntry);
	connectBox->add(*connectButton);
	connectBox->add(*connectionStatusLabel);

	sensorBox->add(*powerDistributionPanelFrame);
	talonBox->add(*talon1InfoFrame);
	talonBox->add(*talon2InfoFrame);
	sensorBox->add(*talonBox);
	victor1Box->add(*victor1InfoFrame);
	victor1Box->add(*victor2InfoFrame);
	victor1Box->add(*victor3InfoFrame);
	sensorBox->add(*victor1Box);

	stateBox->add(*silentRunButton);
	stateBox->add(*modeLabel);
	stateBox->add(*controlModeLabel);

	remoteControlBox->add(*shutdownRobotButton);

	controlsRightBox->add(*connectBox);
	controlsRightBox->add(*stateBox);
	controlsRightBox->add(*remoteControlBox);

	scrolledList->add(*addressListBox);

	controlsBox->add(*scrolledList);
	controlsBox->add(*controlsRightBox);

	// Create the window
	window = WindowFactory()
				 .addEventWithCallback(Gdk::KEY_PRESS_MASK, on_key_press_event)
				 .addEventWithCallback(Gdk::KEY_RELEASE_MASK, on_key_release_event)
				 .addWidget(BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
								.addWidget(controlsBox)
								.addWidget(sensorBox)
								.build())
				 .addDeleteEvent(quit)
				 .build();

	// Show the window
	window->show_all();
}

struct RemoteRobot {
	std::string tag;
	time_t lastSeenTime{};
};
std::vector<RemoteRobot> robotList;

bool contains(std::vector<std::string>& list, std::string& value) {
	return std::any_of(list.begin(), list.end(), [value](const std::string& storedValue) {
		return storedValue == value;
	});
}

bool contains(std::vector<RemoteRobot>& list, std::string& robotTag) {
	return std::any_of(list.begin(), list.end(), [robotTag](const RemoteRobot& storedValue) {
		return robotTag == storedValue.tag;
	});
}

void update(std::vector<RemoteRobot>& list, std::string& robotTag) {
	for(auto& index : list) {
		time_t now;
		time(&now);
		index.lastSeenTime = now;
	}
}

std::vector<std::string> getAddressList() {
	std::vector<std::string> addressList;
	ifaddrs* interfaceAddresses = nullptr;
	for(int failed = getifaddrs(&interfaceAddresses); !failed && interfaceAddresses != nullptr; interfaceAddresses = interfaceAddresses->ifa_next) {
		if(interfaceAddresses->ifa_addr != nullptr && interfaceAddresses->ifa_addr->sa_family == AF_INET) {
			auto* socketAddress = reinterpret_cast<sockaddr_in*>(interfaceAddresses->ifa_addr);
			std::string addressString(inet_ntoa(socketAddress->sin_addr));
			if(addressString == "0.0.0.0") continue;
			if(addressString == "127.0.0.1") continue;
			if(contains(addressList, addressString)) continue;
			addressList.push_back(addressString);
		}
	}
	return addressList;
}

void broadcastListen() {
	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0) {
		perror("Opening datagram socket error");
		return;
	}

	int reuse = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		perror("Setting SO_REUSEADDR error");
		close(sd);
		return;
	}

	/* Bind to the proper port number with the IP address */
	/* specified as INADDR_ANY. */
	struct sockaddr_in localSock {};
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

	std::vector<std::string> addressList = getAddressList();
	for(const std::string& addressString : addressList) {
		//        std::cout << "got " << addressString << std::endl;
		struct ip_mreq group {};
		group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
		group.imr_interface.s_addr = inet_addr(addressString.c_str());
		if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&group, sizeof(group)) < 0) {
			perror("Adding multicast group error");
		}
	}

	char databuf[1024];
	int datalen = sizeof(databuf);
	while(true) {
		if(read(sd, databuf, datalen) >= 0) {
			std::string message(databuf);
			if(!contains(robotList, message)) {
				RemoteRobot remoteRobot;
				remoteRobot.tag = message;
				time(&remoteRobot.lastSeenTime);
				robotList.push_back(remoteRobot);
			}
			update(robotList, message);
		}
	}
}

void adjustRobotList() {
	for(int index = 0; index < robotList.size(); ++index) {
		time_t now;
		time(&now);
		if(now - robotList[index].lastSeenTime > 12) {
			robotList.erase(robotList.begin() + index--);
		}
	}
	//add new elements
	for(const RemoteRobot& remoteRobot : robotList) {
		std::string robotID = remoteRobot.tag;
		bool match = false;
		int index = 0;
		for(Gtk::ListBoxRow* listBoxRow = addressListBox->get_row_at_index(index); listBoxRow; listBoxRow = addressListBox->get_row_at_index(++index)) {
			auto* label = dynamic_cast<Gtk::Label*>(listBoxRow->get_child());
			Glib::ustring addressString = label->get_text();
			if(robotID == addressString.c_str()) {
				match = true;
				break;
			}
		}
		if(!match) {
			Gtk::Label* label = Gtk::manage(new Gtk::Label(robotID));
			label->set_visible(true);
			addressListBox->append(*label);
		}
	}

	//remove old element
	int index = 0;
	for(Gtk::ListBoxRow* listBoxRow = addressListBox->get_row_at_index(index); listBoxRow; listBoxRow = addressListBox->get_row_at_index(++index)) {
		auto* label = dynamic_cast<Gtk::Label*>(listBoxRow->get_child());
		Glib::ustring addressString = label->get_text();
		bool match = false;
		for(const RemoteRobot& remoteRobot : robotList) {
			std::string robotID = remoteRobot.tag;
			if(robotID == addressString.c_str()) {
				match = true;
				break;
			}
		}
		if(!match) {
			addressListBox->remove(*listBoxRow);
			--index;
		}
	}
}

int main(int argc, char** argv) {
	// Initialize GTK
	Glib::RefPtr<Gtk::Application> application = Gtk::Application::create(argc, argv, "edu.uark.razorbotz");
	setupGui(application);

	// Create network listener
	std::thread broadcastListenThread(broadcastListen);

	// Initialize SDL
	if(SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	//    int sensorCount=SDL_NumSensors();
	//    std::cout << "number of sensors " << sensorCount << std::endl;

	// Initialize joystick(s)
	int joystickCount = SDL_NumJoysticks();
	std::cout << "number of joysticks " << joystickCount << std::endl;
	SDL_Joystick* joystickList[joystickCount];

	if(joystickCount > 0) {
		for(int joystickIndex = 0; joystickIndex < joystickCount; joystickIndex++) {
			joystickList[joystickIndex] = SDL_JoystickOpen(joystickIndex);

			if(joystickList[joystickIndex]) {
				std::cout << "Opened Joystick " << joystickIndex << std::endl;
				std::cout << "\tName: " << SDL_JoystickName(joystickList[joystickIndex]) << std::endl;
				std::cout << "\tNumber of Axes: " << SDL_JoystickNumAxes(joystickList[joystickIndex]) << std::endl;
				std::cout << "\tNumber of Buttons: " << SDL_JoystickNumButtons(joystickList[joystickIndex]) << std::endl;
				std::cout << "\tNumber of Balls: " << SDL_JoystickNumBalls(joystickList[joystickIndex]) << std::endl;
			} else {
				std::cout << "Couldn't open Joystick " << joystickIndex << std::endl;
			}
		}
	}

	SDL_Event event;
	char buffer[1024] = {0};
	size_t bytesRead = 0;

	std::list<uint8_t> messageBytesList;
	uint8_t headMessage[256];
	while(true) {
		adjustRobotList();

		while(Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}
		if(!connected) continue;

		bytesRead = read(sock, buffer, 1024);
		for(int index = 0; index < bytesRead; index++) {
			messageBytesList.push_back(buffer[index]);
		}

		if(bytesRead == 0) {
			//std::cout << "Lost Connection" << std::endl;
			setDisconnectedState();

			//            connectButton->set_label("Connect");
			//		      connectionStatusLabel->set_text("Lost Connection");
			//			  Gdk::RGBA red;
			//			  red.set_rgba(1.0,0,0,1.0);
			//			  connectionStatusLabel->override_background_color(red);
			//			  connected=false;
			//            ipAddressEntry->set_can_focus(true);
			//            ipAddressEntry->set_editable(true);

			continue;
		}

		while(!messageBytesList.empty() && messageBytesList.front() <= messageBytesList.size()) {
			int messageSize = messageBytesList.front();
			messageBytesList.pop_front();
			messageSize--;
			for(int index = 0; index < messageSize; index++) {
				headMessage[index] = messageBytesList.front();
				messageBytesList.pop_front();
			}

			//std::cout << "Got Message" << std::endl;
			uint8_t command = headMessage[0];
			//std::cout << "command " << (int)command << std::endl;
			// TODO: Refactor to reduce code duplication
			if(command == 1) {
				auto voltage = parseType<float>((uint8_t*)&headMessage[1]);
				auto current0 = parseType<float>((uint8_t*)&headMessage[5]);
				auto current1 = parseType<float>((uint8_t*)&headMessage[9]);
				auto current2 = parseType<float>((uint8_t*)&headMessage[13]);
				auto current3 = parseType<float>((uint8_t*)&headMessage[17]);
				auto current4 = parseType<float>((uint8_t*)&headMessage[21]);
				auto current5 = parseType<float>((uint8_t*)&headMessage[25]);
				auto current6 = parseType<float>((uint8_t*)&headMessage[29]);
				auto current7 = parseType<float>((uint8_t*)&headMessage[33]);
				auto current8 = parseType<float>((uint8_t*)&headMessage[37]);
				auto current9 = parseType<float>((uint8_t*)&headMessage[41]);
				auto current10 = parseType<float>((uint8_t*)&headMessage[45]);
				auto current11 = parseType<float>((uint8_t*)&headMessage[49]);
				auto current12 = parseType<float>((uint8_t*)&headMessage[53]);
				auto current13 = parseType<float>((uint8_t*)&headMessage[57]);
				auto current14 = parseType<float>((uint8_t*)&headMessage[61]);
				auto current15 = parseType<float>((uint8_t*)&headMessage[65]);
				voltageLabel->set_label(std::to_string(voltage).c_str());
				current0Label->set_label(std::to_string(current0).c_str());
				current1Label->set_label(std::to_string(current1).c_str());
				current2Label->set_label(std::to_string(current2).c_str());
				current3Label->set_label(std::to_string(current3).c_str());
				current4Label->set_label(std::to_string(current4).c_str());
				current5Label->set_label(std::to_string(current5).c_str());
				current6Label->set_label(std::to_string(current6).c_str());
				current7Label->set_label(std::to_string(current7).c_str());
				current8Label->set_label(std::to_string(current8).c_str());
				current9Label->set_label(std::to_string(current9).c_str());
				current10Label->set_label(std::to_string(current10).c_str());
				current11Label->set_label(std::to_string(current11).c_str());
				current12Label->set_label(std::to_string(current12).c_str());
				current13Label->set_label(std::to_string(current13).c_str());
				current14Label->set_label(std::to_string(current14).c_str());
				current15Label->set_label(std::to_string(current15).c_str());
			}
			if(command == 2) {
				auto deviceID = parseType<int>((uint8_t*)&headMessage[1]);
				auto busVoltage = parseType<float>((uint8_t*)&headMessage[5]);
				auto outputCurrent = parseType<float>((uint8_t*)&headMessage[9]);
				auto outputVoltage = parseType<float>((uint8_t*)&headMessage[13]);
				auto outputPercent = parseType<float>((uint8_t*)&headMessage[17]);
				auto temperature = parseType<float>((uint8_t*)&headMessage[21]);
				auto sensorPosition = parseType<int>((uint8_t*)&headMessage[25]);
				auto sensorVelocity = parseType<int>((uint8_t*)&headMessage[29]);
				auto closedLoopError = parseType<int>((uint8_t*)&headMessage[33]);
				auto integralAccumulator = parseType<int>((uint8_t*)&headMessage[37]);
				auto errorDerivative = parseType<int>((uint8_t*)&headMessage[41]);
				talon1InfoFrame->setItem("Device ID", deviceID);
				talon1InfoFrame->setItem("Bus Voltage", busVoltage);
				talon1InfoFrame->setItem("Output Current", outputCurrent);
				talon1InfoFrame->setItem("Output Voltage", outputVoltage);
				talon1InfoFrame->setItem("Output Percent", outputPercent);
				talon1InfoFrame->setItem("Temperature", temperature);
				talon1InfoFrame->setItem("Sensor Position", sensorPosition);
				talon1InfoFrame->setItem("Sensor Velocity", sensorVelocity);
				talon1InfoFrame->setItem("Closed Loop Error", closedLoopError);
				talon1InfoFrame->setItem("Integral Accumulator", integralAccumulator);
				talon1InfoFrame->setItem("Error Derivative", errorDerivative);
			}
			if(command == 3) {
				auto deviceID = parseType<int>((uint8_t*)&headMessage[1]);
				auto busVoltage = parseType<float>((uint8_t*)&headMessage[5]);
				auto outputCurrent = parseType<float>((uint8_t*)&headMessage[9]);
				auto outputVoltage = parseType<float>((uint8_t*)&headMessage[13]);
				auto outputPercent = parseType<float>((uint8_t*)&headMessage[17]);
				auto temperature = parseType<float>((uint8_t*)&headMessage[21]);
				auto sensorPosition = parseType<int>((uint8_t*)&headMessage[25]);
				auto sensorVelocity = parseType<int>((uint8_t*)&headMessage[29]);
				auto closedLoopError = parseType<int>((uint8_t*)&headMessage[33]);
				auto integralAccumulator = parseType<int>((uint8_t*)&headMessage[37]);
				auto errorDerivative = parseType<int>((uint8_t*)&headMessage[41]);
				talon2InfoFrame->setItem("Device ID", deviceID);
				talon2InfoFrame->setItem("Bus Voltage", busVoltage);
				talon2InfoFrame->setItem("Output Current", outputCurrent);
				talon2InfoFrame->setItem("Output Voltage", outputVoltage);
				talon2InfoFrame->setItem("Output Percent", outputPercent);
				talon2InfoFrame->setItem("Temperature", temperature);
				talon2InfoFrame->setItem("Sensor Position", sensorPosition);
				talon2InfoFrame->setItem("Sensor Velocity", sensorVelocity);
				talon2InfoFrame->setItem("Closed Loop Error", closedLoopError);
				talon2InfoFrame->setItem("Integral Accumulator", integralAccumulator);
				talon2InfoFrame->setItem("Error Derivative", errorDerivative);
			}
			if(command == 4) {
				auto deviceID = parseType<int>((uint8_t*)&headMessage[1]);
				auto busVoltage = parseType<float>((uint8_t*)&headMessage[5]);
				auto outputVoltage = parseType<float>((uint8_t*)&headMessage[9]);
				auto outputPercent = parseType<float>((uint8_t*)&headMessage[13]);
				victor1InfoFrame->setItem("Device ID", deviceID);
				victor1InfoFrame->setItem("Bus Voltage", busVoltage);
				victor1InfoFrame->setItem("Output Voltage", outputVoltage);
				victor1InfoFrame->setItem("Output Percent", outputPercent);
			}
			if(command == 5) {
				auto deviceID = parseType<int>((uint8_t*)&headMessage[1]);
				auto busVoltage = parseType<float>((uint8_t*)&headMessage[5]);
				auto outputVoltage = parseType<float>((uint8_t*)&headMessage[9]);
				auto outputPercent = parseType<float>((uint8_t*)&headMessage[13]);
				victor2InfoFrame->setItem("Device ID", deviceID);
				victor2InfoFrame->setItem("Bus Voltage", busVoltage);
				victor2InfoFrame->setItem("Output Voltage", outputVoltage);
				victor2InfoFrame->setItem("Output Percent", outputPercent);
			}
			if(command == 6) {
				auto deviceID = parseType<int>((uint8_t*)&headMessage[1]);
				auto busVoltage = parseType<float>((uint8_t*)&headMessage[5]);
				auto outputVoltage = parseType<float>((uint8_t*)&headMessage[9]);
				auto outputPercent = parseType<float>((uint8_t*)&headMessage[13]);
				victor3InfoFrame->setItem("Device ID", deviceID);
				victor3InfoFrame->setItem("Bus Voltage", busVoltage);
				victor3InfoFrame->setItem("Output Voltage", outputVoltage);
				victor3InfoFrame->setItem("Output Percent", outputPercent);
			}
			if(command == 7) { //control mode
				int mode = headMessage[1];
				//std::cout << mode << std::endl;
				if(mode == 1) {
					controlModeLabel->set_label("Drive");
				}
				if(mode == 0) {
					controlModeLabel->set_label("Dig");
				}
			}
		}

		// Main event loop
		while(SDL_PollEvent(&event)) {
			//std::cout << "here" << std::endl;
			const Uint8* state = SDL_GetKeyboardState(nullptr);

			//const Uint8 *length = SDL_GetKeyboardState(state);
			//std::cout << "length " << *length << std::endl;
			//std::cout << "scan code A " << (int)state[SDL_SCANCODE_A] << std::endl;

			switch(event.type) {
				case SDL_MOUSEMOTION: {
					int mouseX = event.motion.x;
					int mouseY = event.motion.y;

					std::cout << "X: " << mouseX << " Y: " << mouseY << std::endl;

					break;
				}

				case SDL_KEYDOWN: {
					std::cout << event.key.keysym.sym << std::endl;
					std::cout << "Key Down" << std::endl;
					break;
				}

				case SDL_KEYUP: {
					std::cout << "Key Up" << std::endl;
					break;
				}

				case SDL_JOYHATMOTION: {
					//				std::cout << "joystick " << event.jhat.which << " ";
					//				std::cout << "timestamp " << event.jhat.timestamp << " ";
					//              std::cout << "hat " << (uint32_t)event.jhat.hat << " ";
					//				std::cout << "value " << (uint32_t)event.jhat.value << std::endl;

					uint8_t command = 6;
					int length = 5;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jhat.which;
					message[3] = event.jhat.hat;
					message[4] = event.jhat.value;

					send(sock, message, length, 0);

					break;
				}
				case SDL_JOYBUTTONDOWN: {
					//				std::cout << "joystick " << event.jbutton.which << " ";
					//				std::cout << "timestamp " << event.jbutton.timestamp << " ";
					//				std::cout << "button " << (uint32_t)event.jbutton.button << " ";
					//				std::cout << "state " << (uint32_t)event.jbutton.state << std::endl;

					uint8_t command = 5;
					int length = 5;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jbutton.which;
					message[3] = event.jbutton.button;
					message[4] = event.jbutton.state;

					send(sock, message, length, 0);

					break;
				}
				case SDL_JOYBUTTONUP: {
					//				std::cout << "joystick " << event.jbutton.which << " ";
					//				std::cout << "timestamp " << event.jbutton.timestamp << " ";
					//				std::cout << "button " << (uint32_t)event.jbutton.button << " ";
					//				std::cout << "state " << (uint32_t)event.jbutton.state << std::endl;

					uint8_t command = 5;
					int length = 5;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jbutton.which;
					message[3] = event.jbutton.button;
					message[4] = event.jbutton.state;

					send(sock, message, length, 0);

					break;
				}
				case SDL_JOYAXISMOTION: {
					//                std::cout << "joystick " << event.jaxis.which << " ";
					//                std::cout << "timestamp " << event.jaxis.timestamp << " ";
					//                std::cout << "axis " << (int) event.jaxis.axis << " ";
					//                std::cout << "value " << event.jaxis.value << std::endl;

					uint8_t command = 1;
					float value = (float)event.jaxis.value / -32768.f;

					int length = 8;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jaxis.which;
					message[3] = event.jaxis.axis; //0-roll 1-pitch 2-throttle 3-yaw
					insert(value, &message[4]);

					send(sock, message, length, 0);
				}

				//				if(event.jaxis.axis == 0){
				//					uint8_t command=1;
				//					float value=(float) event.jaxis.value/-32768.0;
				//
				//					int length=8;
				//					uint8_t headMessage[length];
				//					headMessage[0]=length;
				//                    headMessage[1]=command;
				//                    headMessage[2]=event.jaxis.axis;//0-roll 1-pitch 2-throttle 3-yaw
				//                    headMessage[3]=event.jaxis.which;
				//					insert(value,&headMessage[4]);
				//
				//					send(sock, headMessage, length, 0);
				//				}
				//				if(event.jaxis.axis == 1){
				//					uint8_t command=2;// pitch
				//					float value=(float) event.jaxis.value/-32768.0;
				//
				//                  int length=7;
				//					uint8_t headMessage[length];
				//					headMessage[0]=length;
				//					headMessage[1]=command;
				//                  headMessage[2]=event.jhat.which;
				//					insert(value,&headMessage[3]);
				//
				//					send(sock, headMessage, length, 0);
				//				}
				//				if(event.jaxis.axis == 2){
				//					uint8_t command=3;// throttle
				//					float value=(float) event.jaxis.value/-32768.0;
				//
				//                    int length=7;
				//					uint8_t headMessage[length];
				//					headMessage[0]=length;
				//					headMessage[1]=command;
				//                    headMessage[2]=event.jhat.which;
				//					insert(value,&headMessage[3]);
				//
				//					send(sock, headMessage, length, 0);
				//				}
				//				if(event.jaxis.axis == 3){
				//					uint8_t command=4;// yaw
				//					float value=(float) event.jaxis.value/-32768.0;
				//
				//                    int length=7;
				//					uint8_t headMessage[length];
				//					headMessage[0]=length;
				//					headMessage[1]=command;
				//                    headMessage[2]=event.jhat.which;
				//					insert(value,&headMessage[3]);
				//
				//					send(sock, headMessage, length, 0);
				//				}
				break;
				default:
					break;
			}
		}
	}
	return 0;
}
