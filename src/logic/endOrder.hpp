#pragma once

#include "../core/user_orders_table_helper.hpp"
#include "../core/user_db_helper.hpp"
#include "../libraries.hpp"
#include "../stateless/stock_db_helper.hpp"
#include "task.hpp"

namespace Gut
{
	enum class Status : uint8_t{
		ORDERNOTEXIST,
		EXPECTEDPRICEOUTOFRANGE
	};

	class EndOrder : public Task
	{
	private:
		ID orderId; //id of the order you want to cancel
		double expected_price; //price the client expects the ticker to have when he exits (inportatnt for profit and stop loss, will be used to calculate slips to prevent unwanted behaviour)
		String password; //Buy/Sell actions require authentication
				
	public:
		EndOrder(std::shared_ptr<Client>& client, uint32_t reqId, String content);
		std::optional<Message> execute(ThreadResources& resources) override;
	};
}
