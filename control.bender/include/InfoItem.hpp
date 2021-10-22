#include <iostream>

#include <gtkmm.h>
#include <gdkmm.h>

class InfoItem:public Gtk::Box{
	private:
	Gtk::Label* nameLabel;
	Gtk::Label* valueLabel;

	public:
	InfoItem(std::string name);

	void setName(std::string name);
	std::string getName();
	void setValue(std::string value);
	void setValue(float value);
	void setValue(int value);
	void setValue(bool value);
	std::string getValue();
	int getValueAsInt();
	float getValueAsFloat();
	bool getValueAsBool();

};
