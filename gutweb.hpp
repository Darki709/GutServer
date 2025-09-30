#ifndef gutweb_hpp
#define gutweb_hpp
//libraries included
#include "networksec/tcpSocket.hpp"
#include "networksec/baseSocket.hpp"
#include <thread>
#include <atomic>

std::atomic<bool> running(true);
void consoleThread();
void shutdownServer();
#endif