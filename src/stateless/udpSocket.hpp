#ifndef UDPSOCKET_HPP
#define UDPSOCKET_HPP
#include "baseSocket.hpp"
#include "../libraries.hpp"


namespace Gut {

    class udpSocket : public BaseSocket {
    private:

    public:
        udpSocket();
		
        String* receive();
        int send(String& message, const sockaddr_in& clientAddr);
    };
}

#endif // UDPSOCKET_HPP
