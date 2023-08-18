/** @file
 * @brief Declares a class, VictorInfoFrame, as an InfoFrame to information about a Victor motor.
 *
 * @author Johnston
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Declares the class VictorInfoFrame, which is a child of GTK InfoFrame, to show information
 * about a Victor motor.
 *
 * @see VictorInfoFrame
 * @see src/VictorInfoFrame.cpp
 * @see include/InfoFrame.hpp
 * @see src/control.cpp
 * */

#ifndef CONTROL_VICTORINFOFRAME_HPP
#define CONTROL_VICTORINFOFRAME_HPP

#include "InfoFrame.hpp"

/** @brief InfoFrame to display information about Victor motors.
 *
 * Declares the class VictorInfoFrame, which is a child of InfoFrame, to show information
 * about Victor motors. Has a set of predefined values to display.
 *
 * @see include/VictorInfoFrame.hpp
 * @see src/VictorInfoFrame.cpp
 * @see InfoFrame
 * @see src/control.cpp:victorInfoFrames
 * @see src/control.cpp:setupGui
 * @see src/control.cpp:displayVictorInfo
 * */
class VictorInfoFrame : public InfoFrame {
	/// Names of the values for a Victor motor to be displayed
	static constexpr const char* items[] = {
		"Device ID",
		"Bus Voltage",
		"Output Voltage",
		"Output Percent"};

  public:
	/** @brief Constructor for VictorInfoFrame.
	 *
	 * Constructor for VictorInfoFrame.
	 * Creates all the relevant InfoItems for the frame.
	 * Labels are managed by the VictorInfoFrame frame.
	 *
	 * @param[in]   name    Name of the Victor motor that will be the name of frame.
	 * @param[in]   managed Whether to make the box managed by its parent (will be destructed when parent is). Default is true.
	 * @return VictorInfoFrame object
	 * */
	explicit VictorInfoFrame(const std::string& name, bool managed = true);
};

#endif // !CONTROL_VICTORINFOFRAME_HPP
