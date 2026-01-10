#include "client.hpp"

Gut::Client::Client(SOCKET socket) : clientSocket(socket) , state(ClientState::CONNECTED){}

Gut::Client::~Client(){
	incomingBuffer.clear();
	outgoingBuffer.clear();
}

SOCKET Gut::Client::getSocket(){
	return clientSocket;
}

void Gut::Client::pushInBuffer(const String& content){
	incomingBuffer.append(content);
}

void Gut::Client::pushOutBuffer(const String& content){
	outgoingBuffer.append(content);
}

void Gut::Client::popInBuffer(int len){
	incomingBuffer.erase(0, len);
}


void Gut::Client::popOutBuffer(int len){
	outgoingBuffer.erase(0, len);
}

const Gut::String& Gut::Client::getInBuffer(){
	return incomingBuffer;
}

const Gut::String& Gut::Client::getOutBuffer(){
	return outgoingBuffer;
}

Gut::CryptoContext& Gut::Client::getCipher(){
	return cipher;
}

const Gut::ClientState Gut::Client::getState(){
	return state;
}

void Gut::Client::setState(ClientState state){
	this->state = state;
}

void Gut::Client::setClientEncrypted(SessionKey key){
	setState(ClientState::ENCRYPTED);
	//
}

//initializes client secure encryption stats
void Gut::Client::startTunnel(){
	cipher.recvNonce = 0;
	cipher.sendNonce = 0;
	state = ClientState::ENCRYPTED;
}

//sets a client as authenticated
void Gut::Client::setAuthenticated(String username, UsrID usrId){
	credentials.userId = usrId;
	credentials.username = username;
	state = ClientState::AUTHENTICATED;
}

const Gut::AuthContext& Gut::Client::getCredentials(){
	return credentials;
}
