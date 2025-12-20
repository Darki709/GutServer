#pragma once

// Centralized common headers for the project
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <memory>
#include <stdio.h>
#include <queue>
#include <mutex>
#include <condition_variable>


namespace Gut {
	typedef std::string String;
	typedef unsigned long UsrID;
	typedef std::byte EncryptKey;
	typedef std::unordered_map<SOCKET, Client> ClientSet;
}