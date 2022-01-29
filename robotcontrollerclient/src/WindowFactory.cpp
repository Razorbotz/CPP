//
// Created by luke on 1/29/22.
//

#include "WindowFactory.hpp"
WindowFactory::WindowFactory() {
	window = new Gtk::Window();
}

WindowFactory& WindowFactory::addEvent(Gdk::EventMask event) {
	events.emplace(event, nullptr);
	return *this;
}
WindowFactory& WindowFactory::addEventWithCallback(Gdk::EventMask event, eventCallback callback) {
	events.insert_or_assign(event, callback);
	return *this;
}
WindowFactory& WindowFactory::addWidget(Gtk::Widget* widget) {
	widgets.insert(widgets.end(), widget);
	return *this;
}
WindowFactory& WindowFactory::addDeleteEvent(deleteCallback callback) {
	// Add delete event
	window->signal_delete_event().connect(callback);

	return *this;
}

Gtk::Window* WindowFactory::build() {
	// Add events
	for(auto event : events) {
		// Add event
		window->add_events(event.first);

		// Add event handlers for specific events
		switch(event.first) {
			case Gdk::KEY_PRESS_MASK:
				window->signal_key_press_event().connect(event.second);
				break;
			case Gdk::KEY_RELEASE_MASK:
				window->signal_key_release_event().connect(event.second);
				break;
		}
	}

	// Add widgets
	for(auto widget : widgets) {
		window->add(*widget);
	}

	return window;
}
