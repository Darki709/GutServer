#include "socket.hpp"


SocketGut::BaseSocket::BaseSocket(u_int port, u_int ip_address, int service){
    //define adress structure
    adress.sin_family = AF_INET;
    adress.sin_port = htons(port);
    adress.sin_addr.s_addr = htonl(ip_address);

    //establish socket
    sock = socket(AF_INET, service, 0);
    test_connection(sock);
    //establish connection
    connection = connect_to_client(sock, adress);
    test_connection(connection);
}   

void SocketGut::BaseSocket::test_connection(SOCKET test){
    if(test < 0){
        perror("Socket error");
        exit(EXIT_FAILURE);
    }
}

struct sockaddr_in SocketGut::BaseSocket::get_adress() const {
    return adress;
}

SOCKET SocketGut::BaseSocket::get_sock() const {
    return sock;
}

SOCKET SocketGut::BaseSocket::get_connection() const {
    return connection;
}
    
    
