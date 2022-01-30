//
// Created by luke on 1/29/22.
//

#include "TalonInfoFrame.hpp"
TalonInfoFrame::TalonInfoFrame(const std::string& name, bool managed) : InfoFrame(name) {
	for(auto item : items) {
		addItem(item);
	}
	if(managed) Gtk::manage(this);
}
