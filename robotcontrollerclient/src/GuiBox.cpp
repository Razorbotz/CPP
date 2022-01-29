//
// Created by luke on 1/29/22.
//

#include "GuiBox.hpp"

Gtk::Box* makeBox(Gtk::Orientation orientation, int spacing) {
  return Gtk::manage(new Gtk::Box(orientation, spacing));
}
