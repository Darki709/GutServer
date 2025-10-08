#ifndef BASESOCKET_H
#define BASESOCKET_H
//libraries
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream> 
#include <stdio.h>

#define LOCAL_HOST "127.0.0.1"
#define DEFAULT_PORT 6769

// Your code here
namespace Gut{
class BaseSocket {
    private:
    SOCKET sock;

    public:
        BaseSocket(int type);
        void bind();
        SOCKET getSocket();
};
};




#endif // BASESOCKET_H
