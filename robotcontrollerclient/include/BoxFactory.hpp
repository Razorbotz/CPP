/** @file
 * @brief Declares a class, BoxFactory, to streamline creating GTK Boxes.
 *
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Declares the class BoxFactory to make creating GKT3 Boxes easier.
 *
 * @see BoxFactory
 * @see src/BoxFactory.cpp
 * @see src/control.cpp
 * */

#ifndef CONTROL_BOXFACTORY_HPP
#define CONTROL_BOXFACTORY_HPP

#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

/** @brief Class to streamline creating GTK Boxes.
 *
 * A class that helps to create GTK3 Boxes easily. Each function call returns
 * a reference to itself, except for build, which returns the pointer to the
 * actual GTK box. Build should be called once per instance of this class.
 *
 * @see include/BoxFactory.hpp
 * @see src/BoxFactory.cpp
 * @see src/control.cpp:setupGui
 * */
class BoxFactory {
  private:
	/// The box that is created
	Gtk::Box* box;

  public:
	/** @brief Constructor for BoxFactory.
	 *
	 * Constructor for BoxFactory.
	 * Set parameters for how box will be laid out and be managed.
	 * Creates a new GTK Box on the heap.
	 *
	 * @param[in]   orientation     Orientation of the box (horizontal or vertical).
	 * @param[in]   spacing         Padding of the child widgets in pixels. Default is 2 px.
	 * @param[in]   managed         Whether to make the box managed by its parent. Default is true. \see manage
	 * @return BoxFactory object
	 * */
	explicit BoxFactory(const Gtk::Orientation orientation, const int spacing = 2, const bool managed = true);

	/** @brief Makes the box managed by its parent.
	 *
	 * Sets the box to be destroyed when its parent is.
	 * Unnecessary to call if managed was set to true in the constructor.
	 *
	 * @see BoxFactory:BoxFactory
	 *
	 * @return Reference to instance of current object.
	 * */
	BoxFactory& manage();

	/** @brief Adds a widget to the box.
	 *
	 * Adds a child widget to the box. Widget should already be initialized
	 * before adding.
	 *
	 * @param[in]   widget  Pointer to widget to add
	 * @return Reference to instance of current object.
	 * */
	BoxFactory& addWidget(Gtk::Widget* widget);

	/** @brief Insert a widget to the left side of the box.
	 *
	 * Adds a child widget to the box. Widget should already be initialized
	 * before adding. Adds it to the far-left of the box, with a specified
	 * padding and layout parameters.
	 *
	 * @param[in]   widget  Pointer to widget to add
	 * @param[in]   expand  If the widget should use extra space allowed to box. Default is false.
	 * @param[in]   fill    If the widget should allocate extra space to child, instead of just padding it like expand. Default is true.
	 * @param[in]   padding How much space between the widget's neighbors, in pixels, the widget should have on all sides in addition to the global value. Default is 10 px.
	 * @return Reference to instance of current object.
	 * */
	BoxFactory& packStart(Gtk::Widget* widget, bool expand = false, bool fill = true, guint padding = 10);

	/** @brief Insert a widget to the right side of the box.
	 *
	 * Adds a child widget to the box. Widget should already be initialized
	 * before adding. Adds it to the far-right of the box, with a specified
	 * padding and layout parameters.
	 *
	 * @param[in]   widget  Pointer to widget to add
	 * @param[in]   expand  If the widget should use extra space allowed to box. Default is false.
	 * @param[in]   fill    If the widget should allocate extra space to child, instead of just padding it like expand. Default is true.
	 * @param[in]   padding How much space between the widget's neighbors, in pixels, the widget should have on all sides in addition to the global value. Default is 10 px.
	 * @return Reference to instance of current object.
	 * */
	BoxFactory& packEnd(Gtk::Widget* widget, bool expand = false, bool fill = true, guint padding = 10);

	/** @brief Insert a text label to the left side of the box.
	 *
	 * Adds a manged text label to the far-left side of the box. Uses default
	 * settings of packStart (expand = false, fill = true, padding = 10 px).
	 *
	 * @param[in]   text    Text the label will display.
	 * @return  Reference to instance of current object.
	 * */
	BoxFactory& addFrontLabel(Glib::ustring text);

	/** @brief Sets up the box and returns it.
	 *
	 * Sets up the box according to all the options in the factory, then
	 * returns a pointer to the finished box.
	 *
	 * @return Pointer to the created box.
	 * */
	Gtk::Box* build() const;
};

#endif // !CONTROL_BOXFACTORY_HPP
