#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include <gtkmm.h>
#include <gdkmm.h>

#include <InfoItem.hpp>

class InfoFrame:public Gtk::Frame{
public:
	Gtk::Frame* frame;
	Gtk::Box* contentsBox;
	
	std::vector<std::shared_ptr<InfoItem>> itemList;

	public:
	InfoFrame(std::string frameName);
	void addItem(std::string itemName);
	void setItem(std::string itemName, std::string itemValue);
    void setItem(std::string itemName, bool itemValue);
    void setItem(std::string itemName, int itemValue);
    void setItem(std::string itemName, long itemValue);
	void setItem(std::string itemName, float itemValue);
    void setItem(std::string itemName, double itemValue);
    void setItem(std::string itemName, uint8_t itemValue);
    void setItem(std::string itemName, uint16_t itemValue);
    void setItem(std::string itemName, uint32_t itemValue);
    void setItem(std::string itemName, uint64_t itemValue);
};
