//
// Created by luke on 1/29/22.
//

#include "BoxFactory.hpp"
BoxFactory::BoxFactory(Gtk::Orientation orientation, int spacing) {
	box = new Gtk::Box(orientation, spacing);
}
Gtk::Box* BoxFactory::build() { return box; }
