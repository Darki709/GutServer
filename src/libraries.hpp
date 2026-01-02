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
#include <iomanip>

namespace Gut
{

	typedef std::string String;
	typedef uint32_t UsrID;
	typedef std::byte EncryptKey;
	typedef std::array<uint8_t, 32> SessionKey; // AES-256

	enum Errors
	{
		CLIENTNOTFOUND,
		HANDSHAKEFAILURE,
		ILLEGALACCESS,
		INVALIDREQUEST,
	};

	template <typename T>
	inline static void append_bytes(std::string &out, const T &value)
	{
		std::cout << "appending " << sizeof(T) << std::endl;
		out.append(reinterpret_cast<const char *>(&value), sizeof(T));
	}

	inline void append_double(String &s, double v)
	{
		uint64_t tmp;
		static_assert(sizeof(double) == 8);
		memcpy(&tmp, &v, 8);
		tmp = htonll(tmp);
		append_bytes(s, tmp);
	}
}

#endif