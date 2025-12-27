#ifndef TAKSK_HPP
#define TAKSK_HPP

#include "../libraries.hpp"
#include "../runtime/client.hpp"
#include "../core/message.hpp"

namespace Gut
{
	struct AssignedClient{
		SOCKET client;
		ClientState state;
		AuthContext credentials;
	};

	enum class ExecutionCode
	{
		SERVERERROR,
		FORBIDDEN,
	};

	enum class TaskType : uint8_t
	{
		HANDSHAKEHELLO,
		HANDSHAKEVERIFY,
		LOGIN,
		REGISTER,
		PRICE_REQUEST,
		PRICE_STREAM,
	};
	
	class Task
	{
	private:
		std::weak_ptr<Client> client;

	public:
		Task(Client& client);
		virtual ~Task() = default;
		std::shared_ptr<Gut::Client> getClient();
		//instructions for the task
		virtual std::optional<Message> execute() = 0;
	};
}

#endif