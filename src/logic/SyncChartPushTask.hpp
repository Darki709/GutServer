#pragma once
#include "task.hpp"
#include "../core/chart_state_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut
{
	// TaskType 17 (SYNC_CHART_PUSH): apply a batch of the user's locally-changed rows
	// (last-write-wins). Response MsgType 19 with a single status byte.
	class SyncChartPushTask : public Task
	{
	private:
		String content; // raw payload: [2B count][rows...]
	public:
		SyncChartPushTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content);
		std::optional<Message> execute(ThreadResources &resources) override;
	};
}
