#include "message.hpp"



Gut::Message::Message(const String& content, SOCKET recipient){
	this->content = content;
	this->recipient = recipient;
}


Gut::String& Gut::Message::getContent(){
	return content;
}

SOCKET Gut::Message::getRecipient(){
	return recipient;
}