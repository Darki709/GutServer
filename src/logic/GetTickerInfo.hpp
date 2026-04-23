#pragma once

#include "task.hpp"
#include "../libraries.hpp"
#include "../core/ticker_list_db_helper.hpp"

namespace Gut{
	class GetTickerInfo : public Task{
		private:
			String symbol;
		public:
			GetTickerInfo(std::shared_ptr<Client>& client, uint32_t reqId, String symbol);
			~GetTickerInfo() = default;
			std::optional<Message> execute(ThreadResources& resources);
	};
}