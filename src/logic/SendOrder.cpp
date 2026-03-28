#include "SendOrder.hpp"

namespace Gut
{
	SendOrder::SendOrder(std::shared_ptr<Client> &client, uint32_t reqId, String content)
		: Task(client, reqId) // Assuming Task takes these
	{
		size_t offset = 0;

		// 1. 1 byte order type
		if (offset + 1 > content.size())
			throw std::runtime_error("Invalid SendOrder: type missing");
		this->type = static_cast<OrderType>(content[offset++]);

		// 2. 1 byte symbol length + symbol
		if (offset + 1 > content.size())
			throw std::runtime_error("Invalid SendOrder: sym_len missing");
		uint8_t symLen = static_cast<uint8_t>(content[offset++]);

		if (offset + symLen > content.size())
			throw std::runtime_error("Invalid SendOrder: symbol truncated");
		this->symbol = content.substr(offset, symLen);
		offset += symLen;

		// 3. 4 bytes quantity (Network Byte Order)
		if (offset + 4 > content.size())
			throw std::runtime_error("Invalid SendOrder: quantity missing");
		uint32_t netQty;
		memcpy(&netQty, &content[offset], 4);
		this->quantity = ntohl(netQty);
		offset += 4;

		// 4. 8 bytes asking price as double
		if (offset + 8 > content.size())
			throw std::runtime_error("Invalid SendOrder: price missing");
		// Direct memory copy to preserve double precision
		memcpy(&this->asking_price, &content[offset], 8);
		offset += 8;

		// 5. 1 byte password length + password
		if (offset + 1 > content.size())
			throw std::runtime_error("Invalid SendOrder: pass_len missing");
		uint8_t passLen = static_cast<uint8_t>(content[offset++]);

		if (offset + passLen > content.size())
			throw std::runtime_error("Invalid SendOrder: password truncated");
		this->password = content.substr(offset, passLen);
		offset += passLen;
	}

	std::optional<Message> SendOrder::execute()
	{
		auto client = Task::getClient();
		if (!client)
			throw Errors::CLIENTNOTFOUND;

		// prepare the message header
		String content;
		char *n_reqId = reinterpret_cast<char *>(htonl(Task::getReqId()), 4);

		// first we check that the order has valid authentication
		User_table user_helper;
		if (user_helper.authenticateUser(client->getCredentials().username, this->password) < 0)
		{
			throw Errors::ILLEGALACCESS; // client will be automatically kicked for breaking the api rules
		}

		// client passed authentication, now we check that the symbol is valid
		{
			TickerListDBHelper tickerDB;
			try
			{
				tickerDB.getTickerInfo(this->symbol); // if this throw this means the ticker isn't in the database
			}
			catch (std::invalid_argument invalid_err)
			{
				content.push_back(static_cast<uint8_t>(MsgType::INVALIDORDER));
				content.append(n_reqId);
				content.push_back(static_cast<uint8_t>(OrderStatus::INVALIDSYMBOL));
				return Message{content, Task::getClient()->getSocket()};
			}
		}

		// check is the user can even pay for the order he sent, we do this before the costy api call to refresh prices
		double user_balance = Task::getClient()->getCredentials().balance;

		if ((this->asking_price * this->quantity) > user_balance)
		{
			content.push_back(static_cast<uint8_t>(MsgType::INVALIDORDER));
			content.append(n_reqId);
			content.push_back(static_cast<uint8_t>(OrderStatus::INVALIDBALANCE));
			return Message{content, Task::getClient()->getSocket()};
		}

		StockData latest_data;
		// the request is valid and now the task fetches the latest price
		{
			Stock_helper &stock_helper = Stock_helper::getInstance();
			if (stock_helper.fetchLiveData(this->symbol, static_cast<uint32_t>(Interval::MIN_1)) != 0)
			{
				content.push_back(static_cast<uint8_t>(MsgType::INVALIDORDER));
				content.append(n_reqId);
				content.push_back(static_cast<uint8_t>(OrderStatus::INVALIDSYMBOL));
				return Message{content, Task::getClient()->getSocket()};
			}
			latest_data = stock_helper.getLastRowFromDB(this->symbol);
		}

		// check for price slip
		double latest_price = latest_data.close;
		if (!((latest_price > 0.99 * this->asking_price) && (latest_price < 1.01 * this->asking_price)))
		{
			content.push_back(static_cast<uint8_t>(MsgType::ORDERSLIPPED));
			content.append(n_reqId);
			return Message{content, Task::getClient()->getSocket()};
		}

		// after the slip check we need to check again the client balance
		if ((latest_price * this->quantity) > user_balance)
		{
			content.push_back(static_cast<uint8_t>(MsgType::INVALIDORDER));
			content.append(n_reqId);
			content.push_back(static_cast<uint8_t>(OrderStatus::INVALIDBALANCE));
			return Message{content, Task::getClient()->getSocket()};
		}

		// if the client passed all check we procceed with the order
		uint64_t ts = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		UsrID orderId;

		Order order{this->symbol, Task::getClient()->getCredentials().userId, this->type, ts, latest_price, this->quantity};
		
		{
			TransactionGuard transaction{user_helper.getHandle()};

			OrdersTable ordersDB{user_helper.getHandle()};
			orderId = ordersDB.setOrder(order);

			double newBalance = user_helper.updateBalance(Task::getClient()->getCredentials().userId, -(this->quantity * latest_price));
			if (newBalance < 0)
			{
				content.push_back(static_cast<uint8_t>(MsgType::INVALIDORDER));
				content.append(n_reqId);
				content.push_back(static_cast<uint8_t>(OrderStatus::INVALIDBALANCE));
				return Message{content, Task::getClient()->getSocket()};
			}
			transaction.commit();
		}

		// order is committed in the db and now we send back the update order info to the client
		content.push_back(static_cast<uint8_t>(MsgType::ORDERCOMMITED));
		content.append(n_reqId);
		append_8bytes_num(content, latest_price);
		append_8bytes_num(content, ts);
		uint32_t n_orderId = htonl(orderId);
		content.append(reinterpret_cast<char *>(&n_orderId, 4));
		return Message{content, Task::getClient()->getSocket()};
	}
}