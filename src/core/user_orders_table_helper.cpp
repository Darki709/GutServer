#include "user_orders_table_helper.hpp"

namespace Gut
{
	Order::Order(String symbol, ID usrId, OrderType type, uint64_t ts, double unit_price, int quantity) : symbol(symbol), usrId(usrId), entry_ts(ts),
																													  entry_price(unit_price), quantity(quantity), active(true), end_price(std::nullopt), end_ts(std::nullopt), type(type) {}
	Order::Order(String symbol, ID usrId, OrderType type, uint64_t ts, double unit_price, int quantity, ID orderId) : symbol(symbol),
																																	 usrId(usrId), entry_ts(ts), entry_price(unit_price), quantity(quantity), orderId(orderId), active(true), end_price(std::nullopt), end_ts(std::nullopt), type(type) {}
	Order::Order(String symbol, ID usrId, OrderType type, uint64_t ts, double unit_price, int quantity, ID orderId, uint64_t end_ts, double end_price) : symbol(symbol), usrId(usrId), entry_ts(ts), entry_price(unit_price), quantity(quantity), orderId(orderId), active(false), end_price(end_price), end_ts(end_ts), type(type) {}

	bool Order::is_active()
	{
		return active;
	}

	String Order::to_string()
	{
		if (orderId == -1)
			throw std::invalid_argument("cannot fetch an unregistered order");
		String formatted_order;
		formatted_order.push_back(symbol.size());
		formatted_order.append(symbol);
		auto n_orderId = htonl(orderId);
		append_bytes(formatted_order, n_orderId);
		formatted_order.push_back(static_cast<uint8_t>(type));
		append_8bytes_num(formatted_order, entry_price);
		append_8bytes_num(formatted_order, entry_ts);
		auto n_quantity = htonl(quantity);
		append_bytes(formatted_order, n_quantity);
		formatted_order.push_back(active);
		if (!active && end_ts.has_value() && end_price.has_value())
		{
			append_8bytes_num(formatted_order, end_price.value());
			append_8bytes_num(formatted_order, end_ts.value());
		}
		return std::move(formatted_order);
	}

	OrdersTable::OrdersTable() : Table_helper() {}

	uint32_t OrdersTable::setOrder(Order &order)
	{
		const char *query = "INSERT INTO orders (user_id, symbol, entry_price, entry_ts, quantity, type) VALUES (?, ?, ?, ?, ?, ?) RETURNING order_id;";
		Gut::SecureStmt stmt{db, query};
		if (sqlite3_prepare(db, query, -1, &stmt.stmt, nullptr) != SQLITE_OK)
		{
			throw std::runtime_error("failed to register order");
		}

		sqlite3_bind_int(stmt.stmt, 1, order.usrId);
		sqlite3_bind_text(stmt.stmt, 2, order.symbol.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_double(stmt.stmt, 3, order.entry_price);
		sqlite3_bind_int64(stmt.stmt, 4, order.entry_ts);
		sqlite3_bind_int(stmt.stmt, 5, order.quantity);
		sqlite3_bind_int(stmt.stmt, 6, static_cast<int>(order.type));

		if (sqlite3_step(stmt.stmt) == SQLITE_ROW)
		{
			return sqlite3_column_int(stmt.stmt, 0);
		}
		throw std::runtime_error("failed to register order");
	}

	std::vector<Order> OrdersTable::fetchOrders(const OrderFilters &filters)
	{
		std::vector<Order> results;

		// 1. Base query - Always filter by userId for security!
		std::string sql = R"(SELECT order_id, user_id, symbol, entry_price, entry_ts, quantity, active,
		 end_ts, end_price, type FROM orders WHERE user_id = ?)";

		// 2. Add dynamic filters
		if (filters.symbol.has_value())
		{
			sql += " AND symbol = ?";
		}

		if (filters.view.has_value())
		{
			if (filters.view == OrderView::ACTIVE)
			{
				sql += " AND active = 1";
			}
			else if (filters.view == OrderView::INACTIVE)
			{
				sql += " AND active = 0";
			}
		}

		// 3. Add Pagination
		sql += " ORDER BY entry_ts DESC LIMIT ? OFFSET ?;";

		Gut::SecureStmt stmt{db, sql.c_str()};
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt.stmt, nullptr) != SQLITE_OK)
		{
			throw std::runtime_error("Failed to prepare fetch query");
		}

		// 4. Binding - The order MUST match the string building above
		int bindIdx = 1;
		sqlite3_bind_int(stmt.stmt, bindIdx++, filters.userId);

		if (filters.symbol.has_value())
		{
			sqlite3_bind_text(stmt.stmt, bindIdx++, filters.symbol->c_str(), -1, SQLITE_TRANSIENT);
		}

		sqlite3_bind_int(stmt.stmt, bindIdx++, filters.limit);
		sqlite3_bind_int(stmt.stmt, bindIdx++, filters.offset);

		results.reserve(filters.limit); // pre-allocate the vector

		// 5. Execute and Populate
		while (sqlite3_step(stmt.stmt) == SQLITE_ROW)
		{
			// 1. Collect the common values into local variables
			uint32_t id = sqlite3_column_int(stmt.stmt, 0);
			ID usr = sqlite3_column_int(stmt.stmt, 1);
			std::string sym = reinterpret_cast<const char *>(sqlite3_column_text(stmt.stmt, 2));
			double price = sqlite3_column_double(stmt.stmt, 3);
			int64_t ts = sqlite3_column_int64(stmt.stmt, 4);
			int qty = sqlite3_column_int(stmt.stmt, 5);
			bool active = sqlite3_column_int(stmt.stmt, 6);
			OrderType type = orderTypeFromInt(sqlite3_column_int(stmt.stmt, 9));

			if (active)
			{
				// Calls the "Active Order" constructor
				results.emplace_back(sym, usr, type, ts, price, qty, id);
			}
			else
			{
				// 2. Collect the extra fields only for inactive orders
				int64_t endTs = sqlite3_column_int64(stmt.stmt, 7);
				double endPrice = sqlite3_column_double(stmt.stmt, 8);

				// Calls the "Closed Order" constructor
				results.emplace_back(sym, usr, type, ts, price, qty, id, endTs, endPrice);
			}
		}
		std::cout << "[DEBUG] Fetched " << results.size() << " orders for User ID: " << filters.userId << std::endl;
		return results;
	}

	double OrdersTable::closeOrder(ID orderId, double end_price, uint64_t end_ts)
	{
		// We fetch entry_price, quantity, and type to calculate P/L
		// We also fetch 'symbol' just to make the exception message more helpful
		const char *query = R"(
        UPDATE orders 
        SET active = 0, end_price = ?, end_ts = ? 
        WHERE order_id = ? AND active = 1
        RETURNING entry_price, quantity, type, symbol;
    )";

		Gut::SecureStmt stmt{db, query};
		if (sqlite3_prepare_v2(db, query, -1, &stmt.stmt, nullptr) != SQLITE_OK)
		{
			throw std::runtime_error("Database Error: Failed to prepare closeOrder statement.");
		}

		sqlite3_bind_double(stmt.stmt, 1, end_price);
		sqlite3_bind_int64(stmt.stmt, 2, end_ts);
		sqlite3_bind_int(stmt.stmt, 3, orderId);

		if (sqlite3_step(stmt.stmt) == SQLITE_ROW)
		{
			double entry_price = sqlite3_column_double(stmt.stmt, 0);
			int quantity = sqlite3_column_int(stmt.stmt, 1);
			OrderType type = orderTypeFromInt(sqlite3_column_int(stmt.stmt, 2));

			// Use a switch for extensibility (e.g., adding Spread or Options later)
			// return the price of the position that was existted
			return end_price * quantity;
		}

		// If we reach here, the order wasn't found or was already closed.
		// We throw an exception instead of returning -1.0
		throw std::runtime_error("Order Close Failed: Order ID " + std::to_string(orderId) +
								 " not found or already inactive.");
	}

	OrdersTable::OrdersTable(sqlite3 *db) : Table_helper(db){}
}