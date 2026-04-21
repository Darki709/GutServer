#pragma once

#include "../libraries.hpp"
#include "task.hpp"
#include "../core/user_orders_table_helper.hpp"

namespace Gut
{
	class FetchOrdersTask : public Task
	{
	public:		
		FetchOrdersTask(std::shared_ptr<Gut::Client> &client, uint32_t reqId, String content);	
		std::optional<Message> execute() override;

	private:
		OrderFilters m_filters;
	};
}