#ifndef gutweb_hpp
#define gutweb_hpp


#include "handlersec/client.hpp"

#include "server.hpp"

#include "libraries.hpp"

namespace Gut {

	std::atomic<bool> running(true);
	void consoleThread();
	void Shutdown();
	int main();
}
#endif