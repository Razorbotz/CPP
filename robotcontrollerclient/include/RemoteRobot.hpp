//
// Created by luke on 2/11/22.
//

#ifndef CONTROL_REMOTEROBOT_HPP
#define CONTROL_REMOTEROBOT_HPP

#include <vector>
#include <thread>
#include <string>

struct RemoteRobot {
	static std::vector<RemoteRobot> robotList;

	std::string tag;
	time_t lastSeenTime{};
};

#endif // CONTROL_REMOTEROBOT_HPP
