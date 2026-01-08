#include "CancelTickerStream.hpp"

Gut::CancelTickerStream::CancelTickerStream(std::shared_ptr<Client> &client, uint32_t reqId, String content) : Task(client, reqId)
{
	// check client privileges
	if (static_cast<int>(client->getState()) < 3) // client shoul already be logged in
		throw Errors::ILLEGALACCESS;

	// get the og reqId
	memcpy(&ogReqId, content.data(), 4);
	content.erase(0, 4);
	ogReqId = ntohl(ogReqId); // return to host byte order

	// get symbol
	uint8_t symbolLen;
	memcpy(&symbolLen, content.data(), 1);
	content.erase(0, 1);
	if (content.length() < symbolLen)
        throw std::runtime_error("Symbol length mismatch in payload");

	symbol.assign(content.data(), symbolLen);

	std::cout << "started cancel ticker stream for reqId " << std::to_string(ogReqId) << " on symbol " << symbol << std::endl;
}

std::optional<Gut::Message> Gut::CancelTickerStream::execute()
{
	std::shared_ptr<Client> client;
	if ((client = Task::getClient()) != nullptr)
	{
		Streamer::getInstance().cancelRequest(symbol, client->getSocket(), ogReqId);
		return std::nullopt;
	}
	throw Errors::CLIENTNOTFOUND;
}