//
// Created by luke on 1/29/22.
//

#include "WindowFactory.hpp"
WindowFactory::WindowFactory() {
	window = new Gtk::Window();
}

WindowFactory& WindowFactory::addWidget(Gtk::Widget* widget) {
	widgets.insert(widgets.end(), widget);
	return *this;
}

Gtk::Window* WindowFactory::build() {
	// Add widgets
	for(auto widget : widgets) {
		window->add(*widget);
	}

	return window;
}
WindowFactory& WindowFactory::setTitle(const Glib::ustring& title) {
	window->set_title(title);
	return *this;
}
