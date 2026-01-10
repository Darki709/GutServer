#pragma once 

#include "../libraries.hpp"
#include "task.hpp"
#include "../core/user_db_helper.hpp"

namespace Gut{
	enum class LoginFlags : uint8_t{
		SUCCESS,
		WRONGPASSWORD,
		USERDOESNTEXIST
	};

	class LoginTask : public Task{
		public:
			LoginTask(std::shared_ptr<Client>& client, uint32_t reqId, String content);
			~LoginTask() = default;
			std::optional<Message> execute();
		private:
			String username;
			String password;
	};
}