#include "baseSocket.hpp"



Gut::BaseSocket::BaseSocket(int type) {
    // Initialize WSA variables
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);

    // Check for initialization success
    if (wsaerr != 0) {
        std::cout << "The Winsock dll not found!" << std::endl;
        exit(1);
    } else {
        std::cout << "The Winsock dll found" << std::endl;
        std::cout << "The status: " << wsaData.szSystemStatus << std::endl;
    }

    int protocol = type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
    sock = socket(AF_INET, type, protocol);
    if (sock == INVALID_SOCKET) {
        std::cout << "Error at socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(1);
    } else {
        std::cout << "socket initialized" << std::endl;
    }
}

void Gut::BaseSocket::bind(){
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr(LOCAL_HOST);  // Replace with your desired IP address
    service.sin_port = htons(DEFAULT_PORT);
    if(::bind(sock, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cout << "bind() failed." << std::endl;
        closesocket(sock);
        WSACleanup();
        exit(1);
    }
    else {
        std::cout << "bind() is OK." << std::endl;
    }
}

SOCKET Gut::BaseSocket::getSocket(){
    return sock;
}