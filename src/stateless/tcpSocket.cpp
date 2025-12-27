#include "tcpSocket.hpp"


Gut::TcpSocket::TcpSocket() : BaseSocket(SOCK_STREAM) {
    SOCKET socket = BaseSocket::getSocket();

	//configure nonblocking behaviour
	u_long mode = 1;
	if(ioctlsocket(socket, FIONBIO, &mode) != 0) {
		throw std::runtime_error("nonblocking mode failed");
	}
    std::cout << "TCP Socket created" << std::endl;    
}

Gut::TcpSocket::~TcpSocket() {
    closesocket(getSocket());
    std::cout << "TCP Socket destroyed" << std::endl;
}

void Gut::TcpSocket::listen() {
    if(::listen(getSocket(), SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "listen() failed." << std::endl;
        closesocket(getSocket());
        WSACleanup();
        throw std::runtime_error("listen failed");
    }
    else {
        std::cout << "listen() is OK, waiting for connections..." << std::endl;
    }
}

//returns theclient socket on success
SOCKET Gut::TcpSocket::accept() {
	char ip_str[INET_ADDRSTRLEN];
    SOCKET clientSocket;
    sockaddr_in clientInfo;
    int clientInfoSize = sizeof(clientInfo);

    clientSocket = ::accept(getSocket(), (SOCKADDR*)&clientInfo, &clientInfoSize);
    if (clientSocket != INVALID_SOCKET) {
		//get client credentials
		inet_ntop(
    	AF_INET,
    	&clientInfo.sin_addr,
    	ip_str,
    	sizeof(ip_str)
		);
		int port = ntohs(clientInfo.sin_port);

		//configure nonblocking behaviour
		u_long mode = 1;
		if(ioctlsocket(clientSocket, FIONBIO, &mode) != 0) {
			throw std::runtime_error("nonblocking mode failed");
		}

        std::cout << "Client connected and pending authentication "<< ip_str << ":" << port << std::endl;
		//returns the client socket to add to the client set in the server
    }
	return clientSocket; //returns the socket for error handling or for adding client to client set
}

int Gut::TcpSocket::send(SOCKET client, const String& message) {
    int sendResult = ::send(client, message.c_str(), static_cast<int>(message.size()), 0);
    if (sendResult == SOCKET_ERROR) {
        std::cout << "send failed: " << WSAGetLastError() << std::endl;
    }
    return sendResult;
}


std::string Gut::TcpSocket::receive(SOCKET client) {
	char buffer[1024];
	int bytesReceived = ::recv(client, buffer, sizeof(buffer) - 1, 0);
	if (bytesReceived == SOCKET_ERROR) {
		int err = WSAGetLastError();
		std::cout << "recv failed: " << err << std::endl;
		throw err;
	}
	buffer[bytesReceived] = '\0'; // Null-terminate the received data
	return String(buffer);
}

