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

	enum class TaskType : int
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
		AssignedClient assignedClient;

	public:
		Task(Client &client);
		virtual ~Task() = default;
		AssignedClient& getClient();
		//instructions for the task
		virtual Message&& execute() = 0;
	};
}

#endif