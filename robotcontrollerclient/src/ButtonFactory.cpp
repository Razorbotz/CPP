//
// Created by luke on 1/30/22.
//

#include "ButtonFactory.hpp"

ButtonFactory::ButtonFactory(const char* label, const bool managed) {
	button = new Gtk::Button(label);

	if(managed) manage();
}

ButtonFactory& ButtonFactory::manage() {
	Gtk::manage(button);
	return *this;
}

Gtk::Button* ButtonFactory::build() {
	return button;
}
