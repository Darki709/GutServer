#include "fetchorders.hpp"

namespace Gut
{
	FetchOrdersTask::FetchOrdersTask(std::shared_ptr<Gut::Client> &client, uint32_t reqId, String payload) : Task(client, reqId)
	{
		size_t offset = 0;

		// 1. Read Symbol Length
		uint8_t symLen = static_cast<uint8_t>(payload[offset++]);

		// 2. Read Symbol (Optional)
		if (symLen > 0)
		{
			m_filters.symbol = payload.substr(offset, symLen);
			offset += symLen;
		}
		else
		{
			m_filters.symbol = std::nullopt; //
		}

		// 3. Read View Filter (1 byte)
		uint8_t viewVal = static_cast<uint8_t>(payload[offset++]);
		m_filters.view = static_cast<OrderView>(viewVal); //

		// 4. Read Limit (4 bytes Big Endian)
		uint32_t netLimit;
		memcpy(&netLimit, &payload[offset], 4);
		m_filters.limit = ntohl(netLimit); // Convert to Host Endian
		offset += 4;

		// 5. Read Offset (4 bytes Big Endian)
		uint32_t netOffset;
		memcpy(&netOffset, &payload[offset], 4);
		m_filters.offset = ntohl(netOffset); // Convert to Host Endian

		// Note: userId is usually handled by the session/client context
		m_filters.userId = Task::getClient()->getCredentials().userId;

		std::cout << "[DEBUG] FetchOrders ID:" << getReqId() << " User:" << m_filters.userId << " Sym:" << (m_filters.symbol ? *m_filters.symbol : "ALL") << " View:" << static_cast<int>(m_filters.view.value()) << " Lim:" << m_filters.limit << " Off:" << m_filters.offset << std::endl;
	}

	std::optional<Message> FetchOrdersTask::execute(ThreadResources& resources)
	{

		// 1. Fetch the data from SQLite
		std::vector<Order> orders;

		OrdersTable ordersTable;
		try
		{
			orders = ordersTable.fetchOrders(m_filters);
		}
		catch (const std::exception &e)
		{
			std::cerr << "[ERROR] Database fetch failed: " << e.what() << std::endl;
			return std::nullopt;
		}

		// 2. Prepare the binary response buffer
		std::string response;

		// Header: [1 byte Message type | 4 bytes client request id | 4 bytes order count]
		response.push_back(static_cast<uint8_t>(MsgType::ORDERSBATCH));

		uint32_t netReqId = htonl(getReqId());
		response.append(reinterpret_cast<const char *>(&netReqId), 4);

		uint32_t count = htonl(static_cast<uint32_t>(orders.size()));
		response.append(reinterpret_cast<const char *>(&count), 4);

		// 3. Serialize each Order using its binary to_string() method
		for (auto &order : orders)
		{
			// Appends: [1b symLen | sym | 4b ID | 1b Type | 8b Price | 8b TS | 4b Qty | 1b Active | (opt) 8b EndPrice | 8b EndTS]
			response.append(order.to_string());
		}

		// 4. Wrap in Message and return
		return Message{response, getClient()->getSocket()};
	}
}