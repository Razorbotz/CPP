#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

#include <InfoItem.hpp>

class InfoFrame : public Gtk::Frame {
  private:
	Gtk::Frame* frame;
	Gtk::Box* contentsBox;

	std::vector<std::shared_ptr<InfoItem>> itemList;

  public:
	InfoFrame(std::string frameName);
	~InfoFrame();
	void addItem(std::string itemName);

	template <typename T>
	void setItem(std::string itemName, T itemValue) {
		for(std::shared_ptr<InfoItem> item : this->itemList) {
			if(item->getName() == itemName) {
				item->setValue(itemValue);
				break;
			}
		}
	}
};
