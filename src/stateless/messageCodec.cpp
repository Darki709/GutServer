#include "messageCodec.hpp"
#include "../runtime/client.hpp"
#include <iomanip>

//encrypts the content variable of message using the private keys of the client
void encrypt(Gut::String& content, Gut::Client& client){
	const uint8_t* plainText = reinterpret_cast<const uint8_t*>(content.data());
	std::vector<uint8_t> out;
	if(client.getCipher().cipher.encrypt(client.getCipher().sendNonce, plainText, content.size(), out)){
		int nonce = client.getCipher().sendNonce++;
		std::cout << "encryptin outgoing data" << nonce <<std::endl;
		content.assign(out.begin(), out.end()); //set the encrypted text as the content of the message;
	}
	else throw std::runtime_error("encryption failed");
}

//overwrites the content variable of the message with a framed version of it according to the protocol
//message should already come with msg type and content
void Gut::MessageCodec::encode(Message& msg, Client& client){
	char8_t isEncrypted = 0;
	String& content = msg.getContent();
	//checks if client's messages should be encrypted
	if(static_cast<int>(client.getState()) > 1){
		encrypt(content, client);
		isEncrypted = 1;
	}
	
    uint32_t len = htonl((uint32_t)content.size() + 1); //take into account the isEncrypted length

    String framed;
    framed.resize(4);
	//add the length to the char buffer
    memcpy(framed.data(), &len, 4);
	//add the payload
	framed += isEncrypted;
    framed += content;

	//set the new framed message as the content of the message object
    content = framed;

	//Gut::debugOutgoingMessage(framed);
}

//decrypts the message
void decrypt(Gut::String& content, Gut::Client& client){
	const uint8_t* cipherText = reinterpret_cast<const uint8_t*>(content.data());
	std::vector<uint8_t> out;
	if(client.getCipher().cipher.decrypt(client.getCipher().recvNonce, cipherText, content.size(), out)){
		content.assign(out.begin(), out.end());
		client.getCipher().recvNonce++;
	}
	else throw std::runtime_error("decryption and authentication failed");	
}

//unframes and gets ready for proccessing
void Gut::MessageCodec::decode(Message& msg, Client& client){
	String& content = msg.getContent();


	//extract the flag
	int flag = static_cast<int>(content[0]);
	//remove flag bytes
	content.erase(0,1);

	//checks for a combinatin of either client that should have necrypted messages with flag set to encrypted
	//or a client with no encryption that has a no encryption flag any other combination is invalid and kicked
	std::cout << flag << std::endl;
	if(flag == 1){
		if((int)client.getState() > 1){
			decrypt(msg.getContent(), client);
		}
		else{
			throw std::runtime_error("invalid message");
		}
	}
	else if(flag == 0){
		if((int)client.getState() > 1){
			throw std::runtime_error("invalid message");
		}
	}
	else{
		throw std::runtime_error("invalid message");
	}
}

//for debugging

void Gut::debugOutgoingMessage(const std::string& data) {
    size_t length = data.size();
    std::cout << "=== Outgoing Message Debug ===" << std::endl;
    std::cout << "Total Bytes in buffer: " << length << std::endl;

    if (length < 6) {
        std::cout << "Message too short to contain length + flag + msgType" << std::endl;
        return;
    }

    // Message length prefix (4 bytes, network byte order)
    uint32_t msgLen;
    memcpy(&msgLen, data.data(), 4);
    msgLen = ntohl(msgLen);
    std::cout << "Length Prefix: " << msgLen << " bytes" << std::endl;

    // Flag / isEncrypted
    uint8_t flag = static_cast<uint8_t>(data[4]);
    std::cout << "Flag / IsEncrypted: 0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(flag) << std::dec << " (" << static_cast<int>(flag) << ")" << std::endl;

    // Payload
    std::string payload = data.substr(5);
    std::cout << "Payload length: " << payload.size() << " bytes" << std::endl;

    // Print payload in hex + ASCII
    std::cout << "Payload (hex / ASCII):" << std::endl;
    for (size_t i = 0; i < payload.size(); i += 16) {
        std::cout << std::setw(4) << std::setfill('0') << i << ": ";
        // hex
        for (size_t j = i; j < i + 16 && j < payload.size(); ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(payload[j])) << " ";
        }
        // padding for last line
        for (size_t j = payload.size(); j < i + 16; ++j) std::cout << "   ";

        // ASCII
        std::cout << " | ";
        for (size_t j = i; j < i + 16 && j < payload.size(); ++j) {
            char c = payload[j];
            std::cout << (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
        }
        std::cout << std::endl;
    }

    std::cout << "==============================" << std::endl;
}