#ifndef MMESSAGE_HPP
#define MMESSAGE_HPP

#include "../libraries.hpp"

namespace Gut
{
	class Message
	{
	private:
		String content;
		SOCKET recipient;


	public:
		// crates a message
		Message(const String& content, SOCKET recipient);
		String &getContent();
		SOCKET getRecipient();
	};
}

#endif

// message layout
/*

|header|
[4 bytes]  total_length of the payload + 1 for flags   (uint32, network order)
[1 byte ]  flags (encrypted\plaintext)

|payload| (encrypted or plaintext)
[1 byte ]  message_type
[4 bytes]  request_id
[...    ]  message_payload
*/
