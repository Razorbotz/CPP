/** @file
 * @brief Declares a class, InfoFrame, as a GTK Frame to show multiple InfoItem objects.
 *
 * @author Johnston
 * @author Luke Simmons
 *
 * @date 2022-2-5
 *
 * Declares the class InfoFrame, which is a child of GTK Frame, to show related
 * InfoItem objects and give an overarching label to them all.
 *
 * @see InfoFrame
 * @see src/InfoFrame.cpp
 * @see include/TalonInfoFrame.hpp
 * @see include/VictorInfoFrame.hpp
 * */

#ifndef CONTROL_INFOFRAME_HPP
#define CONTROL_INFOFRAME_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

#include <InfoItem.hpp>

/** @brief GTK Frame to show multiple InfoItem objects.
 *
 * Declares the class InfoFrame, which is a child of GTK Frame, to show related
 * InfoItem objects and give an overarching label to them all. Can create new InfoItem
 * objects and set their values at any time.
 *
 * @see include/InfoFrame.hpp
 * @see src/InfoFrame.cpp
 * @see InfoItem
 * @see TalonInfoFrame
 * @see VictorInfoFrame
 * */
class InfoFrame : public Gtk::Frame {
  private:
	/// GTK Box to hold all the InfoItem objects.
	std::unique_ptr<Gtk::Box> contentsBox;
	/// List of InfoItem objects to display. \see InfoItem
	std::vector<std::shared_ptr<InfoItem>> itemList;

  public:
	/** @brief Constructor for InfoFrame.
	 *
	 * Constructor for InfoFrame.
	 * Creates the labels to display dynamically on the heap.
	 * Labels are managed by the InfoFrame frame.
	 *
	 * @param[in]   name   Name of the frame to display.
	 * @return InfoFrame object
	 * */
	explicit InfoFrame(const std::string& frameName);

	/** @brief Create a new item and add it to the info frame.
	 *
	 * Adds an item with a specified name to the info frame.
	 * Item is of type InfoItem.
	 * Sets value for item to "0".
	 *
	 * @see InfoItem
	 *
	 * @param[in]   itemName   Name of the information/value to create and display.
	 * @return void
	 * */
	void addItem(const std::string& itemName);

	/** @brief Sets a specified item's value.
	 *
	 * Sets the value for a specified item given its name.
	 * Searches child InfoItem objects for a matching name and sets its value.
	 * If there is no matching name, then nothing is changed.
	 *
	 * @see InfoItem
	 * @see InfoItem:getName
	 * @see InfoItem:setValue
	 *
	 * @param[in]   itemName    Name of the information/value to display.
	 * @param[in]   itemValue   What to set the InfoItem object's value to and display.
	 * @return void
	 * */
	template <typename T>
	void setItem(const std::string& itemName, T itemValue) {
		for(const std::shared_ptr<InfoItem>& item : this->itemList) {
			if(item->getName() == itemName) {
				item->setValue(itemValue);
				break;
			}
		}
	}
};

#endif // !CONTROL_INFOFRAME_HPP
