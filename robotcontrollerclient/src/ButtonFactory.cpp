/** @file
 * @brief Implementation for ButtonFactory.
 *
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Implements the ButtonFactory class.
 *
 * @see include/ButtonFactory.hpp
 * @see ButtonFactory
 * */

#include "ButtonFactory.hpp"

ButtonFactory::ButtonFactory(const char* label, const bool managed) {
	button = new Gtk::Button(label);

	if(managed) manage();
}

ButtonFactory& ButtonFactory::manage() {
	Gtk::manage(button);
	return *this;
}

Gtk::Button* ButtonFactory::build() const {
	return button;
}
