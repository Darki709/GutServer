#include "ModifyWatchlistItemsTask.hpp"

namespace Gut
{
	ModifyWatchlistItemsTask::ModifyWatchlistItemsTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content)
		: Task(client, reqId)
	{
		action = content[0];
		uint8_t listLen = content[1];
		listName = content.substr(2, listLen);
		uint8_t symLen = content[2 + listLen];
		symbol = content.substr(3 + listLen, symLen);
	}

	std::optional<Message> ModifyWatchlistItemsTask::execute(ThreadResources &resources)
	{
		auto client = getClient();
		SOCKET sock = client->getSocket();

		String payload;
		uint8_t msgType = 16;
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

		int dbRes = (action == 0)
						? listDb.insert_new_ticekr(symbol, listName)
						: listDb.delete_ticker_from_list(client->getCredentials().userId, symbol, listName);

		uint8_t status = static_cast<uint8_t>(dbRes);
		payload.append(reinterpret_cast<char *>(&status), 1);
		return Message(payload, sock);
	}
}