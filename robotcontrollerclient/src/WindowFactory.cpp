/** @file
 * @brief Implementation for WindowFactory.
 *
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Implements the WindowFactory class.
 *
 * @see include/BoxFactory.hpp
 * @see BoxFactory
 * */

#include "WindowFactory.hpp"

WindowFactory::WindowFactory() {
	window = new Gtk::Window();
}

WindowFactory& WindowFactory::addWidget(Gtk::Widget* widget) {
	window->add(*widget);

	return *this;
}

Gtk::Window* WindowFactory::build() const {
	return window;
}
WindowFactory& WindowFactory::setTitle(const Glib::ustring& title) {
	window->set_title(title);
	return *this;
}
