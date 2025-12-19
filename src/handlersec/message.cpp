#include "message.hpp"

//temporary encryption method
Gut::String Gut::Message::encrypt(){
	return content;
}

std::string frameMessage(const std::string& content) {
    uint32_t len = htonl((uint32_t)content.size());

    std::string framed;
    framed.resize(4);
    memcpy(framed.data(), &len, 4);
    framed += content;

    return framed;
}

