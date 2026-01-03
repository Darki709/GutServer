#ifndef BASESOCKET_H
#define BASESOCKET_H
#include "../libraries.hpp"

#define LOCAL_HOST "127.0.0.1"
#define DEFAULT_PORT 6767

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
}




#endif // BASESOCKET_H
