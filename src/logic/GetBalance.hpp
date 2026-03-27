#pragma once 
#include "task.hpp"
#include "../core/user_db_helper.hpp"

namespace Gut{
	class GetBalance : public Task{
		public:
		GetBalance(std::shared_ptr<Client>& client, uint32_t reqId);
		std::optional<Message> execute();
	};
}