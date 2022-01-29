//
// Created by luke on 1/29/22.
//

#include "BoxFactory.hpp"

BoxFactory::BoxFactory(const Gtk::Orientation orientation, const int spacing, const bool managed) {
	box = new Gtk::Box(orientation, spacing);
	if(managed) manage();
}

BoxFactory& BoxFactory::manage() {
	Gtk::manage(box);
	return *this;
}

BoxFactory& BoxFactory::addWidget(Gtk::Widget* widget) {
	widgets.insert(widgets.end(), widget);
	return *this;
}
Gtk::Box* BoxFactory::build() {
	// Add widgets
	for(auto widget : widgets)
		box->add(*widget);

	return box;
}
