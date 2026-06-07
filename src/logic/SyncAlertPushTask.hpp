#pragma once
#include "task.hpp"
#include "../core/alert_state_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut
{
	// TaskType 19 (SYNC_ALERT_PUSH): apply a batch of the user's locally-changed alerts
	// (last-write-wins). Response MsgType 21 with a single status byte.
	class SyncAlertPushTask : public Task
	{
	private:
		String content; // raw payload: [2B count][rows...]
	public:
		SyncAlertPushTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content);
		std::optional<Message> execute(ThreadResources &resources) override;
	};
}
