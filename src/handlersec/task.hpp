#ifndef TAKSK_HPP
#define TAKSK_HPP

#include "../libraries.hpp"
#include "client.hpp"


namespace Gut {
	class Task {
	private:
		SOCKET assignedClient;
	public:
		Task(Client &client);
		virtual ~Task() = default;
		static Task& createTask(SOCKET client, const String& data);
		virtual String* getHeaders() = 0;
		virtual String* getData() = 0;
		int getClientId();
		Client* getClient();
		virtual Message* xecute() = 0;
	};
}

#endif 