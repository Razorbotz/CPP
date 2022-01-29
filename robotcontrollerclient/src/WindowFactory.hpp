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
	typedef std::function<bool(GdkEventAny*)> deleteCallback;

  private:
	Gtk::Window* window;
	std::map<Gdk::EventMask, eventCallback> events; // Event and callback map
	std::vector<Gtk::Widget*> widgets;

  public:
	WindowFactory();
	WindowFactory& addEvent(Gdk::EventMask);
	WindowFactory& addEventWithCallback(Gdk::EventMask event, eventCallback callback);
	WindowFactory& addWidget(Gtk::Widget* widget);
	WindowFactory& addDeleteEvent(deleteCallback callback);
	Gtk::Window* build();
};

#endif // CONTROL_WINDOWFACTORY_HPP
