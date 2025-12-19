#ifndef MMESSAGE_HPP
#define MMESSAGE_HPP

#include "../libraries.hpp"
#include "client.hpp"

namespace Gut {
    class Message {
        private:
            std::string content;
			Client* recipient;
			String encrypt();
			String frame();
        public:
			//crates a message, frames the content [4 bytes length, content encrypted with client key]
            Message(const std::string& content, const Client* recipient); 
			String decrypt();
			std::string& getContent() const;
			Client& getRecipient();
    };
}

#endif