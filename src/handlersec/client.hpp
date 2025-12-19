#ifndef HANDLERSEC_CLIENT_H
#define HANDLERSEC_CLIENT_H

#include "../libraries.hpp"



namespace Gut
{
	typedef int User;
class Client
{
	private:
		SOCKET clientSocket;
		String incomingBuffer;
		String outgoingBuffer;
	public:
		Client(SOCKET socket);
		~Client();
		virtual bool isAuthed(); //false fro regular clients
		SOCKET getSocket();
		void remove(); //remove out a client
		//add characters to the message buffers
		void pushInBuffer(const String& content);
		void pushOutBuffer(const String& content);
		//pop len characters from the start buffer
		void popInBuffer(int len);
		void popOutBuffer(int len);

		String& getInBuffer();
		String& getOutBuffer();
};
	
} 


#endif
