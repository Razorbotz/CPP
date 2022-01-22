#include <iostream>
#include <memory>

#include <gdkmm.h>
#include <gtkmm.h>

class InfoItem : public Gtk::Box {
  private:
	std::unique_ptr<Gtk::Label> nameLabel;
	std::unique_ptr<Gtk::Label> valueLabel;

  public:
	InfoItem(std::string name);

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
