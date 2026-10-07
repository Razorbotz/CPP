#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include <gtkmm.h>
#include <gdkmm.h>

#include <InfoItem.hpp>

class InfoFrame:public Gtk::Frame{
	private:
	Gtk::Frame* frame;
	Gtk::Box* contentsBox;
	
	std::vector<std::shared_ptr<InfoItem>> itemList;

	public:
	InfoFrame(std::string frameName);
	void addItem(std::string itemName);
	void setItem(std::string itemName, std::string itemValue);
	void setItem(std::string itemName, float itemValue);
	void setItem(std::string itemName, int itemValue);
	void setItem(std::string itemName, bool itemValue);

};
