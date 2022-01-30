//
// Created by luke on 1/29/22.
//

#include "VictorInfoFrame.hpp"
VictorInfoFrame::VictorInfoFrame(const std::string& name, bool managed) : InfoFrame(name) {
	for(auto item : items) {
		addItem(item);
	}
	if(managed) Gtk::manage(this);
}
