#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "baseSocket.hpp"
#include <vector>
#include <string>


namespace Gut{
    typedef std::vector<SOCKET> SocketList;
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
    };
};



#endif 
