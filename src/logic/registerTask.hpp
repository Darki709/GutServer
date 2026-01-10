#pragma once

#include "../libraries.hpp"
#include "task.hpp"
#include "../core/user_db_helper.hpp"

namespace Gut{
	enum class RegisterFlag : uint8_t{
		SUCCESS,
		USERALREADYEXISTS,
		INSECUREPASSWORD
	};
	class RegisterTask : public Task{
		public:
			RegisterTask(std::shared_ptr<Client>& client, uint32_t reqId, String content);
			~RegisterTask() = default;
			std::optional<Message> execute();
		private:
			String username;
			String password;
	};
}