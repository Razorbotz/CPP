//
// Created by luke on 1/29/22.
//

#ifndef CONTROL_BOXFACTORY_HPP
#define CONTROL_BOXFACTORY_HPP

#include <gdkmm.h>
#include <gtkmm.h>
#include <vector>

class BoxFactory {
  private:
	Gtk::Box* box;

  public:
	BoxFactory(Gtk::Orientation orientation, int spacing = 2);
	Gtk::Box* build();
};

#endif // CONTROL_BOXFACTORY_HPP
