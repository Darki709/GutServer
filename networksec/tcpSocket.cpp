#include "tcpSocket.hpp"


Gut::TcpSocket::TcpSocket() : BaseSocket(SOCK_STREAM) {
    std::cout << "TCP Socket created" << std::endl;
}

Gut::TcpSocket::~TcpSocket() {
    for (SOCKET client : clients) {
        closesocket(client);
    }
    closesocket(getSocket());
    std::cout << "TCP Socket destroyed" << std::endl;
}

void Gut::TcpSocket::listen() {
    if(::listen(getSocket(), SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "listen() failed." << std::endl;
        closesocket(getSocket());
        WSACleanup();
        exit(1);
    }
    else {
        std::cout << "listen() is OK, waiting for connections..." << std::endl;
    }
}

void Gut::TcpSocket::accept() {
    SOCKET clientSocket;
    sockaddr_in clientInfo;
    int clientInfoSize = sizeof(clientInfo);

    clientSocket = ::accept(getSocket(), (SOCKADDR*)&clientInfo, &clientInfoSize);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "accept failed: " << WSAGetLastError() << std::endl;
    } else {
        clients.insert(clientSocket);
        std::cout << "Client connected." << std::endl;
    }
}

Gut::SocketList* Gut::TcpSocket::getClients() {
    return &clients;
}
        
    
// Your code here
