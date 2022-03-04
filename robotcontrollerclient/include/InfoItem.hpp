/** @file
 * @brief Declares a class, InfoItem, as a GTK Box show information.
 *
 * @author Johnston
 * @author Luke Simmons
 *
 * @date 2022-2-5
 *
 * Declares the class InfoItem, which is a child of GTK Box, to show information
 * in the form of a name and a value.
 *
 * @see InfoItem
 * @see src/InfoItem.cpp
 * @see include/InfoFrame.hpp
 * */

#ifndef CONTROL_INFOITEM_HPP
#define CONTROL_INFOITEM_HPP

#include <iostream>
#include <memory>

#include <gdkmm.h>
#include <gtkmm.h>

/** @brief GTK Box to hold a name and value.
 *
 * A subclass of Gtk::Box to display information in a key (name) and value form.
 *
 * @see includeInfoItem.hpp
 * @see src/InfoItem.cpp
 * @see include/InfoFrame.hpp:setItem
 * @see include/InfoFrame.hpp:itemList
 * */
class InfoItem : public Gtk::Box {
  private:
	/// GTK Label for the name
	std::unique_ptr<Gtk::Label> nameLabel;
	/// GTK Label for the value
	std::unique_ptr<Gtk::Label> valueLabel;

  public:
	/** @brief Constructor for InfoItem.
	 *
	 * Constructor for InfoItem.
	 * Creates the labels to display dynamically on the heap.
	 * Labels are managed by the InfoItem box.
	 * Sets value to "0".
	 *
	 * @param[in]   name   Name of the information/value to display.
	 * @return InfoItem object
	 * */
	explicit InfoItem(const std::string& name);

	/** @brief Sets the value of the item.
	 *
	 * Sets the value label to the value of the variable passed in.
	 * Is generic and can take any type that can be converted to a string.
	 * Converts the value to a string and displays it on the value label.
	 *
	 * @param[in]   value    Value to be converted to a string and displayed.
	 * @return void
	 * */
	template <typename T>
	void setValue(T value) {
		valueLabel->set_text(std::to_string(value));
	}

	/** @brief Returns the name of the information.
	 *
	 * Returns the value of nameLabel, which is the name of the information.
	 *
	 * @return Name of information.
	 * */
	std::string getName() const;
};

#endif // !CONTROL_INFOITEM_HPP
