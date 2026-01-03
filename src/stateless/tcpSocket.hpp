#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "baseSocket.hpp"
#include "../libraries.hpp"


namespace Gut{
    class TcpSocket : public BaseSocket {
        public:
            TcpSocket();
            virtual ~TcpSocket();
            void listen();
            SOCKET accept();
            String receive(SOCKET client);
            int send(SOCKET client, const String& message);   
    };
};



#endif 
