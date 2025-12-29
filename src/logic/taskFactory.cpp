#include "HandshakeHello.hpp"
#include "HandshakeVerify.hpp"
#include "taskFactory.hpp"
#include "../runtime/client.hpp"

std::unique_ptr<Gut::Task> Gut::TaskFactory::createTask(Message message, std::shared_ptr<Client>& client)
{
	String& content = message.getContent();

	//extract task type
	uint8_t taskType;
	memcpy(&taskType, content.data(), 1);
	content.erase(0, 1);

	//extract request id
	uint32_t reqId;
	memcpy(&reqId, content.data(), 4);
	content.erase(0, 4);

	std::cout << static_cast<int>(taskType) << std::endl;

	//switch on task type
	switch (static_cast<int>(taskType))
	{
	case static_cast<int>(TaskType::HANDSHAKEHELLO):
		return std::make_unique<HandShakeHello>(client, content);
	case static_cast<int>(TaskType::HANDSHAKEVERIFY):
		return std::make_unique<HandShakeVerify>(client, content);
	default:
		return nullptr;
	}
}
