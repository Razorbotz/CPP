//
// Created by luke on 1/29/22.
//

#ifndef CONTROL_VICTORINFOFRAME_HPP
#define CONTROL_VICTORINFOFRAME_HPP

#include "InfoFrame.hpp"
class VictorInfoFrame : public InfoFrame{
	static constexpr const char* items[] = {
		"Device ID",
		"Bus Voltage",
		"Output Voltage",
		"Output Percent"
	};
  public:
	explicit VictorInfoFrame(const std::string& name, bool managed = true);
};

#endif // CONTROL_VICTORINFOFRAME_HPP
