#include "InfoItem.hpp"
#include <cstdlib>
#include <string>

InfoItem::InfoItem(std::string name) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 2) {
	nameLabel = Gtk::manage(new Gtk::Label(name));
	valueLabel = Gtk::manage(new Gtk::Label("0"));

	this->pack_start(*nameLabel, false, true, 10);
	this->pack_end(*valueLabel, false, true, 10);
}

InfoItem::~InfoItem() {
	delete nameLabel;
	delete valueLabel;
}

void InfoItem::setName(std::string name) {
	nameLabel->set_text(name);
}

std::string InfoItem::getName() const {
	return nameLabel->get_text();
}

std::string InfoItem::getValue() const {
	return valueLabel->get_text();
}
int InfoItem::getValueAsInt() const {
	return std::stoi(getValue());
}
float InfoItem::getValueAsFloat() const {
	return std::stof(getValue());
}
bool InfoItem::getValueAsBool() const {
	bool valueBool;
	std::string valueString = getValue();
	if(valueString == "true") {
		valueBool = 1;
	} else {
		valueBool = 0;
	}
	return valueBool;
}
