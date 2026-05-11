#include "GetWatchlistContentTask.hpp"

namespace Gut
{
	GetWatchlistContentTask::GetWatchlistContentTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content)
		: Task(client, reqId)
	{
		uint8_t len = content[0];
		listName = content.substr(1, len);
	}

	std::optional<Message> GetWatchlistContentTask::execute(ThreadResources &resources)
	{
		auto client = getClient();
		SOCKET sock = client->getSocket();

		String payload;
		uint8_t msgType = 17;
		uint32_t rId = htonl(getReqId());
		payload.append(reinterpret_cast<char *>(&msgType), 1);
		payload.append(reinterpret_cast<char *>(&rId), 4);

		if (client->getState() != ClientState::AUTHENTICATED)
		{
			uint8_t status = 5;
			payload.append(reinterpret_cast<char *>(&status), 1);
			return Message(payload, sock);
		}

		List_DB listDb;

		std::vector<String> symbols;
		int result = listDb.get_tickers_in_list(client->getCredentials().userId, listName, symbols);

		uint8_t status = (result >= 0) ? 0 : 1;
		uint8_t count = (result >= 0) ? static_cast<uint8_t>(symbols.size()) : 0;

		payload.append(reinterpret_cast<char *>(&status), 1);
		payload.append(reinterpret_cast<char *>(&count), 1);

		if (status == 0)
		{
			for (const auto &sym : symbols)
			{
				uint8_t symLen = static_cast<uint8_t>(sym.length());
				payload.append(reinterpret_cast<char *>(&symLen), 1);
				payload.append(sym);
			}
		}

		return Message(payload, sock);
	}
}