#pragma once
#include "task.hpp"
#include "../core/chart_state_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut
{
	// TaskType 16 (SYNC_CHART_PULL): return all of the authenticated user's chart-state
	// rows (drawings + indicators + presets). Response MsgType 18.
	class SyncChartPullTask : public Task
	{
	public:
		SyncChartPullTask(std::shared_ptr<Client> &client, uint32_t reqId);
		std::optional<Message> execute(ThreadResources &resources) override;
	};
}
