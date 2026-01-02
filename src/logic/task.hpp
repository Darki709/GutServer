#ifndef TAKSK_HPP
#define TAKSK_HPP

#include "../libraries.hpp"
#include "../runtime/client.hpp"
#include "../core/message.hpp"

namespace Gut
{
	class Server;

	struct AssignedClient{
		SOCKET client;
		ClientState state;
		AuthContext credentials;
	};

	enum class TaskType : int
	{
		HANDSHAKEHELLO,
		HANDSHAKEVERIFY,
		LOGIN,
		REGISTER,
		REQUESTTICKERDATA,
		CANCELTICKERSTREAM,
	};
	
	class Task
	{
	private:
		std::weak_ptr<Client> client;
		uint32_t reqId;
		Server* server;

	public:
		Task(std::shared_ptr<Client>& client, uint64_t reqId);
		virtual ~Task() = default;
		std::shared_ptr<Gut::Client> getClient();
		uint32_t getReqId();
		Server* getServer();
		void setServer(Server* server);
		//instructions for the task
		virtual std::optional<Message> execute() = 0;
	};
}

#endif