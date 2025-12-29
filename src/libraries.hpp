#ifndef LIBRARIES_HPP
#define LIBRARIES_HPP

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
#include <optional>
#include <array>
#include <cassert>


namespace Gut {

	typedef std::string String;
	typedef uint32_t UsrID;
	typedef std::byte EncryptKey;
	typedef std::array<uint8_t, 32> SessionKey; // AES-256

	enum Errors{
		CLIENTNOTFOUND,
		HANDSHAKEFAILURE,
	};
}

#endif