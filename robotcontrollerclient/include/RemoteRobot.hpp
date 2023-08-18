/** @file
 * @brief Declares a struct, RemoteRobot, to hold robot information.
 *
 * @author Luke Simmons
 *
 * @date 2022-2-11
 *
 * Declares the struct RemoteRobot to hold information about connected robots.
 *
 * @see src/RemoteRobot.cpp
 * @see src/control.cpp
 * */

#ifndef CONTROL_REMOTEROBOT_HPP
#define CONTROL_REMOTEROBOT_HPP

#include <string>
#include <thread>
#include <vector>

/** @brief Struct to hold information about connected robots.
 *
 * Holds information about connected robots and has a static member to hold
 * list of robots. Contains information including identifier of robot and the
 * last time the robot communicated with the client, which is used to disconnect
 * from it after a specified timeout.
 *
 * @see src/RemoteRobot.cpp
 * @see src/control.cpp:broadcastListen
 * @see src/control.cpp:robotList
 * @see src/control.cpp:contains
 * @see src/control.cpp:update
 * */
struct RemoteRobot {
	/// List to hold robots in
	static std::vector<RemoteRobot> robotList;

	/// Identifier for the robot
	std::string tag;
	/// Last time the robot communicated with the client
	time_t lastSeenTime{};
};

#endif // CONTROL_REMOTEROBOT_HPP
