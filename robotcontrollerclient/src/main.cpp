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
#include "ButtonFactory.hpp"
#include "GuiBox.hpp"
#include "RemoteRobot.hpp"
#include "TalonInfoFrame.hpp"
#include "VictorInfoFrame.hpp"
#include "WindowFactory.hpp"

// Port for communicating with the robot
constexpr unsigned int netPortNumber = 31337;

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
// TODO: Make this standards compliant and platform independent
template <typename T>
void insert(T value, uint8_t* array, const size_t arrayLen) {
	if(arrayLen < sizeof(T)) throw std::out_of_range("Type of value doesn't fit in array!"); // Throw an exception if variable doesn't fit in array

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

constexpr size_t numCurrents = 16;
Gtk::Label* currentLabels[numCurrents];

Gtk::ListBox* addressListBox;
Gtk::Entry* ipAddressEntry;
Gtk::Label* connectionStatusLabel;

Gtk::Button* silentRunButton;
Gtk::Button* connectButton;
Gtk::Label* controlModeLabel;

constexpr size_t numTalonInfoFrames = 2;
TalonInfoFrame* talonInfoFrames[numTalonInfoFrames];

constexpr size_t numVictorInfoFrames = 3;
VictorInfoFrame* victorInfoFrames[numVictorInfoFrames];

Gtk::Window* window;

int sock = 0;
bool connected = false;

// Net Commands
enum Commands {
	COMMAND_POWER_DISTRIBUTION_PANEL = 1,
	COMMAND_TALON_1,
	COMMAND_TALON_2,
	COMMAND_VICTOR_1,
	COMMAND_VICTOR_2,
	COMMAND_VICTOR_3,
	COMMAND_CONTROL
};

// Robot Modes
enum Modes {
	MODE_DRIVE = 1,
	MODE_DIG
};

// Helper function to set status and GUI info when robot disconnects/is not connected
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
	serv_addr.sin_port = htons(netPortNumber);

	char buffer[1024] = {0};
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");

		setDisconnectedState();
		return;
	}
	if(inet_pton(AF_INET, ipAddressEntry->get_text().c_str(), &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");

		Gtk::MessageDialog dialog(*window, "Invalid Address", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK);
		int result = dialog.run();

		setDisconnectedState();
		return;
	}
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");

		Gtk::MessageDialog dialog(*window, "Connection Failed", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK);
		int result = dialog.run();

		setDisconnectedState();
	} else {
		send(sock, hello.c_str(), strlen(hello.c_str()), 0);
		bytesRead = read(sock, buffer, 1024);
		fcntl(sock, F_SETFL, O_NONBLOCK);

		setConnectedState();
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
	// Create List Box
	addressListBox = Gtk::manage(new Gtk::ListBox());
	addressListBox->signal_row_activated().connect(sigc::ptr_fun(&rowActivated));
	addressListBox->set_size_request(200, 100);

	// Create Scrolled List
	auto scrolledList = Gtk::manage(new Gtk::ScrolledWindow());
	scrolledList->set_size_request(200, 100);
	scrolledList->add(*addressListBox);

	// Connect Button
	connectButton = Gtk::manage(new Gtk::Button("Connect"));
	connectButton->signal_clicked().connect(sigc::ptr_fun(&connectOrDisconnect));

	// Connect Status Label
	connectionStatusLabel = Gtk::manage(new Gtk::Label("Not Connected"));
	Gdk::RGBA red;
	red.set_rgba(1.0, 0, 0, 1.0);
	connectionStatusLabel->override_background_color(red);

	// IP Address Label
	auto ipAddressLabel = Gtk::manage(new Gtk::Label("IP Address"));
	ipAddressEntry = Gtk::manage(new Gtk::Entry());
	ipAddressEntry->set_can_focus(true);
	ipAddressEntry->set_editable(true);

	// Mode Label
	Gtk::Label* modeLabel = Gtk::manage(new Gtk::Label("Control Mode: "));
	controlModeLabel = Gtk::manage(new Gtk::Label("Drive"));

	// Silent Run Button
	auto silentRunButton = ButtonFactory("Silent Running")
							   .addClickedCallback<std::function<typeof(silentRun)>>(silentRun)
							   .build();

	// Shutdown Robot Button
	const auto shutdownCallback = [] { return shutdownDialog(window); };
	auto shutdownRobotButton = ButtonFactory("Shutdown Robot")
								   .addClickedCallback<typeof(shutdownCallback)>(shutdownCallback)
								   .build();

	// Power Distribution Frame
	auto powerDistributionPanelFrame = Gtk::manage(new Gtk::Frame("Power Distribution Panel"));

	// Power Labels
	voltageLabel = Gtk::manage(new Gtk::Label("0"));

	for(auto& label : currentLabels) {
		label = Gtk::manage(new Gtk::Label("0"));
	}

	// Power Boxes
	auto voltageBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
						  .addFrontLabel("Voltage:")
						  .packEnd(voltageLabel)
						  .build();

	Gtk::Box* currentBoxes[numCurrents];
	for(int i = 0; i < numCurrents; i++) {
		currentBoxes[i] = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
							  .addFrontLabel("Current " + std::to_string(i) + ":")
							  .packEnd(currentLabels[i])
							  .build();
	}

	// Power Distribution Panel Box
	auto powerDistributionPanelBoxFactory = BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
												.addWidget(voltageBox);
	for(auto& box : currentBoxes) {
		powerDistributionPanelBoxFactory.addWidget(box);
	}
	auto powerDistributionPanelBox = powerDistributionPanelBoxFactory.build();

	powerDistributionPanelFrame->add(*powerDistributionPanelBox);

	// Info Frame
	// Talon Info Frames
	for(int i = 0; i < numTalonInfoFrames; i++) {
		talonInfoFrames[i] = new TalonInfoFrame("Talon " + std::to_string(1 + i));
	}

	// Victor Info Frames
	for(int i = 0; i < numVictorInfoFrames; i++) {
		victorInfoFrames[i] = new VictorInfoFrame("Victor " + std::to_string(1 + i));
	}

	// Remote Control Box
	auto remoteControlBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
								.addWidget(shutdownRobotButton)
								.build();

	// State Box
	auto stateBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
						.addWidget(silentRunButton)
						.addWidget(modeLabel)
						.addWidget(controlModeLabel)
						.build();
	// Connect Box
	auto connectBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL, 5)
						  .addWidget(ipAddressLabel)
						  .addWidget(ipAddressEntry)
						  .addWidget(connectButton)
						  .addWidget(connectionStatusLabel)
						  .build();

	// Victor 1 Box
	auto victor1BoxFactory = BoxFactory(Gtk::ORIENTATION_VERTICAL);
	for(auto& victorInfoFrame : victorInfoFrames) {
		victor1BoxFactory.addWidget(victorInfoFrame);
	}
	auto victor1Box = victor1BoxFactory.build();

	// Controls Right Box
	auto controlsRightBox = BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
								.addWidget(connectBox)
								.addWidget(stateBox)
								.addWidget(remoteControlBox)
								.build();

	// Talon Box
	auto talonBoxFactory = BoxFactory(Gtk::ORIENTATION_VERTICAL);
	for(auto& talonInfoFrame : talonInfoFrames) {
		talonBoxFactory.addWidget(talonInfoFrame);
	}
	auto talonBox = talonBoxFactory.build();

	// Controls Box
	auto controlsBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL, 5)
						   .addWidget(scrolledList)
						   .addWidget(controlsRightBox)
						   .build();

	// Sensor Box
	auto sensorBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
						 .addWidget(powerDistributionPanelFrame)
						 .addWidget(talonBox)
						 .addWidget(victor1Box)
						 .build();

	auto parentBox = BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
						 .addWidget(controlsBox)
						 .addWidget(sensorBox)
						 .build();

	// Create the window
	window = WindowFactory()
				 .setTitle("Razorbotz Robot Remote Monitor and Controller")
				 .addEventWithCallback(Gdk::KEY_PRESS_MASK, on_key_press_event)
				 .addEventWithCallback(Gdk::KEY_RELEASE_MASK, on_key_release_event)
				 .addDeleteEvent(quit)
				 .addWidget(parentBox)
				 .build();

	// Show the window
	window->show_all();
}

// Find if string is in vector of strings
bool contains(std::vector<std::string>& list, std::string& value) {
	return std::any_of(list.begin(), list.end(), [value](const std::string& storedValue) {
		return storedValue == value;
	});
}

// Find if robot tag is in remote robot list
bool contains(std::vector<RemoteRobot>& list, std::string& robotTag) {
	return std::any_of(list.begin(), list.end(), [robotTag](const RemoteRobot& storedValue) {
		return robotTag == storedValue.tag;
	});
}

// Set last seen time
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

void displayVictorInfo(VictorInfoFrame* victorInfoFrame, uint8_t* headMessage) {
	auto deviceId = parseType<int>((uint8_t*)&headMessage[1]);
	auto busVoltage = parseType<float>((uint8_t*)&headMessage[5]);
	auto outputVoltage = parseType<float>((uint8_t*)&headMessage[9]);
	auto outputPercent = parseType<float>((uint8_t*)&headMessage[13]);
	victorInfoFrame->setItem("Device ID", deviceId);
	victorInfoFrame->setItem("Bus Voltage", busVoltage);
	victorInfoFrame->setItem("Output Voltage", outputVoltage);
	victorInfoFrame->setItem("Output Percent", outputPercent);
}

void displayTalonInfo(TalonInfoFrame* talonInfoFrame, uint8_t* headMessage) {
	auto deviceId = parseType<int>((uint8_t*)&headMessage[1]);
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
	talonInfoFrame->setItem("Device ID", deviceId);
	talonInfoFrame->setItem("Bus Voltage", busVoltage);
	talonInfoFrame->setItem("Output Current", outputCurrent);
	talonInfoFrame->setItem("Output Voltage", outputVoltage);
	talonInfoFrame->setItem("Output Percent", outputPercent);
	talonInfoFrame->setItem("Temperature", temperature);
	talonInfoFrame->setItem("Sensor Position", sensorPosition);
	talonInfoFrame->setItem("Sensor Velocity", sensorVelocity);
	talonInfoFrame->setItem("Closed Loop Error", closedLoopError);
	talonInfoFrame->setItem("Integral Accumulator", integralAccumulator);
	talonInfoFrame->setItem("Error Derivative", errorDerivative);
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

#ifdef DEBUG
			std::cout << "Got Message" << std::endl;
#endif // DEBUG
	   // Get command
			uint8_t command = headMessage[0];
#ifdef DEBUG
			std::cout << "Command: " << (int)command << std::endl;
#endif // DEBUG

			// Handle command
			switch(command) {
				case COMMAND_POWER_DISTRIBUTION_PANEL: {
					// Display voltage
					auto voltage = parseType<float>((uint8_t*)&headMessage[1]);
					voltageLabel->set_label(std::to_string(voltage).c_str());

					// Display Currents
					for(int index = 0; index < numCurrents; index++) {
						const auto current = parseType<float>((uint8_t*)&headMessage[5 + index * sizeof(float)]);
						currentLabels[index]->set_label(std::to_string(current).c_str());
					}
				} break;

				case COMMAND_TALON_1:
					displayTalonInfo(talonInfoFrames[0], headMessage);
					break;

				case COMMAND_TALON_2:
					displayTalonInfo(talonInfoFrames[1], headMessage);
					break;

				case COMMAND_VICTOR_1:
					displayVictorInfo(victorInfoFrames[0], headMessage);
					break;

				case COMMAND_VICTOR_2:
					displayVictorInfo(victorInfoFrames[1], headMessage);
					break;

				case COMMAND_VICTOR_3:
					displayVictorInfo(victorInfoFrames[2], headMessage);
					break;

				case COMMAND_CONTROL: { // Control mode
					const int mode = headMessage[1];
#ifdef DEBUG
					std::cout << "Mode: " << mode << std::endl;
#endif // DEBUG

					switch(mode) {
						case MODE_DRIVE:
							controlModeLabel->set_label("Drive");
							break;
						case MODE_DIG:
							controlModeLabel->set_label("Dig");
							break;
					}
				} break;
				default:
					break;
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
				break;
				default:
					break;
			}
		}
	}
}
