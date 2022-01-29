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
	std::vector<Gtk::Widget*> widgets;

  public:
	BoxFactory(const Gtk::Orientation orientation, const int spacing = 2, const bool managed = true);
	BoxFactory& manage();
	BoxFactory& addWidget(Gtk::Widget* widget);
	Gtk::Box* build();
};

#endif // CONTROL_BOXFACTORY_HPP
