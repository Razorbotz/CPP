//
// Created by luke on 1/30/22.
//

#ifndef CONTROL_BUTTONFACTORY_HPP
#define CONTROL_BUTTONFACTORY_HPP

#include <gdkmm.h>
#include <gtkmm.h>
#include <vector>

class ButtonFactory {
  private:
	Gtk::Button* button;

  public:
	ButtonFactory(const char* label, const bool managed = true);
	ButtonFactory& manage();

	template <class T>
	ButtonFactory& addClickedCallback(T callback) {
		button->signal_clicked().connect(callback);

		return *this;
	}
	Gtk::Button* build();
};

#endif // CONTROL_BUTTONFACTORY_HPP
