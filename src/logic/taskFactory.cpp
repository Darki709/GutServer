#include "HandshakeHello.hpp"
#include "HandshakeVerify.hpp"
#include "taskFactory.hpp"
#include "RequestTickerData.hpp"
#include "CancelTickerStream.hpp"
#include "registerTask.hpp"
#include "../runtime/client.hpp"

std::unique_ptr<Gut::Task> Gut::TaskFactory::createTask(Message message, std::shared_ptr<Client> &client)
{
	String &content = message.getContent();

	// extract task type
	uint8_t taskType;
	memcpy(&taskType, content.data(), 1);
	content.erase(0, 1);

	// extract request id
	uint32_t reqId;
	memcpy(&reqId, content.data(), 4);
	reqId = ntohl(reqId);
	std::cout << "request id" << std::to_string(reqId) << std::endl;
	content.erase(0, 4);

	std::cout << static_cast<int>(taskType) << std::endl;
	try
	{
		// switch on task type, content sent to tasks is without task type and request id
		switch (static_cast<int>(taskType))
		{
		case static_cast<int>(TaskType::HANDSHAKEHELLO):
			return std::make_unique<HandShakeHello>(client, reqId, content);
		case static_cast<int>(TaskType::HANDSHAKEVERIFY):
			return std::make_unique<HandShakeVerify>(client, reqId, content);
		case static_cast<int>(TaskType::REQUESTTICKERDATA):
			return std::make_unique<RequestTickerData>(client, reqId, content);
		case static_cast<int>(TaskType::CANCELTICKERSTREAM):
			return std::make_unique<CancelTickerStream>(client, reqId, content);
		case static_cast<int>(TaskType::REGISTER):
			return std::make_unique<RegisterTask>(client, reqId, content);
		default:
			return nullptr;
		}
	}
	catch (Errors err)
	{
		// add specialized error handling later
		if (err == Errors::ILLEGALACCESS)
		{
			std::cout << "illegal access from " << std::to_string(client->getSocket()) << std::endl;
			return nullptr;
		}
		if (err == Errors::INVALIDREQUEST)
		{
			std::cout << "invalid message from " << std::to_string(client->getSocket()) << std::endl;
			return nullptr;
		}
	}
	return nullptr;
}
