#include "gutweb.hpp"


int main() {
    Gut::TcpSocket server;
    server.bind();
    server.listen();
    std::thread input(consoleThread);  // start console command listener
    std::cout << "Server started. Type 'quit' to stop.\n";
    while (true) {
        server.accept();
        Gut::SocketList* clients = server.getClients();
        for (SOCKET client : *clients) {
            // Handle client communication here
        }
    }
    return 0;
}

void consoleThread() {
    std::string cmd;
    while (running) {
        std::getline(std::cin, cmd);
        if (cmd == "quit" || cmd == "exit") {
            shutdownServer();
            exit(0);
        }
    }
}

void shutdownServer() {
    std::cout << "Shutting down server..." << std::endl;
    WSACleanup();
    running = false;
}

