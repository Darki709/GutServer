#include "SyncAlertPushTask.hpp"
#include <cstring>

namespace Gut
{
	SyncAlertPushTask::SyncAlertPushTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content)
		: Task(client, reqId), content(content) {}

	std::optional<Message> SyncAlertPushTask::execute(ThreadResources &resources)
	{
		auto client = getClient();
		SOCKET sock = client->getSocket();

		String payload;
		uint8_t msgType = 21; // ALERT_SYNC_PUSH_RESULT
		uint32_t rId = htonl(getReqId());
		payload.append(reinterpret_cast<char *>(&msgType), 1);
		payload.append(reinterpret_cast<char *>(&rId), 4);

		auto respond = [&](uint8_t status) -> Message {
			payload.append(reinterpret_cast<char *>(&status), 1);
			return Message(payload, sock);
		};

		if (client->getState() != ClientState::AUTHENTICATED)
			return respond(5); // UNAUTHORIZED

		const uint8_t *data = reinterpret_cast<const uint8_t *>(content.data());
		size_t size = content.size();
		size_t pos = 0;

		if (size < 2)
			return respond(4); // malformed
		uint16_t countN;
		std::memcpy(&countN, data + pos, 2);
		pos += 2;
		uint16_t count = ntohs(countN);

		uint32_t userId = client->getCredentials().userId;
		Alert_State_DB db;
		bool ok = true;

		// Defensive parse: every field is bounds-checked so a malformed payload can
		// never read past the buffer or crash the worker.
		for (uint16_t i = 0; i < count; ++i)
		{
			if (pos + 2 > size) { ok = false; break; } // uuidLen(2)
			uint16_t uuidLenN;
			std::memcpy(&uuidLenN, data + pos, 2);
			pos += 2;
			uint16_t uuidLen = ntohs(uuidLenN);

			if (pos + uuidLen + 13 > size) { ok = false; break; } // uuid + deleted(1) + ts(8) + payLen(4)
			String uuid(reinterpret_cast<const char *>(data + pos), uuidLen);
			pos += uuidLen;
			uint8_t deleted = data[pos];
			pos += 1;
			uint64_t updN;
			std::memcpy(&updN, data + pos, 8);
			pos += 8;
			uint64_t updated = ntohll(updN);
			uint32_t payLenN;
			std::memcpy(&payLenN, data + pos, 4);
			pos += 4;
			uint32_t payLen = ntohl(payLenN);

			if (pos + payLen > size) { ok = false; break; }
			String pl(reinterpret_cast<const char *>(data + pos), payLen);
			pos += payLen;

			if (db.upsert_lww(userId, uuid, pl, updated, deleted != 0) != 0)
				ok = false;
		}

		return respond(ok ? 0 : 4); // SUCCESS / DB_ERROR
	}
}
