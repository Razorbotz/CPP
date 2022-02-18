//
// Created by luke on 1/29/22.
//

#ifndef CONTROL_WINDOWFACTORY_HPP
#define CONTROL_WINDOWFACTORY_HPP

#include <gdkmm.h>
#include <gtkmm.h>
#include <vector>

class WindowFactory {
	typedef std::function<bool(GdkEventKey*)> eventCallback;

  private:
	Gtk::Window* window;
	std::map<Gdk::EventMask, eventCallback> events; // Event and callback map
	std::vector<Gtk::Widget*> widgets;

  public:
	WindowFactory();

	template <class T>
	WindowFactory& addEventWithCallback(Gdk::EventMask event, T callback) {
		window->add_events(event);

		// Add event handlers for specific events
		switch(event) {
			case Gdk::KEY_PRESS_MASK:
				window->signal_key_press_event().connect(callback);
				break;
			case Gdk::KEY_RELEASE_MASK:
				window->signal_key_release_event().connect(callback);
				break;
		}

		return *this;
	}
	WindowFactory& addWidget(Gtk::Widget* widget);

	template <class T>
	WindowFactory& addDeleteEvent(T callback) {
		// Add delete event
		window->signal_delete_event().connect(callback);

		return *this;
	}

	WindowFactory& setTitle(const Glib::ustring& title);
	Gtk::Window* build();
};

#endif // CONTROL_WINDOWFACTORY_HPP
