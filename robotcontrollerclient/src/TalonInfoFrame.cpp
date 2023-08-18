/** @file
 * @brief Implementation for the TalonInfoFrame class.
 *
 * @author Johnston
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Implements the TalonInfoFrame class.
 *
 * @see include/TalonInfoFrame.hpp
 * @see TalonInfoFrame
 * */

#include "TalonInfoFrame.hpp"

TalonInfoFrame::TalonInfoFrame(const std::string& name, bool managed) : InfoFrame(name) {
	for(auto item : items)
		addItem(item);

	if(managed) Gtk::manage(this);
}
