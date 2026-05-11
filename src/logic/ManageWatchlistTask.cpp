#include "ManageWatchlistTask.hpp"

namespace Gut
{
	ManageWatchlistTask::ManageWatchlistTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content)
		: Task(client, reqId)
	{
		action = content[0];
		uint8_t nameLen = content[1];
		name = content.substr(2, nameLen);

		if (action == 1)
		{ // Rename logic
			uint8_t newNameLen = content[2 + nameLen];
			newName = content.substr(3 + nameLen, newNameLen);
		}
	}

	std::optional<Message> ManageWatchlistTask::execute(ThreadResources &resources)
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

		uint32_t uid = client->getCredentials().userId;
		int dbRes = -1;
		List_DB listDb;
		if (action == 0)
			dbRes = listDb.create_new_list(uid, name);
		else if (action == 1)
			dbRes = listDb.rename_list(uid, name, newName);
		else if (action == 2)
			dbRes = listDb.delete_list(uid, name);

		uint8_t status = 0;
		if (dbRes == 0)
			status = 0;
		else if (dbRes == 1)
			status = (action == 0) ? 2 : 1;
		else
			status = 4;

		payload.append(reinterpret_cast<char *>(&status), 1);
		return Message(payload, sock);
	}
}