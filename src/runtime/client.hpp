#ifndef HANDLERSEC_CLIENT_H
#define HANDLERSEC_CLIENT_H

#include "../libraries.hpp"
#include "../core/message.hpp"
#include "../stateless/crypto.hpp"


namespace Gut
{
	enum class ClientState : int{
		CONNECTED = 0, //socket accepted
		HANDSHAKE_VERIFY = 1, //verify sent
		ENCRYPTED = 2, //secure tunnel achieved
		AUTHENTICATED = 3 //user logged in
	};

	struct AuthContext {
    	UsrID userId;
    	std::string username;
	};

	struct CryptoContext {
    	AESGCM   cipher;
    	uint64_t sendNonce;
    	uint64_t recvNonce;
	};

	class Client : public std::enable_shared_from_this<Client>
	{
	private:

		SOCKET clientSocket;

		//client identifiers
		ClientState state; //state of clients connection cycle
		std::optional<AuthContext> credentials; // user id and username


		//encryption
		CryptoContext cipher;

		//message buffers
		String incomingBuffer;
		String outgoingBuffer;


	public:
		~Client();
		explicit Client(SOCKET socket);
		SOCKET getSocket();
		//add characters to the message buffers
		void pushInBuffer(const String& content);
		void pushOutBuffer(const String& content);
		//pop len characters from the start buffer
		void popInBuffer(int len);
		void popOutBuffer(int len);

		const String& getInBuffer();
		const String& getOutBuffer();

		const ClientState getState();
		void setState(ClientState state);

		CryptoContext& getCipher();
		
		void setClientEncrypted(SessionKey key); //initializes crypto context after client succefully finished te private key exchange 
		
		void startTunnel();
	};

}


#endif
