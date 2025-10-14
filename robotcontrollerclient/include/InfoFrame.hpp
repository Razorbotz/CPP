#pragma once

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
    void removeItem(std::string itemName);
    void removeAllItems();
    void addWidget(Gtk::Widget& widget);
    template <typename T>
    void setItem(std::string itemName, T itemValue) {
        for (std::shared_ptr<InfoItem> infoItem : itemList) {
            if (infoItem->getName() == itemName) {
                infoItem->setValue(itemValue);
                return;
            }
        }
        addItem(itemName);
        if (!itemList.empty() && itemList.back()->getName() == itemName) {
            itemList.back()->setValue(itemValue);
        } else {
            for (std::shared_ptr<InfoItem> infoItem : itemList) {
                if (infoItem->getName() == itemName) {
                    infoItem->setValue(itemValue);
                    return;
                }
            }
        }
    }

    void setBackground(std::string itemName, std::string color);
    void setTextColor(std::string itemName, std::string color, bool bold);
};
