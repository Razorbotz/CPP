#include <string>
#include <cstdlib>
#include "InfoItem.hpp"

InfoItem::InfoItem(std::string name):Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,2){
        nameLabel=Gtk::manage(new Gtk::Label(name));
        valueLabel=Gtk::manage(new Gtk::Label("0"));

	this->pack_start(*nameLabel,false,true,10);
	this->pack_end(*valueLabel,false,true,10);
}

void InfoItem::setName(std::string name){
	nameLabel->set_text(name);
}

std::string InfoItem::getName(){
	return nameLabel->get_text();
}

void InfoItem::setValue(std::string value){
	valueLabel->set_text(value);
}

void InfoItem::setValue(float value){
	std::string valueString=std::to_string(value);
	setValue(valueString);
}
void InfoItem::setValue(int value){
	std::string valueString=std::to_string(value);
	setValue(valueString);
}
void InfoItem::setValue(bool value){
	std::string valueString=std::to_string(value);
	setValue(valueString);
}
std::string InfoItem::getValue(){
	return valueLabel->get_text();
}
int InfoItem::getValueAsInt(){
	return std::stoi(getValue());
}
float InfoItem::getValueAsFloat(){
	return std::stof(getValue());
}
bool InfoItem::getValueAsBool(){
	bool valueBool;
	std::string valueString=getValue();
	if(valueString=="true"){
		valueBool = 1;
	}else{
		valueBool = 0;
	}
		
}


