#pragma once 

#include "../libraries.hpp"
#include "task.hpp"
#include "../stateless/stock_db_helper.hpp"
#include "../core/user_orders_table_helper.hpp"
#include "../core/user_db_helper.hpp"

namespace Gut{

	enum class OrderType : uint8_t{
		LONG,
		SHORT
	};

	enum class OrderStatus : uint8_t{
		INVALIDBALANCE,
		INVALIDSYMBOL,
	};

	class SendOrder : public Task{
		private:
			OrderType type;
			String symbol;
			int quantity;
			double asking_price; //the price the client expects to pay ,server prevents big slippage
		public:
			SendOrder(std::shared_ptr<Client> &client, uint32_t reqId, String content);
			std::optional<Message> execute();
	};
}