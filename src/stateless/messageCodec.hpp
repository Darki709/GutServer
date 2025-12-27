#pragma once

#include "../libraries.hpp"
#include "../core/message.hpp"

namespace Gut
{
	class Client;

	class MessageCodec
	{
	public:
		// encode/decodes the message internal contents 
		static void encode(Message& msg, Client& client);

		static void decode(Message& msg, Client& client);
	};

	// constant flags that are used to define the message type
	enum class Flags : uint8_t
	{
		PLAIN_TEXT = 0,
		ENCRYPTED = 1
	};

}
