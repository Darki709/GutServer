#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "baseSocket.hpp"
#include <string>
#include <unordered_set>


namespace Gut{
    typedef std::unordered_set<SOCKET> SocketList;
    typedef std::string String;

    class TcpSocket : public BaseSocket {
        private:
            SocketList clients;
        public:
            TcpSocket();
            ~TcpSocket();
            void listen();
            void accept();
            SocketList* getClients();
            String receive(SOCKET client);
            int send(SOCKET client, String& message);            
    };
};



#endif 
