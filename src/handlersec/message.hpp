#ifndef MMESSAGE_HPP
#define MMESSAGE_HPP

#include "../libraries.hpp"
#include "client.hpp"

namespace Gut {
    class Message {
        private:
            std::string content;
			SOCKET recipient;
			//encrypts with the clients key
			String encrypt();
			//frames the content | 4-byte length | payload bytes ... |
			String frame();
        public:
			//crates a message
            Message(const std::string& content, const Client* recipient); 
			String decrypt();
			std::string& getContent() const;
			SOCKET getRecipient();
    };
}

#endif