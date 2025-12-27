#pragma once

#include "../libraries.hpp"
#include "task.hpp"

namespace Gut{
	// verifies that the client got the key 
	class HandShakeVerify : public Task{
		private:
			String encryptedMessage;
		public:
			HandShakeVerify(Client& client, String encryptedMessage);
			std::optional<Message> execute() override;
	};
}