#pragma once 

#include "../libraries.hpp"
#include "../stateless/streamer.hpp"
#include "task.hpp"

namespace Gut{
	class CancelTickerStream : public Task{
		public:
			CancelTickerStream(std::shared_ptr<Client>& client, uint32_t reqId, String content);
			~CancelTickerStream() = default;
			std::optional<Message> execute();

		private:
			String symbol;
			uint32_t ogReqId;
	};
}