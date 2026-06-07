#include "SyncAlertPullTask.hpp"

namespace Gut
{
	SyncAlertPullTask::SyncAlertPullTask(std::shared_ptr<Client> &client, uint32_t reqId)
		: Task(client, reqId) {}

	std::optional<Message> SyncAlertPullTask::execute(ThreadResources &resources)
	{
		auto client = getClient();
		SOCKET sock = client->getSocket();

		String payload;
		uint8_t msgType = 20; // ALERT_SYNC_PULL_RESULT
		uint32_t rId = htonl(getReqId());
		payload.append(reinterpret_cast<char *>(&msgType), 1);
		payload.append(reinterpret_cast<char *>(&rId), 4);

		if (client->getState() != ClientState::AUTHENTICATED)
		{
			uint8_t status = 5; // UNAUTHORIZED
			payload.append(reinterpret_cast<char *>(&status), 1);
			return Message(payload, sock);
		}

		std::vector<AlertStateRow> rows;
		Alert_State_DB db;
		int result = db.get_all(client->getCredentials().userId, rows);

		uint8_t status = (result >= 0) ? 0 : 4; // SUCCESS / DB_ERROR
		payload.append(reinterpret_cast<char *>(&status), 1);
		if (status != 0)
			return Message(payload, sock);

		uint16_t count = htons(static_cast<uint16_t>(rows.size()));
		payload.append(reinterpret_cast<char *>(&count), 2);

		for (const auto &r : rows)
		{
			uint16_t uuidLen = htons(static_cast<uint16_t>(r.uuid.size()));
			uint8_t  deleted = r.deleted ? 1 : 0;
			uint64_t updated = htonll(r.updated_at);
			uint32_t payLen  = htonl(static_cast<uint32_t>(r.payload.size()));

			payload.append(reinterpret_cast<char *>(&uuidLen), 2);
			payload.append(r.uuid);
			payload.append(reinterpret_cast<char *>(&deleted), 1);
			payload.append(reinterpret_cast<char *>(&updated), 8);
			payload.append(reinterpret_cast<char *>(&payLen), 4);
			payload.append(r.payload);
		}

		return Message(payload, sock);
	}
}
