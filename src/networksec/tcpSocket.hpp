#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "baseSocket.hpp"
#include "../libraries.hpp"
#include "../handlersec/client.hpp"
#include "../handlersec/message.hpp"


namespace Gut{

    class TcpSocket : public BaseSocket {
        private:
            ClientSet ClientsSet;
			std::queue<Message> outgoingMessages;

        public:
            TcpSocket();
            virtual ~TcpSocket();
            void listen();
            int accept();
            ClientSet* getClients();
			std::queue<Message>& getMessages();
            String* receive(SOCKET client);
            int send(SOCKET client, const String& message);  
			void acceptClients();          
    };
};



#endif 
