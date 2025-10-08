#include "udpSocket.hpp"

Gut::udpSocket::udpSocket() : Gut::BaseSocket(SOCK_DGRAM) {
    std::cout << "UDP Socket created" << std::endl;
}

Gut::String* Gut::udpSocket::receive() {
    char buffer[1024];
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    int bytesReceived = recvfrom(getSocket(), buffer, sizeof(buffer) - 1, 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
    if (bytesReceived == SOCKET_ERROR) {
        std::cout << "recvfrom failed: " << WSAGetLastError() << std::endl;
        return nullptr;
    }
    buffer[bytesReceived] = '\0'; // Null-terminate the received data
    return new String(buffer);
}

int Gut::udpSocket::send(String& message, const sockaddr_in& clientAddr) {
    int sendResult = sendto(getSocket(), message.c_str(), static_cast<int>(message.size()), 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
    if (sendResult == SOCKET_ERROR) {
        std::cout << "sendto failed: " << WSAGetLastError() << std::endl;
    }
    return sendResult;
}
