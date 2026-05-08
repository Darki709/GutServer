#include "endOrder.hpp"
#include "../runtime/worker.hpp"
#include "../core/price_data_db_helper.hpp"

Gut::EndOrder::EndOrder(std::shared_ptr<Client> &client, uint32_t reqId, String content)
	: Task(client, reqId)
{
	// Payload: [4B OrderID | 8B ExpectedPrice | 1B PwdLen | Password]
	size_t offset = 0;

	// 1. Manually extract and flip OrderID
	uint32_t netOrderId;
	memcpy(&netOrderId, content.data() + offset, 4);
	orderId = ntohl(netOrderId);
	offset += 4;

	// 2. Manually handle ntohd (8-byte double endianness)
	uint64_t rawPrice;
	memcpy(&rawPrice, content.data() + offset, 8);
	uint64_t hostPrice = ntohll(rawPrice);	// Flip bytes
	memcpy(&expected_price, &hostPrice, 8); // Re-interpret as double
	offset += 8;

	// 3. Extract Password
	uint8_t pwdLen = static_cast<uint8_t>(content[offset]);
	offset += 1;
	password = content.substr(offset, pwdLen);
}

std::optional<Gut::Message> Gut::EndOrder::execute(ThreadResources& resources)
{
    OrdersTable ordersDb;
    User_table userDb;

    std::cout << "[DEBUG] Executing EndOrder for OrderID: " << orderId << " | Expected Price: " << expected_price << std::endl;

    // --- VALIDATION ---
    if (userDb.authenticateUser(Task::getClient()->getCredentials().username, password) < 0)
    {
        std::cerr << "[ERROR] Auth failed for user: " << Task::getClient()->getCredentials().username << std::endl;
        throw Errors::ILLEGALACCESS; 
    }

    OrderFilters filter;
    filter.userId = Task::getClient()->getCredentials().userId;
    filter.view = OrderView::ACTIVE;
    auto userOrders = ordersDb.fetchOrders(filter);

    auto it = std::find_if(userOrders.begin(), userOrders.end(),
                           [this](const Order &o)
                           { return o.orderId == this->orderId; });

    if (it == userOrders.end())
    {
        std::cout << "[DEBUG] Order " << orderId << " not found or not active for user." << std::endl;
        String res;
        res.push_back(static_cast<uint8_t>(MsgType::ORDERFAILEDEXIT));
        uint32_t netReqId = htonl(Task::getReqId());
        append_bytes(res, netReqId);
        res.push_back(static_cast<uint8_t>(Status::ORDERNOTEXIST));
        return Message{res, Task::getClient()->getSocket()};
    }

    // --- MARKET CHECK ---
    Order &targetOrder = *it;
    YFinance_fetcher::fetch_price_data(targetOrder.symbol, Interval::MIN_1);
	Price_data_db_helper price_helper;
    double actualPrice = price_helper.getLastRow(targetOrder.symbol).value().close;

    std::cout << "[DEBUG] Market Check - Symbol: " << targetOrder.symbol << " | Current Price: " << actualPrice << std::endl;

    // --- SLIPPAGE (1%) ---
    double threshold = expected_price * 0.01;
    bool blocked = false;
    if (targetOrder.type == OrderType::LONG && actualPrice < (expected_price - threshold))
        blocked = true;
    if (targetOrder.type == OrderType::SHORT && actualPrice > (expected_price + threshold))
        blocked = true;

    if (blocked)
    {
        std::cout << "[DEBUG] Slippage check failed. Price outside 1% range of " << expected_price << std::endl;
        String res;
        res.push_back(static_cast<uint8_t>(MsgType::ORDERFAILEDEXIT));
        uint32_t netReqId = htonl(Task::getReqId());
        append_bytes(res, netReqId);
        res.push_back(static_cast<uint8_t>(Status::EXPECTEDPRICEOUTOFRANGE));
        return Message{res, Task::getClient()->getSocket()};
    }

    // --- EXECUTION ---
    uint64_t endTs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::cout << "[DEBUG] Closing order at price: " << actualPrice << "..." << std::endl;

    double pnl = ordersDb.closeOrder(orderId, actualPrice, endTs);
    userDb.updateBalance(Task::getClient()->getCredentials().userId, pnl);

    std::cout << "[DEBUG] Order Closed. PnL: " << pnl << " | New Balance updated for UserID: " << Task::getClient()->getCredentials().userId << std::endl;

    // Build Type 13 (ORDEREXISTTED)
    String res;
    res.push_back(static_cast<uint8_t>(MsgType::ORDEREXITTED));
    uint32_t netReqId = htonl(Task::getReqId());
    append_bytes(res, netReqId);

    append_8bytes_num(res, actualPrice); 

    uint64_t netTs = htonll(endTs);
    append_bytes(res, netTs);

    std::cout << "[DEBUG] Response built successfully for ReqID: " << Task::getReqId() << std::endl;

    return Message{res, Task::getClient()->getSocket()};
}