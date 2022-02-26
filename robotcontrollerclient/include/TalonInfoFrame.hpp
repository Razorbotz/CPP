//
// Created by luke on 1/29/22.
//

#ifndef CONTROL_TALONINFOFRAME_HPP
#define CONTROL_TALONINFOFRAME_HPP

#include "InfoFrame.hpp"

class TalonInfoFrame : public InfoFrame {
	static constexpr const char* items[] = {
		"Device ID",
		"Bus Voltage",
		"Output Current",
		"Output Voltage",
		"Output Percent",
		"Temperature",
		"Sensor Velocity",
		"Close Loop Error",
		"Integral Accumulator",
		"Error Derivative"
	};
  public:
	explicit TalonInfoFrame(const std::string& name, bool managed = true);
};

#endif // !CONTROL_TALONINFOFRAME_HPP
