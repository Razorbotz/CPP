#include <string>
#include <iostream>
#include <vector>
#include <memory>

#include "InfoFrame.hpp"


InfoFrame::InfoFrame(std::string frameName):Gtk::Frame(frameName){
        contentsBox=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL,5));
	this->add(*contentsBox);	
}


void InfoFrame::addItem(std::string itemName){
	std::shared_ptr<InfoItem> infoItem=std::make_shared<InfoItem>(itemName); 
	this->itemList.push_back(infoItem);
	this->contentsBox->add(*(infoItem));
	this->show_all();
}

void InfoFrame::setItem(std::string itemName, std::string itemValue){
	for(std::shared_ptr<InfoItem> infoItem:itemList){
		if(infoItem->getName()==itemName){
			infoItem->setValue(itemValue);
			break;
		}
	}	
}

void InfoFrame::setItem(std::string itemName, float itemValue){
	for(std::shared_ptr<InfoItem> infoItem:itemList){
		if(infoItem->getName()==itemName){
			infoItem->setValue(itemValue);
			break;
		}
	}	

}

void InfoFrame::setItem(std::string itemName, int itemValue){
	for(std::shared_ptr<InfoItem> infoItem:itemList){
		if(infoItem->getName()==itemName){
			infoItem->setValue(itemValue);
			break;
		}
	}	
}

void InfoFrame::setItem(std::string itemName, bool itemValue){
	for(std::shared_ptr<InfoItem> infoItem:itemList){
		if(infoItem->getName()==itemName){
			infoItem->setValue(itemValue);
			break;
		}
	}	
}
