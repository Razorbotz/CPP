#include "InfoItem.hpp"
#include <string>

InfoItem::InfoItem(std::string name) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 2) {
	nameLabel = std::unique_ptr<Gtk::Label>(Gtk::manage(new Gtk::Label(name)));
	valueLabel = std::unique_ptr<Gtk::Label>(Gtk::manage(new Gtk::Label("0")));

	pack_start(*nameLabel, false, true, 10);
	pack_end(*valueLabel, false, true, 10);
}

std::string InfoItem::getName() const {
	return nameLabel->get_text();
}
