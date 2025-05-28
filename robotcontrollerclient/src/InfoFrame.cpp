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

void InfoFrame::removeItem(std::string itemName) {
    for (auto it = itemList.begin(); it != itemList.end(); ++it) {
        if ((*it)->getName() == itemName) {
            this->contentsBox->remove(*(*it));
            itemList.erase(it);
            break;
        }
    }
}

void InfoFrame::setItem(std::string itemName, std::string itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }    
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }   
}

void InfoFrame::setItem(std::string itemName, bool itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, int itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, long itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, float itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, double itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, uint32_t itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}


void InfoFrame::setItem(std::string itemName, uint64_t itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, uint8_t itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setItem(std::string itemName, uint16_t itemValue){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setValue(itemValue);
            return;
        }
    }
}

void InfoFrame::setBackground(std::string itemName, std::string color){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setBackground(color);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setBackground(color);
            return;
        }
    }
}

void InfoFrame::setTextColor(std::string itemName, std::string color, bool bold){
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setTextColor(color, bold);
            return;
        }
    }
    addItem(itemName);
    for(std::shared_ptr<InfoItem> infoItem:itemList){
        if(infoItem->getName()==itemName){
            infoItem->setTextColor(color, bold);
            return;
        }
    }
}