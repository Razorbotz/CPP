/** @file
 * @brief Implementation for the InfoFrame class.
 *
 * @author Luke Simmons
 *
 * @date 2022-2-5
 *
 * Implements the InfoFrame class.
 *
 * @see include/InfoFrame.hpp
 * @see InfoFrame
 * */

#include <memory>
#include <vector>

#include "InfoFrame.hpp"

InfoFrame::InfoFrame(const std::string& frameName) : Gtk::Frame(frameName) {
	contentsBox = std::unique_ptr<Gtk::Box>(Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5)));
	this->add(*contentsBox);
}

void InfoFrame::addItem(const std::string& itemName) {
	const auto infoItem = std::make_shared<InfoItem>(itemName);
	itemList.push_back(infoItem);
	contentsBox->add(*(infoItem));
	show_all();
}
