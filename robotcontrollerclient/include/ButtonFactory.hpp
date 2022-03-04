/** @file
 * @brief Declares a class, ButtonFactory, to streamline creating GTK Buttons.
 *
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Declares the class ButtonFactory to make creating GKT3 Buttons easier.
 *
 * @see ButtonFactory
 * @see src/ButtonFactory.cpp
 * @see src/control.cpp
 * */

#ifndef CONTROL_BUTTONFACTORY_HPP
#define CONTROL_BUTTONFACTORY_HPP

#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

/** @brief Class to streamline creating GTK Buttons.
 *
 * A class that helps to create GTK3 buttons easily. Each function call returns
 * a reference to itself, except for build, which returns the pointer to the
 * actual GTK button. Build should be called once per instance of this class.
 *
 * @see include/ButtonFactory.hpp
 * @see src/ButtonFactory.cpp
 * @see src/control.cpp:setupGui
 * */
class ButtonFactory {
  private:
	/// The button that is created
	Gtk::Button* button;

  public:
	/** @brief Constructor for ButtonFactory.
	 *
	 * Constructor for ButtonFactory.
	 * Set parameters for how button will look and be managed.
	 * Creates a new GTK Button on the heap.
	 *
	 * @param[in]   label   Text to make the button display.
	 * @param[in]   managed Whether to make the button managed by its parent. Default is true. \see manage
	 * @return ButtonFactory object
	 * */
	explicit ButtonFactory(const char* label, const bool managed = true);

	/** @brief Makes the button managed by its parent.
	 *
	 * Sets the button to be destroyed when its parent is.
	 * Unnecessary to call if managed was set to true in the constructor.
	 *
	 * @return Reference to instance of current object.
	 * */
	ButtonFactory& manage();

	/** @brief Adds a callback for when the button is clicked.
	 *
	 * Adds a callback to the button for when it is clicked.
	 * Any kind of function can be passed into callback, as long as its
	 * parameters are compatible with the caller.
	 *
	 * @param[in]   callback    Function to callback to.
	 * @return Reference to instance of current object.
	 * */
	template <class T>
	ButtonFactory& addClickedCallback(T callback) {
		button->signal_clicked().connect(callback);

		return *this;
	}

	/** @brief Sets up the button and returns it.
	 *
	 * Sets up the button according to all the options in the factory, then
	 * returns a pointer to the finished button.
	 *
	 * @return Pointer to the created button.
	 * */
	Gtk::Button* build() const;
};

#endif // !CONTROL_BUTTONFACTORY_HPP
