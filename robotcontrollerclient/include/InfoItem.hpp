#include <iostream>

#include <gdkmm.h>
#include <gtkmm.h>

class InfoItem : public Gtk::Box {
  private:
	Gtk::Label* nameLabel;
	Gtk::Label* valueLabel;

  public:
	InfoItem(std::string name);
	~InfoItem();

	void setName(std::string name);

	template <typename T>
	void setValue(T value) {
		valueLabel->set_text(std::to_string(value));
	}

	std::string getName() const;

	std::string getValue() const;
	int getValueAsInt() const;
	float getValueAsFloat() const;
	bool getValueAsBool() const;
};
