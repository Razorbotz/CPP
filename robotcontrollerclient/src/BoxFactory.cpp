/** @file
 * @brief Implementation for BoxFactory.
 *
 * @author Luke Simmons
 *
 * @date 2022-1-29
 *
 * Implements the BoxFactory class.
 *
 * @see include/BoxFactory.hpp
 * @see BoxFactory
 *
 * */

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

BoxFactory& BoxFactory::setSizeRequest(int width, int height) {
	box->set_size_request(width, height);

	return *this;
}

BoxFactory& BoxFactory::packStart(Gtk::Widget* widget, bool expand, bool fill, guint padding) {
	box->pack_start(*widget, false, true, 10);

	return *this;
}

BoxFactory& BoxFactory::packEnd(Gtk::Widget* widget, bool expand, bool fill, guint padding) {
	box->pack_end(*widget, false, true, 10);

	return *this;
}
BoxFactory& BoxFactory::addFrontLabel(Glib::ustring text) {
	// Creates a managed label with the specified text, then uses default settings of packStart
	packStart(Gtk::manage(new Gtk::Label(text)));

	return *this;
}
