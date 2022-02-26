#ifndef CONTROL_INFOFRAME_HPP
#define CONTROL_INFOFRAME_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

#include <InfoItem.hpp>

class InfoFrame : public Gtk::Frame {
  private:
  std::unique_ptr<Gtk::Box> contentsBox;

	std::vector<std::shared_ptr<InfoItem>> itemList;

  public:
	explicit InfoFrame(const std::string& frameName);
	void addItem(const std::string& itemName);

	template <typename T>
	void setItem(const std::string& itemName, T itemValue) {
		for(const std::shared_ptr<InfoItem>& item : this->itemList) {
			if(item->getName() == itemName) {
				item->setValue(itemValue);
				break;
			}
		}
	}
};

#endif // !CONTROL_INFOFRAME_HPP
