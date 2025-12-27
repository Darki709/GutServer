#include "messageCodec.hpp"
#include "../runtime/client.hpp"

//encrypts the content variable of message using the private keys of the client
void encrypt(Gut::String& content, Gut::Client& client){
	const uint8_t* plainText = reinterpret_cast<const uint8_t*>(content.data());
	std::vector<uint8_t> out;
	if(client.getCipher().cipher.encrypt(client.getCipher().sendNonce, plainText, content.size(), out)){
		client.getCipher().sendNonce++;
		content.assign(out.begin(), out.end()); //set the encrypted text as the content of the message;
	}
	else throw std::runtime_error("encryption failed");
}

//overwrites the content variable of the message with a framed version of it according to the protocol
//message should already come with msg type and content
void Gut::MessageCodec::encode(Message& msg, Client& client){
	char8_t isEncrypted = 0;
	//checks if client's messages should be encrypted
	if((int)client.getState() > 1){
		encrypt(msg.getContent(), client);
		isEncrypted = 1;
	}

	String content = msg.getContent();
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
}

//decrypts the message
void decrypt(Gut::String& content, Gut::Client& client){
	const uint8_t* cipherText = reinterpret_cast<const uint8_t*>(content.data());
	std::vector<uint8_t> out;
	if(client.getCipher().cipher.decrypt(client.getCipher().recvNonce, cipherText, content.size(), out)){
		content.assign(out.begin(), out.end());\
		client.getCipher().recvNonce++;
	}
	else throw std::runtime_error("decryption and authentication failed failed");	
}

//unframes and gets ready for proccessing
void Gut::MessageCodec::decode(Message& msg, Client& client){
	String content = msg.getContent();

	//remove length bytes
	content.erase(0,4);

	//extract the flag
	uint8_t flag = static_cast<uint8_t>(content[0]);
	//remove flag bytes
	content.erase(0,1);

	//checks for a combinatin of either client that should have necrypted messages with flag set to encrypted
	//or a client with no encryption that has a no encryption flag any other combination is invalid and kicked
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