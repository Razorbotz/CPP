/** @file
 * @brief Declares a class, TalonInfoFrame, as an InfoFrame to information about a Talon motor.
 *
 * @author Johnston
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Declares the class TalonInfoFrame, which is a child of GTK InfoFrame, to show information
 * about a Talon motor.
 *
 * @see TalonInfoFrame
 * @see src/TalonInfoFrame.cpp
 * @see include/InfoFrame.hpp
 * @see src/control.cpp
 * */

#ifndef CONTROL_TALONINFOFRAME_HPP
#define CONTROL_TALONINFOFRAME_HPP

#include "InfoFrame.hpp"

/** @brief InfoFrame to display information about Talon motors.
 *
 * Declares the class TalonInfoFrame, which is a child of InfoFrame, to show information
 * about Talon motors. Has a set of predefined values to display.
 *
 * @see include/TalonInfoFrame.hpp
 * @see src/TalonInfoFrame.cpp
 * @see InfoFrame
 * @see src/control.cpp:talonInfoFrames
 * @see src/control.cpp:setupGui
 * @see src/control.cpp:displayTalonInfo
 * */
class TalonInfoFrame : public InfoFrame {
	/// Names of the values for a Talon motor to be displayed
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
		"Error Derivative"};

  public:
	/** @brief Constructor for TalonInfoFrame.
	 *
	 * Constructor for TalonInfoFrame.
	 * Creates all the relevant InfoItems for the frame.
	 * Labels are managed by the TalonInfoFrame frame.
	 *
	 * @param[in]   name    Name of the Talon motor that will be the name of frame.
	 * @param[in]   managed Whether to make the box managed by its parent (will be destructed when parent is). Default is true.
	 * @return TalonInfoFrame object
	 * */
	explicit TalonInfoFrame(const std::string& name, bool managed = true);
};

#endif // !CONTROL_TALONINFOFRAME_HPP
