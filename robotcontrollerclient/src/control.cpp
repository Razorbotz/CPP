/** @file
 * @brief GUI client program to control and display info for with the robot.
 *
 * @author Bill
 * @author Johnston
 * @author Luke Simmons
 *
 * @date 2022-2-18
 *
 * This program will connect to the robot via the internet in order to display
 * information about the robot and allow the user to control the robot. The
 * interface is written in GTK and uses the Gtkmm library. It allows the usage
 * of controllers and joysticks on the client to control the robot by using the
 * SDL library. In order to connect to the robot, the user must enter the IP
 * address of the robot in the GUI. It will display information about the
 * voltages and motors on the robot. Define "DEBUG" to print debug information.
 * */

#include <arpa/inet.h>
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
#include "RemoteRobot.hpp"
#include "TalonInfoFrame.hpp"
#include "VictorInfoFrame.hpp"
#include "WindowFactory.hpp"

/// Port for communicating with the robot over the network
constexpr unsigned int netPortNumber = 31337;

/** @brief Checks if the machine is little endian.
 *
 * Determines if the machine running the code is big or little endian.
 * Checks if the first byte of a short is the least significant byte.
 *
 * @return  Endianness of the machine
 * @retval  true    Machine is Little-Endian (LE)
 * @retval  false   Machine is Big-Endian (BE)
 * */
bool isLittleEndian() {
	// Needs to be volatile to test at runtime
	volatile const unsigned short n = 0x1;
	return *((char*)&n);
}

/** @brief Converts a byte array to a specified type.
 *
 * Takes a little-endian byte array of a serialized type and returns an object of that type.
 *
 * The conversion itself is big-endian, but since the data is in little-endian
 * form, it gives a little-endian result. For example a byte array with a value
 * of {1, 0, 0, 0} will return an integer with a value of 1.
 *
 * If the array's size is smaller than type, the higher-order (MSBs of type) bytes will be filled with zero.
 *
 * @throw   std::out_of_range   If size of array to convert is larger than the type to be converted
 *
 * @param[in]   array   The array to be converted to a type T
 * @param[in]   size    The size of the array. Must be less than the size of the type
 * @return Value of type T filled with data from input array
 * */
template <typename T>
T deserialize(const uint8_t* array, const size_t size) {
	if(size > sizeof(T)) throw std::out_of_range("Size of array does not fit into value!");

	union {
		T value;
		uint8_t rawBytes[sizeof(T)];
	} dest{};

	// Computer is Little-Endian: Already in that form
	if(isLittleEndian()) {
		for(size_t i = 0; i < size; i++)
			dest.rawBytes[i] = array[i];
	}
	// Computer is Big-Endian: Need to reverse array
	else {
		for(size_t i = 0; i < size; i++)
			dest.rawBytes[i] = array[size - i];
	}

	return dest.value;
}

/** @brief Converts a variable of a given type to a byte array.
 *
 * Takes a data type and fills byte array with its data in little-endian form.
 *
 * The conversion itself is big-endian, but since the data expected is in
 * little-endian form, it gives a little-endian result. For example an integer
 * with a value of 1 will return a byte array with a value of {1, 0, 0, 0}.
 *
 * If the array's size is smaller than type, it will be filled with the most/least? significant bytes.
 * If the array's size is larger than the type, then the higher bytes of the array will remain untouched.
 *
 * @param[in]   value       Variable to fill array with
 * @param[out]  array       The array to be converted to a type T
 * @param[in]   arrayLen    Size of array to be filled
 * @return void
 * */
template <typename T>
void serialize(const T value, uint8_t* array, const size_t arrayLen) {
	// Only use smallest of the two given sizes
	const size_t smallestSize = arrayLen > sizeof(T) ? sizeof(T) : arrayLen;

	// Union for converting the type to raw bytes
	union {
		T value;
		uint8_t rawBytes[sizeof(T)];
	} source{};

	// Copy the given value to the union
	source.value = value;

	// Changes endianness if it is not LE (is BE instead). Since it expects that the computer is LE.
	if(!isLittleEndian()) {
		uint8_t copy[sizeof(source.rawBytes)/sizeof(source.rawBytes[0])];
		memcpy(copy, source.rawBytes, sizeof(source.rawBytes)/sizeof(source.rawBytes[0]));

		for(size_t k = 0; k < sizeof(T); k++)
			source.rawBytes[k] = copy[sizeof(T) - k - 1];
	}

	// Save the value
	for(size_t i = 0; i < smallestSize; i++)
		array[i] = source.rawBytes[i];
}

/** @brief Callback function to quit the program
 *
 * Exits the program with status zero when called.
 *
 * @param[in]   event   Unused. A pointer to any GDK Event.
 * @return Nothing (program exits)
 * */
bool quit(const GdkEventAny* event) {
	exit(0);
}

// GUI Element Variables
/// Label for the voltage
Gtk::Label* voltageLabel;

/// Number of currents on the robot
constexpr size_t numCurrents = 16;
/// Array to store current labels. Size is number of currents.
Gtk::Label* currentLabels[numCurrents];

/// List box for IP addresses.
Gtk::ListBox* addressListBox;
/// Entry box to enter IP address of robot.
Gtk::Entry* ipAddressEntry;
/// Label to show connection status to robot.
Gtk::Label* connectionStatusLabel;

/// Button to enable silent running mode.
Gtk::Button* silentRunButton;
/// Button to connect to robot.
Gtk::Button* connectButton;
/// Label to show what mode is running. \see Modes for values.
Gtk::Label* controlModeLabel;

/// Number of Talon motors on the robot
constexpr size_t numTalonInfoFrames = 2;
/// Array of Talon Info Frames. Size is number of Talon motors.
TalonInfoFrame* talonInfoFrames[numTalonInfoFrames];

/// Number of Victor motors on the robot
constexpr size_t numVictorInfoFrames = 3;
/// Array of Victor Info Frames. Size is number of Victor motors.
VictorInfoFrame* victorInfoFrames[numVictorInfoFrames];

/// GTK Window for the GUI
Gtk::Window* window;

/// Socket for network communication
int sock = 0;
/// Variable for storing if the robot is connected to the client or not.
bool connected = false;

/** @brief Different commands sent from the robot to the client.
 *
 * Used to specify what kind of message is received from the robot to the client.
 * Used at the first byte of messages.
 *
 * @see main
 * */
enum CommandsToClient {
	/// Message contains information about the Power Distribution Panel (PDP) (Voltage and Currents) from the robot.
	COMMAND_TO_CLIENT_POWER_DISTRIBUTION_PANEL = 1,
	/// Message contains information about Talon motor 1 from the robot.
	COMMAND_TO_CLIENT_TALON_1,
	/// Message contains information about Talon motor 2 from the robot.
	COMMAND_TO_CLIENT_TALON_2,
	/// Message contains information about Victor motor 1 from the robot.
	COMMAND_TO_CLIENT_VICTOR_1,
	/// Message contains information about Victor motor 2 from the robot.
	COMMAND_TO_CLIENT_VICTOR_2,
	/// Message contains information about Victor motor 3 from the robot.
	COMMAND_TO_CLIENT_VICTOR_3,
	/// Message contains information about what control mode the robot is in from the robot. \see ControlModes for control modes.
	COMMAND_TO_CLIENT_CONTROL_STATUS
};

/** @brief Different commands sent from the client to the robot.
 *
 * Used to specify what kind of message is sent from the client to the robot.
 * Used at the first byte of messages.
 *
 * @see main
 * @see on_key_press_event
 * @see on_key_release_event
 * @see silentRun
 * @see shutdownDialog
 * */
enum CommandsToRobot {
	/// Message contains information about joystick movement from the client.
	COMMAND_TO_ROBOT_JOYSTICK_AXIS_MOTION_EVENT = 1,
	/// Message contains information about keyboard events from the client.
	COMMAND_TO_ROBOT_KEYBOARD_EVENT,
	/// Message contains information about joystick button events from the client.
	COMMAND_TO_ROBOT_JOYSTICK_BUTTON_EVENT = 5,
	/// Message contains information about joystick hat movement from the client.
	COMMAND_TO_ROBOT_JOYSTICK_HAT_MOTION_EVENT,
	/// Message that tells the robot to set control status.
	COMMAND_TO_ROBOT_SILENT_RUN,
	/// Message that tells the robot to shutdown.
	COMMAND_TO_ROBOT_SHUTDOWN
};

/** @brief Modes for driving the robot.
 *
 * Used to specify which part of the robot should be controlled by the client.
 * */
enum ControlModes {
	/// Robot in drive mode.
	CONTROL_MODE_DRIVE = 1,
	/// Robot in dig mode.
	CONTROL_MODE_DIG
};

/** @brief Set the program's state to disconnected from robot.
 *
 * Change GUI state and internal variables to represent that the robot is disconnected from the client.
 *
 * @return void
 * */
void setDisconnectedState() {
	connectButton->set_label("Connect");
	connectionStatusLabel->set_text("Not Connected");

	// Set status label to red
	Gdk::RGBA red;
	red.set_rgba(1.0, 0.0, 0.0, 1.0);
	connectionStatusLabel->override_background_color(red);
	ipAddressEntry->set_can_focus(true);
	ipAddressEntry->set_editable(true);
	connected = false;

	// Reset socket
	sock = 0;
}

/** @brief Set the program's state to connected to robot.
 *
 * Change GUI state and internal variables to represent that the robot is connected to the client.
 *
 * @return void
 * */
void setConnectedState() {
	connectButton->set_label("Disconnect");
	connectionStatusLabel->set_text("Connected");

	// Set status label to green
	Gdk::RGBA green;
	green.set_rgba(0.0, 1.0, 0.0, 1.0);
	connectionStatusLabel->override_background_color(green);

	ipAddressEntry->set_can_focus(false);
	ipAddressEntry->set_editable(false);
	connected = true;
}

/** @brief Connect to the robot.
 *
 * Connects to the robot remotely via the internet through a POSIX socket.
 * Connects to port specified by "netPortNumber" and the IP address in the IP
 * entry box. Will alter the connection state based on result of connection.
 * Saves the socket to the global variable. Will show a message on failure.
 *
 * @return void
 * */
void connectToServer() {
	if(connected) return;
	struct sockaddr_in serv_addr {};
	const char* hello = "Hello Robot";

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(netPortNumber);

	char buffer[1024] = {0};
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "Socket creation error!" << std::endl;

		setDisconnectedState();
		return;
	}
	if(inet_pton(AF_INET, ipAddressEntry->get_text().c_str(), &serv_addr.sin_addr) <= 0) {
#ifdef DEBUG
		std::cout << "Invalid address/address not supported!" << std::endl;
#endif // DEBUG

		Gtk::MessageDialog dialog(*window, "Invalid Address", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK);
		dialog.run();

		setDisconnectedState();
		return;
	}
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		std::cout << "Connection failed!" << std::endl;

		Gtk::MessageDialog dialog(*window, "Connection Failed", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK);
		dialog.run();

		setDisconnectedState();
	} else {
		send(sock, hello, strlen(hello), 0);
		read(sock, buffer, 1024);
		fcntl(sock, F_SETFL, O_NONBLOCK);

		setConnectedState();
	}
}

/** @brief Disconnect from the robot.
 *
 * Will display a message asking user to confirm to disconnect and will
 * disconnect from robot if user confirms. Will set the disconnected state if
 * the shutdown is successful.
 *
 * @return void
 * */
void disconnectFromServer() {
	Gtk::MessageDialog dialogDisconnect(*window, "Disconnect now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);

	if(dialogDisconnect.run() == Gtk::RESPONSE_OK) {
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
	}
}

/** @brief Toggles the connection state between the client and robot.
 *
 * Runs the opposite connection function of the current status.
 *
 * @return void
 * */
void connectOrDisconnect() {
#ifdef DEBUG
	std::cout << "Connected: " << connected << std::endl;
#endif // DEBUG

	if(connected)
		disconnectFromServer();
	else
		connectToServer();
}

/** @brief Toggles silent run mode on the robot.
 *
 * Checks the state of silent running from the silent run button text.
 * Sends a message to enable or disable silent running mode on the robot.
 * Updates silent run button text to show if it is silent or not.
 * Only works if connected to the robot.
 *
 * @return void
 * */
void silentRun() {
	// Only run if connected
	if(!connected) return;

	// Get silent running state
	std::string currentButtonState = silentRunButton->get_label();
	bool silentRunning = currentButtonState == "Silent Running";

	// Generate message
	constexpr uint8_t messageSize = 3;
	constexpr uint8_t command = COMMAND_TO_ROBOT_SILENT_RUN;
	uint8_t message[messageSize];
	message[0] = messageSize;
	message[1] = command;
	message[2] = silentRunning;

	// Send message
	send(sock, message, messageSize, 0);

	// Set GUI
	silentRunButton->set_label(silentRunning ? "Not Silent Running" : "Silent Running");
}

/** @brief Sets IP Address Entry to selected IP address from the Address List Box
 *
 * Callback function for addressListBox when a row is activated.
 * Sets the text for ipAddressEntry to the address string in the selected row
 * passed in.
 *
 * @see addressListBox
 * @see ipAddressEntry
 *
 * @param[in]   listBoxRow  The selected row
 * @return void
 * */
void rowActivated(const Gtk::ListBoxRow* listBoxRow) {
#ifdef DEBUG
	std::cout << "Row activated: " << std::endl;
#endif // DEBUG

	const auto* label = dynamic_cast<const Gtk::Label*>(listBoxRow->get_child());

#ifdef DEBUG
	std::cout << "\t" << label->get_text() << std::endl;
#endif // DEBUG

	const Glib::ustring connectionString(label->get_text());

#ifdef DEBUG
	std::cout << "\t" << connectionString << std::endl;
#endif // DEBUG

	// Search for "@" in string
	auto index = connectionString.rfind('@');

	// Check if search was successful and exit if not
	if(index++ == -1) return;

	// Set IP Address entry to substring
	ipAddressEntry->set_text(connectionString.substr(index, connectionString.length() - index));
}

/** @brief Shuts down robot after user confirms.
 *
 * Callback function for the Shutdown Robot Button. Only works if connected,
 * otherwise show an error.
 *
 * @param[in]   parentWindow    The window that the message dialog will be spawned in.
 * @return void
 * */
void shutdownDialog(Gtk::Window* parentWindow) {
	if(!connected) {
		Gtk::MessageDialog dialog(*parentWindow, "Not connected to robot!", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
		dialog.run();
		return;
	}

	Gtk::MessageDialog dialog(*parentWindow, "Shutdown now?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);

	if(dialog.run() == Gtk::RESPONSE_OK) {
#ifdef DEBUG
		std::cout << "Shutdown robot." << std::endl;
#endif // DEBUG

		constexpr uint8_t messageSize = 2;
		constexpr uint8_t command = COMMAND_TO_ROBOT_SHUTDOWN;
		uint8_t message[messageSize];
		message[0] = messageSize;
		message[1] = command;
		send(sock, message, messageSize, 0);
	}
}

/** @brief Sends when a specific key is released to the robot.
 *
 * Sends a message to the robot when a key is released on the keyboard.
 * Callback for window.
 *
 * @return  false
 * @retval  false   Always returns false.
 * */
bool on_key_release_event(GdkEventKey* keyEvent) {
#ifdef DEBUG
	std::cout << "Key released:" << std::endl;
	std::cout << "\t" << std::hex << keyEvent->keyval << "  " << keyEvent->state << std::dec << std::endl;
#endif // DEBUG

	if(sock) {
		constexpr uint8_t messageSize = 5;
		constexpr uint8_t command = COMMAND_TO_ROBOT_KEYBOARD_EVENT;
		uint8_t message[messageSize];
		message[0] = messageSize;
		message[1] = command;
		serialize<uint16_t>(keyEvent->keyval, &message[2], sizeof(message) - (2 * sizeof(message[0])));
		message[4] = 0;
		send(sock, message, messageSize, 0);
#ifdef DEBUG
		std::cout << std::hex << (uint)message[2] << "  " << (uint)message[3] << std::dec << std::endl;
#endif // DEBUG
	}
	return false;
}

/** @brief Sends when a specific key is pressed to the robot.
 *
 * Sends a message to the robot when a key is pressed on the keyboard.
 * Callback for window.
 *
 * @see window
 *
 * @return  false
 * @retval  false   Always returns false.
 * */
bool on_key_press_event(GdkEventKey* keyEvent) {
#ifdef DEBUG
	std::cout << "Key pressed:" << std::endl;
	std::cout << "\t" << std::hex << keyEvent->keyval << "\t" << keyEvent->state << std::dec << std::endl;
#endif // DEBUG

	if(sock) {
		constexpr uint8_t messageSize = 5;
		constexpr uint8_t command = COMMAND_TO_ROBOT_KEYBOARD_EVENT;
		uint8_t message[messageSize];
		message[0] = messageSize;
		message[1] = command;
		serialize<uint16_t>(keyEvent->keyval, &message[2], sizeof(message) - (2 * sizeof(message[0])));
		message[4] = 1;
		send(sock, message, messageSize, 0);
#ifdef DEBUG
		std::cout << std::hex << (uint)message[2] << "  " << (uint)message[3] << std::dec << std::endl;
#endif // DEBUG
	}
	return false;
}

/** @brief Creates, sets up, and displays the GUI window.
 *
 * Creates and sets up all sub-elements of the main window.
 * Generates main window and displays it.
 *
 * @see window
 * @see voltageLabel
 * @see numCurrents
 * @see currentLabels
 * @see addressListBox
 * @see ipAddressEntry
 * @see connectionStatusLabel
 * @see silentRunButton
 * @see connectButton
 * @see controlModeLabel
 * @see numTalonInfoFrames
 * @see talonInfoFrames
 * @see numVictorInfoFrames
 * @see victorInfoFrames
 * @see BoxFactory
 * @see ButtonFactory
 * @see InfoFrame
 * @see InfoItem
 * @see TalonInfoFrame
 * @see WindowFactory
 *
 * @return void
 * */
void setupGui(const Glib::RefPtr<Gtk::Application>& application) {
	// Create List Box
	addressListBox = Gtk::manage(new Gtk::ListBox());
	addressListBox->signal_row_activated().connect(&rowActivated);
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
	// Set status label to red
	Gdk::RGBA red;
	red.set_rgba(1.0, 0.0, 0.0, 1.0);
	connectionStatusLabel->override_background_color(red);

	// IP Address Label
	const auto ipAddressLabel = Gtk::manage(new Gtk::Label("IP Address"));

	// IP Address Entry
	ipAddressEntry = Gtk::manage(new Gtk::Entry());
	ipAddressEntry->set_can_focus(true);
	ipAddressEntry->set_editable(true);

	// Mode Label
	const auto modeLabel = Gtk::manage(new Gtk::Label("Control Mode: "));
	controlModeLabel = Gtk::manage(new Gtk::Label("Drive"));

	// Silent Run Button
	const auto silentRunButton = ButtonFactory("Silent Running")
									 .addClickedCallback<std::function<typeof(silentRun)>>(silentRun)
									 .build();

	// Shutdown Robot Button
	const auto shutdownCallback = [] { return shutdownDialog(window); };
	const auto shutdownRobotButton = ButtonFactory("Shutdown Robot")
										 .addClickedCallback<typeof(shutdownCallback)>(shutdownCallback)
										 .build();

	// Power Distribution Frame
	const auto powerDistributionPanelFrame = Gtk::manage(new Gtk::Frame("Power Distribution Panel"));

	// Power Labels
	voltageLabel = Gtk::manage(new Gtk::Label("0"));

	for(auto& label : currentLabels) {
		label = Gtk::manage(new Gtk::Label("0"));
	}

	// Power Boxes
	const auto voltageBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
								.addFrontLabel("Voltage:")
								.packEnd(voltageLabel)
								.build();

	Gtk::Box* currentBoxes[numCurrents];
	for(size_t i = 0; i < numCurrents; i++) {
		currentBoxes[i] = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
							  .addFrontLabel("Current " + std::to_string(i) + ":")
							  .packEnd(currentLabels[i])
							  .build();
	}

	// Power Distribution Panel Box
	auto powerDistributionPanelBoxFactory = BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
												.addWidget(voltageBox);
	for(const auto& box : currentBoxes) {
		powerDistributionPanelBoxFactory.addWidget(box);
	}
	const auto powerDistributionPanelBox = powerDistributionPanelBoxFactory.build();

	powerDistributionPanelFrame->add(*powerDistributionPanelBox);

	// Info Frame
	// Talon Info Frames
	for(size_t i = 0; i < numTalonInfoFrames; i++) {
		talonInfoFrames[i] = new TalonInfoFrame("Talon " + std::to_string(1 + i));
	}

	// Victor Info Frames
	for(size_t i = 0; i < numVictorInfoFrames; i++) {
		victorInfoFrames[i] = new VictorInfoFrame("Victor " + std::to_string(1 + i));
	}

	// Remote Control Box
	const auto remoteControlBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
									  .addWidget(shutdownRobotButton)
									  .build();

	// State Box
	const auto stateBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
							  .addWidget(silentRunButton)
							  .addWidget(modeLabel)
							  .addWidget(controlModeLabel)
							  .build();
	// Connect Box
	const auto connectBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL, 5)
								.addWidget(ipAddressLabel)
								.addWidget(ipAddressEntry)
								.addWidget(connectButton)
								.addWidget(connectionStatusLabel)
								.build();

	// Victor 1 Box
	auto victor1BoxFactory = BoxFactory(Gtk::ORIENTATION_VERTICAL);
	for(const auto& victorInfoFrame : victorInfoFrames) {
		victor1BoxFactory.addWidget(victorInfoFrame);
	}
	const auto victor1Box = victor1BoxFactory.build();

	// Controls Right Box
	const auto controlsRightBox = BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
									  .addWidget(connectBox)
									  .addWidget(stateBox)
									  .addWidget(remoteControlBox)
									  .build();

	// Talon Box
	auto talonBoxFactory = BoxFactory(Gtk::ORIENTATION_VERTICAL);
	for(const auto& talonInfoFrame : talonInfoFrames) {
		talonBoxFactory.addWidget(talonInfoFrame);
	}
	const auto talonBox = talonBoxFactory.build();

	// Controls Box
	const auto controlsBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL, 5)
								 .addWidget(scrolledList)
								 .addWidget(controlsRightBox)
								 .build();

	// Sensor Box
	const auto sensorBox = BoxFactory(Gtk::ORIENTATION_HORIZONTAL)
							   .addWidget(powerDistributionPanelFrame)
							   .addWidget(talonBox)
							   .addWidget(victor1Box)
							   .build();

	const auto parentBox = BoxFactory(Gtk::ORIENTATION_VERTICAL, 5)
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

/** @brief Checks if a string is contained in a vector of strings.
 *
 * Loops through each element in a vector of strings to find if a
 * given string fully matches.
 *
 * @return If string is contained in vector
 * @retval  true    String is in vector
 * @retval  false   String not in vector
 * */
bool contains(std::vector<std::string>& list, std::string& value) {
	return std::any_of(list.begin(), list.end(), [value](const std::string& storedValue) {
		return storedValue == value;
	});
}

/** @brief Checks if a robot is contained in a vector.
 *
 * Loops through each element in a vector of strings to find if a
 * given string fully matches.
 *
 * @see RemoteRobot
 *
 * @param[in] list      Vector of Remote Robots
 * @param[in] robotTag  String for the robot tag
 * @return If tag is contained in list
 * @retval  true    Robot tag is in list
 * @retval  false   Robot tag not in list
 * */
bool contains(std::vector<RemoteRobot>& list, std::string& robotTag) {
	return std::any_of(list.begin(), list.end(), [robotTag](const RemoteRobot& storedValue) {
		return robotTag == storedValue.tag;
	});
}

/** @brief Set last seen time for a robot.
 *
 * Sets the list seen time for s specified robot.
 *
 * @see RemoteRobot
 *
 * @param[in] list      Vector of Remote Robots
 * @param[in] robotTag  String for the robot tag
 * @return void
 * */
void update(std::vector<RemoteRobot>& list, std::string& robotTag) {
	for(auto& remoteRobot : list) {
		time_t now;
		time(&now);
		remoteRobot.lastSeenTime = now;
	}
}

/** @brief Get IP addresses of local computer.
 *
 * Returns non-loopback IP addresses of computer running the client.
 *
 * @return Vector of IP addresses as a string
 * */
std::vector<std::string> getAddressList() {
	std::vector<std::string> addressList;
	ifaddrs* interfaceAddresses = nullptr;
	for(const auto failed = getifaddrs(&interfaceAddresses); !failed && interfaceAddresses != nullptr; interfaceAddresses = interfaceAddresses->ifa_next) {
		if(interfaceAddresses->ifa_addr != nullptr && interfaceAddresses->ifa_addr->sa_family == AF_INET) {
			auto* socketAddress = reinterpret_cast<sockaddr_in*>(interfaceAddresses->ifa_addr);
			std::string addressString(inet_ntoa(socketAddress->sin_addr));

			// Ignore loopback addresses
			if(addressString == "0.0.0.0") continue;
			if(addressString == "127.0.0.1") continue;

			// Ignore already found addresses
			if(contains(addressList, addressString)) continue;

			addressList.push_back(addressString);
		}
	}
	return addressList;
}

/** @brief Listen to multicast.
 *
 * Function for thread to listen to network multicast.
 * Joins the multicast group 226.1.1.1 on the local 203.106.93.94 interface.
 * Runs infinite loop for event loop (no exit condition).
 *
 * @return void
 * */
void broadcastListen() {
	const auto sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0) {
		std::cerr << "Opening datagram socket error!" << std::endl;
		return;
	}

	constexpr int reuse = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		std::cerr << "Setting SO_REUSEADDR error!" << std::endl;
		close(sd);
		return;
	}

	// Bind to the proper port number with the IP address specified as INADDR_ANY.
	struct sockaddr_in localSock {};
	localSock.sin_family = AF_INET;
	localSock.sin_port = htons(4321);
	localSock.sin_addr.s_addr = INADDR_ANY;
	if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
		std::cerr << "Binding datagram socket error!" << std::endl;
		close(sd);
		return;
	}

	const auto addressList = getAddressList();
	for(const auto& addressString : addressList) {
#ifdef DEBUG
		std::cout << "Client Address: " << addressString << std::endl;
#endif // DEBUG

		// Join the multicast groups
		struct ip_mreq group {};
		group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
		group.imr_interface.s_addr = inet_addr(addressString.c_str());
		if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&group, sizeof(group)) < 0) {
			std::cerr << "Adding multicast group error!" << std::endl;
		}
	}

	char databuf[1024];
	constexpr int datalen = sizeof(databuf);
	while(true) {
		if(read(sd, databuf, datalen) >= 0) {
			std::string message(databuf);
			if(!contains(RemoteRobot::robotList, message)) {
				RemoteRobot remoteRobot;
				remoteRobot.tag = message;
				time(&remoteRobot.lastSeenTime);
				RemoteRobot::robotList.push_back(remoteRobot);
			}
			update(RemoteRobot::robotList, message);
		}
	}
}

/** @brief Display connected robots on the GUI.
 *
 * Removes robots if they timeout (12 seconds).
 * Display robots to the Address List Box.
 * Remove old robots from the GUI.
 *
 * @return void
 * */
void robotList() {
	// Remove robots that timeout (12 seconds)
	for(size_t index = 0; index < RemoteRobot::robotList.size(); ++index) {
		time_t now;
		time(&now);
		if(now - RemoteRobot::robotList[index].lastSeenTime > 12) {
			RemoteRobot::robotList.erase(RemoteRobot::robotList.begin() + index--);
		}
	}

	// Add new elements
	for(const auto& remoteRobot : RemoteRobot::robotList) {
		const auto robotId = remoteRobot.tag;
		bool match = false;
		int index = 0;
		for(auto listBoxRow = addressListBox->get_row_at_index(index); listBoxRow; listBoxRow = addressListBox->get_row_at_index(++index)) {
			const auto* label = dynamic_cast<Gtk::Label*>(listBoxRow->get_child());
			Glib::ustring addressString = label->get_text();
			if(robotId == addressString) {
				match = true;
				break;
			}
		}
		if(!match) {
			const auto label = Gtk::manage(new Gtk::Label(robotId));
			label->set_visible(true);
			addressListBox->append(*label);
		}
	}

	// Remove old element
	int index = 0;
	for(auto listBoxRow = addressListBox->get_row_at_index(index); listBoxRow; listBoxRow = addressListBox->get_row_at_index(++index)) {
		const auto* label = dynamic_cast<Gtk::Label*>(listBoxRow->get_child());
		const auto addressString = label->get_text();
		bool match = false;
		for(const RemoteRobot& remoteRobot : RemoteRobot::robotList) {
			std::string robotId = remoteRobot.tag;
			if(robotId == addressString) {
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

/** @brief Display information about a Victor motor.
 *
 * Parses a message byte array and displays the information to the given Victor
 * Info Frame.
 *
 * @param[in]   victorInfoFrame The frame to display the information on
 * @param[in]   headMessage     The message to be parsed
 * @return void
 * */
void displayVictorInfo(VictorInfoFrame* victorInfoFrame, uint8_t* headMessage) {
	const auto deviceId = deserialize<int32_t>(&headMessage[1], 4);
	const auto busVoltage = deserialize<float>(&headMessage[5], 4);
	const auto outputVoltage = deserialize<float>(&headMessage[9], 4);
	const auto outputPercent = deserialize<float>(&headMessage[13], 4);
	victorInfoFrame->setItem("Device ID", deviceId);
	victorInfoFrame->setItem("Bus Voltage", busVoltage);
	victorInfoFrame->setItem("Output Voltage", outputVoltage);
	victorInfoFrame->setItem("Output Percent", outputPercent);
}

/** @brief Display information about a Talon motor.
 *
 * Parses a message byte array and displays the information to the given Talon
 * Info Frame.
 *
 * @param[in]   talonInfoFrame  The frame to display the information on
 * @param[in]   headMessage     The message to be parsed
 * @return void
 * */
void displayTalonInfo(TalonInfoFrame* talonInfoFrame, uint8_t* headMessage) {
	const auto deviceId = deserialize<int32_t>(&headMessage[1], 4);
	const auto busVoltage = deserialize<float>(&headMessage[5], 4);
	const auto outputCurrent = deserialize<float>(&headMessage[9], 4);
	const auto outputVoltage = deserialize<float>(&headMessage[13], 4);
	const auto outputPercent = deserialize<float>(&headMessage[17], 4);
	const auto temperature = deserialize<float>(&headMessage[21], 4);
	const auto sensorPosition = deserialize<int32_t>(&headMessage[25], 4);
	const auto sensorVelocity = deserialize<int32_t>(&headMessage[29], 4);
	const auto closedLoopError = deserialize<int32_t>(&headMessage[33], 4);
	const auto integralAccumulator = deserialize<int32_t>(&headMessage[37], 4);
	const auto errorDerivative = deserialize<int32_t>(&headMessage[41], 4);
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

/** @brief Entry point.
 *
 * Runs the program. Event loop is an infinite loop (no exit condition).
 * Sets up the client.
 * Reads and parses messages.
 *
 * @param[in]   argc    Number of arguments
 * @param[in]   argv    Arguments
 * @return  Program status
 * @retval  0    Success
 * @retval  Non-Zero   Error
 * */
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

	// Initialize joystick(s)
	int joystickCount = SDL_NumJoysticks();
#ifdef DEBUG
	std::cout << "Number of joysticks: " << joystickCount << std::endl;
#endif // DEBUG

	SDL_Joystick* joystickList[joystickCount];

	if(joystickCount > 0) {
		for(int joystickIndex = 0; joystickIndex < joystickCount; joystickIndex++) {
			joystickList[joystickIndex] = SDL_JoystickOpen(joystickIndex);
#ifdef DEBUG
			if(joystickList[joystickIndex]) {
				std::cout << "Opened Joystick " << joystickIndex << std::endl;
				std::cout << "\tName: " << SDL_JoystickName(joystickList[joystickIndex]) << std::endl;
				std::cout << "\tNumber of Axes: " << SDL_JoystickNumAxes(joystickList[joystickIndex]) << std::endl;
				std::cout << "\tNumber of Buttons: " << SDL_JoystickNumButtons(joystickList[joystickIndex]) << std::endl;
				std::cout << "\tNumber of Balls: " << SDL_JoystickNumBalls(joystickList[joystickIndex]) << std::endl;
			} else {
				std::cout << "Couldn't open Joystick " << joystickIndex << std::endl;
			}
#endif // DEBUG
		}
	}

	SDL_Event event;
	char buffer[1024] = {0};

	std::list<uint8_t> messageBytesList;
	uint8_t headMessage[256];

	// Main event loop
	while(true) {
		robotList();

		// Run GTK events
		while(Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}

		// Don't do anything if not connected
		if(!connected) continue;

		// Read from the socket
		auto bytesRead = read(sock, buffer, sizeof(buffer) / sizeof(buffer[0]));
		for(ssize_t index = 0; index < bytesRead; index++) {
			messageBytesList.push_back(buffer[index]);
		}

		// Check for message head and disconnect if no bytes
		if(!bytesRead) {
#ifdef DEBUG
			std::cout << "Lost Connection." << std::endl;
#endif // DEBUG
			setDisconnectedState();
			continue;
		}

		// Handle messages
		while(!messageBytesList.empty() && messageBytesList.front() <= messageBytesList.size()) {
			// Read message
			auto messageSize = messageBytesList.front();
			messageBytesList.pop_front();
			messageSize--;
			for(unsigned char index = 0; index < messageSize; index++) {
				headMessage[index] = messageBytesList.front();
				messageBytesList.pop_front();
			}

#ifdef DEBUG
			std::cout << "Got Message" << std::endl;
#endif // DEBUG
	   // Get command
			uint8_t command = headMessage[0];
#ifdef DEBUG
			std::cout << "Command: " << command << std::endl;
#endif // DEBUG

			// Handle command
			switch(command) {
				case COMMAND_TO_CLIENT_POWER_DISTRIBUTION_PANEL: {
					// Display voltage
					const auto voltage = deserialize<float>(&headMessage[1], 4);
					voltageLabel->set_label(std::to_string(voltage).c_str());

					// Display Currents
					for(size_t index = 0; index < numCurrents; index++) {
						const auto current = deserialize<float>(&headMessage[5 + index * sizeof(float)], 4);
						currentLabels[index]->set_label(std::to_string(current).c_str());
					}
				} break;

				case COMMAND_TO_CLIENT_TALON_1:
					displayTalonInfo(talonInfoFrames[0], headMessage);
					break;

				case COMMAND_TO_CLIENT_TALON_2:
					displayTalonInfo(talonInfoFrames[1], headMessage);
					break;

				case COMMAND_TO_CLIENT_VICTOR_1:
					displayVictorInfo(victorInfoFrames[0], headMessage);
					break;

				case COMMAND_TO_CLIENT_VICTOR_2:
					displayVictorInfo(victorInfoFrames[1], headMessage);
					break;

				case COMMAND_TO_CLIENT_VICTOR_3:
					displayVictorInfo(victorInfoFrames[2], headMessage);
					break;

				case COMMAND_TO_CLIENT_CONTROL_STATUS: {
					const int mode = headMessage[1];
#ifdef DEBUG
					std::cout << "Mode: " << mode << std::endl;
#endif // DEBUG

					switch(mode) {
						case CONTROL_MODE_DRIVE:
							controlModeLabel->set_label("Drive");
							break;
						case CONTROL_MODE_DIG:
							controlModeLabel->set_label("Dig");
							break;
					}
				} break;
				default:
					break;
			}
		}

		// Handle SDL joystick events
		while(SDL_PollEvent(&event)) {
			const auto state = SDL_GetKeyboardState(nullptr);

#ifdef DEBUG
			const size_t length = sizeof(state) / sizeof(state[0]);
			std::cout << "Length: " << length << std::endl;
			std::cout << "Scan code A: " << (int)state[SDL_SCANCODE_A] << std::endl;
#endif // DEBUG

			switch(event.type) {
				case SDL_MOUSEMOTION:
#ifdef DEBUG
					std::cout << "X: " << event.motion.x << ", Y: " << event.motion.y << std::endl;
#endif // DEBUG
					break;

				case SDL_KEYDOWN:
#ifdef DEBUG
					std::cout << event.key.keysym.sym << " Key Down" << std::endl;
#endif // DEBUG
					break;

				case SDL_KEYUP:
#ifdef DEBUG
					std::cout << event.key.keysym.sym << " Key Up" << std::endl;
#endif // DEBUG
					break;

				case SDL_JOYHATMOTION: {
#ifdef DEBUG
					std::cout << "Joystick " << event.jhat.which << " ";
					std::cout << "Timestamp " << event.jhat.timestamp << " ";
					std::cout << "Hat " << (uint32_t)event.jhat.hat << " ";
					std::cout << "Value " << (uint32_t)event.jhat.value << std::endl;
#endif // DEBUG

					constexpr uint8_t command = COMMAND_TO_ROBOT_JOYSTICK_HAT_MOTION_EVENT;
					constexpr int length = 5;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jhat.which;
					message[3] = event.jhat.hat;
					message[4] = event.jhat.value;

					send(sock, message, length, 0);
				} break;

				case SDL_JOYBUTTONDOWN: {
#ifdef DEBUG
					std::cout << "Joystick: " << event.jbutton.which << " ";
					std::cout << "Timestamp: " << event.jbutton.timestamp << " ";
					std::cout << "Button: " << (uint32_t)event.jbutton.button << " ";
					std::cout << "State: " << (uint32_t)event.jbutton.state << std::endl;
#endif // DEBUG

					constexpr uint8_t command = COMMAND_TO_ROBOT_JOYSTICK_BUTTON_EVENT;
					constexpr int length = 5;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jbutton.which;
					message[3] = event.jbutton.button;
					message[4] = event.jbutton.state;

					send(sock, message, length, 0);
				} break;

				case SDL_JOYBUTTONUP: {
#ifdef DEBUG
					std::cout << "Joystick: " << event.jbutton.which << " ";
					std::cout << "Timestamp: " << event.jbutton.timestamp << " ";
					std::cout << "Button: " << (uint32_t)event.jbutton.button << " ";
					std::cout << "State: " << (uint32_t)event.jbutton.state << std::endl;
#endif // DEBUG

					constexpr uint8_t command = COMMAND_TO_ROBOT_JOYSTICK_BUTTON_EVENT;
					constexpr int length = 5;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jbutton.which;
					message[3] = event.jbutton.button;
					message[4] = event.jbutton.state;

					send(sock, message, length, 0);
				} break;

				case SDL_JOYAXISMOTION: {
#ifdef DEBUG
					std::cout << "Joystick: " << event.jaxis.which << " ";
					std::cout << "Timestamp: " << event.jaxis.timestamp << " ";
					std::cout << "Axis: " << (int)event.jaxis.axis << " ";
					std::cout << "Value: " << event.jaxis.value << std::endl;
#endif // DEBUG

					constexpr uint8_t command = COMMAND_TO_ROBOT_JOYSTICK_AXIS_MOTION_EVENT;
					const float value = (float)event.jaxis.value / -32768.f;

					constexpr int length = 8;
					uint8_t message[length];
					message[0] = length;
					message[1] = command;
					message[2] = event.jaxis.which;
					message[3] = event.jaxis.axis; // 0-roll 1-pitch 2-throttle 3-yaw
					serialize(value, &message[4], sizeof(message) - (4 * sizeof(message[0])));

					send(sock, message, length, 0);
				} break;
			}
		}
	}
}
