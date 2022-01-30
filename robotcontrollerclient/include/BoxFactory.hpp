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
	BoxFactory& setSizeRequest(int width = -1, int height = -1);
	BoxFactory& packStart(Gtk::Widget* widget, bool expand = false, bool fill = true, guint padding = 10);
	BoxFactory& addFrontLabel(Glib::ustring text);
	BoxFactory& packEnd(Gtk::Widget* widget, bool expand = false, bool fill = true, guint padding = 10);
	Gtk::Box* build();
};

#endif // CONTROL_BOXFACTORY_HPP
