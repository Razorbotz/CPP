#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "InfoFrame.hpp"

InfoFrame::InfoFrame(std::string frameName) : Gtk::Frame(frameName) {
	contentsBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
	this->add(*contentsBox);
}

void InfoFrame::addItem(std::string itemName) {
	std::shared_ptr<InfoItem> infoItem = std::make_shared<InfoItem>(itemName);
	this->itemList.push_back(infoItem);
	this->contentsBox->add(*(infoItem));
	this->show_all();
}
