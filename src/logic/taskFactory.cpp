#include "HandshakeHello.hpp"
#include "taskFactory.hpp"
#include "../runtime/client.hpp"

std::unique_ptr<Gut::Task>  Gut::TaskFactory::createTask(Message message, Client& client)
{
	String content = message.getContent();

	uint8_t taskType;
	memcpy(&taskType, content.data(), 1);
	content.erase(0, 1);
	uint32_t reqId;
	memcpy(&reqId, content.data(), 4);
	content.erase(0, 4);
	return std::make_unique<HandShakeHello>(client, "dawdawd");
}
