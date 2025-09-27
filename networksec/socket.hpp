#ifndef socket_hpp
#define socket_hpp
//libraries included
#include <iostream>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>


namespace SocketGut{

     class BaseSocket{
        private:
            struct sockaddr_in adress;
            SOCKET sock;
            SOCKET connection;
        public:
            BaseSocket(u_int port, u_int ip_address, int service);
            ~BaseSocket();
            virtual SOCKET connect_to_client(SOCKET sock, struct sockaddr_in adress) = 0;
            void test_connection(SOCKET test);

            // Getter methods
            struct sockaddr_in get_adress() const;
            SOCKET get_sock() const;
            SOCKET get_connection() const;
    };
}

#endif