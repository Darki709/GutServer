#include "gutweb.hpp"

// file that holds the main function

int main()
{
	Gut::Server &serverInstance = Gut::Server::getInstance();
	serverInstance.serverStart();
	// start console command listener
	std::thread input(Gut::consoleThread);
	serverInstance.serverRun();
	return 0;
}

// Console command listener thread
void Gut::consoleThread() {
    std::string cmd;
    while (Gut::running) {
        std::getline(std::cin, cmd);
        if (cmd == "quit" || cmd == "exit") {
            Shutdown();
        }
    }
}

void Gut::Shutdown() {
    std::cout << "Shutting down server..." << std::endl;
    WSACleanup();
    Gut::running = false;
	exit(0);
}
