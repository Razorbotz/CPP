/** @file
 * @brief Declares a class, WindowFactory, to streamline creating GTK Windows.
 *
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Declares the class WindowFactory to make creating GKT3 Windows easier.
 *
 * @see WindowFactory
 * @see src/WindowFactory.cpp
 * @see src/control.cpp
 * */

#ifndef CONTROL_WINDOWFACTORY_HPP
#define CONTROL_WINDOWFACTORY_HPP

#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

/** @brief Class to streamline creating GTK Windows.
 *
 * A class that helps to create GTK3 Window easily. Each function call returns
 * a reference to itself, except for build, which returns the pointer to the
 * actual GTK window. Build should be called once per instance of this class.
 *
 * @see include/WindowFactory.hpp
 * @see src/WindowFactory.cpp
 * @see src/control.cpp:setupGui
 * */
class WindowFactory {
  private:
	/// The window that is created
	Gtk::Window* window;

  public:
	/** @brief Constructor for WindowFactory.
	 *
	 * Constructor for WindowFactory.
	 * Creates a new GTK window on the heap.
	 *
	 * @return WindowFactory object
	 * */
	WindowFactory();

	/** @brief Adds a callback for a specified event.
	 *
	 * Adds a callback to a specified GDK Event (specified as an event mask).
	 * Generic function, any kind of function can be passed into callback, as
	 * long as its parameters are compatible with the caller.
	 *
	 * @param[in]   event       Which event to add the callback for (as an event mask). Only key press or a key release is supported.
	 * @param[in]   callback    Function to callback to.
	 * @return Reference to instance of current object.
	 * */
	template <class T>
	WindowFactory& addEventWithCallback(Gdk::EventMask event, T callback) {
		window->add_events(event);

		// Add event handlers for specific events
		if(event & Gdk::KEY_PRESS_MASK) window->signal_key_press_event().connect(callback);
		if(event & Gdk::KEY_RELEASE_MASK) window->signal_key_release_event().connect(callback);
		/// If an unhandled type of event is passed in, it is ignored and no callback is made.

		return *this;
	}

	/** @brief Adds a widget to the window.
	 *
	 * Adds a child widget to the window. Widget should already be initialized
	 * before adding.
	 *
	 * @param[in]   widget  Pointer to widget to add
	 * @return Reference to instance of current object.
	 * */
	WindowFactory& addWidget(Gtk::Widget* widget);

	/** @brief Adds a callback for when the window is deleted.
	 *
	 * Adds a callback to the window for when it is deleted.
	 * Any kind of function can be passed into callback, as long as its
	 * parameters are compatible with the caller.
	 *
	 * @param[in]   callback    Function to callback to.
	 * @return Reference to instance of current object.
	 * */
	template <class T>
	WindowFactory& addDeleteEvent(T callback) {
		// Add delete event
		window->signal_delete_event().connect(callback);

		return *this;
	}

	/** @brief Sets the title of the window.
	 *
	 * Adds a callback to a specified GDK Event (specified as an event mask).
	 * Generic function, any kind of function can be passed into callback, as
	 * long as its parameters are compatible.
	 *
	 * @param[in]   title       Text to set the title of the window to.
	 * @return Reference to instance of current object.
	 * */
	WindowFactory& setTitle(const Glib::ustring& title);

	/** @brief Sets up the window and returns it.
	 *
	 * Sets up the window according to all the options in the factory, then
	 * returns a pointer to the finished window.
	 *
	 * @return Pointer to the created window.
	 * */
	Gtk::Window* build() const;
};

#endif // !CONTROL_WINDOWFACTORY_HPP
