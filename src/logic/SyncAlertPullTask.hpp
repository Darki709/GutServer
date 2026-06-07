#pragma once
#include "task.hpp"
#include "../core/alert_state_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut
{
	// TaskType 18 (SYNC_ALERT_PULL): return all of the authenticated user's alert rows.
	// Response MsgType 20.
	class SyncAlertPullTask : public Task
	{
	public:
		SyncAlertPullTask(std::shared_ptr<Client> &client, uint32_t reqId);
		std::optional<Message> execute(ThreadResources &resources) override;
	};
}
