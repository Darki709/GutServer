#include "FetchWatchlistsTask.hpp"

namespace Gut
{
	FetchWatchlistsTask::FetchWatchlistsTask(std::shared_ptr<Client> &client, uint32_t reqId)
		: Task(client, reqId) {}

	std::optional<Message> FetchWatchlistsTask::execute(ThreadResources &resources)
	{
		auto client = getClient();
		SOCKET sock = client->getSocket();

		String payload;
		uint8_t msgType = 15;
		uint32_t rId = htonl(getReqId());
		payload.append(reinterpret_cast<char *>(&msgType), 1);
		payload.append(reinterpret_cast<char *>(&rId), 4);

		if (client->getState() != ClientState::AUTHENTICATED)
		{
			uint8_t status = 5;
			payload.append(reinterpret_cast<char *>(&status), 1);
			return Message(payload, sock);
		}

		std::vector<ListInfo> lists;
		List_DB listDb;
		int result = listDb.get_all_lists(client->getCredentials().userId, lists);

		uint8_t status = (result >= 0) ? 0 : 4;
		uint8_t count = (result >= 0) ? static_cast<uint8_t>(lists.size()) : 0;

		payload.append(reinterpret_cast<char *>(&status), 1);
		payload.append(reinterpret_cast<char *>(&count), 1);

		if (status == 0)
		{
			for (const auto &list : lists)
			{
				uint32_t listId = htonl(list.id);
				uint8_t nameLen = static_cast<uint8_t>(list.name.length());
				payload.append(reinterpret_cast<char *>(&listId), 4);
				payload.append(reinterpret_cast<char *>(&nameLen), 1);
				payload.append(list.name);
			}
		}

		return Message(payload, sock);
	}
}