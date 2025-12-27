#pragma once
#include "../core/message.hpp"
#include "task.hpp"

namespace Gut{
	class TaskFactory{
		public:
			static std::unique_ptr<Task> createTask(Message message, Client& client);
	};
}