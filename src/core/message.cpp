#include "message.hpp"



Gut::Message::Message(String content, SOCKET recipient) : content(content) , recipient(recipient){
}


Gut::String& Gut::Message::getContent(){
	return content;
}

SOCKET Gut::Message::getRecipient(){
	return recipient;
}