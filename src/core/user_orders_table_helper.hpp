#pragma once
#include "../external/sqlite3.h"
#include "../libraries.hpp"
#include "db_utilities.hpp"

namespace Gut
{

	enum class OrderType : uint8_t{
		LONG,
		SHORT
	};

	enum class OrderView : uint8_t{
		ACTIVE,
		INACTIVE
	};

	class Order
	{
	private:
		String symbol;
		UsrID usrId;
		OrderType type;
		uint64_t entry_ts;
		double entry_price;
		int quantity;
		UsrID orderId = -1; //if order still has no id (new order before entered to db)
		bool active;
		std::optional<uint64_t> end_ts;
		std::optional<double> end_price;
	public:
		explicit Order(String symbol, UsrID usrId, OrderType type ,uint64_t ts, double unit_price, int quantity); //used to create new orders
		explicit Order(String symbol, UsrID usrId, OrderType type, uint64_t ts, double unit_price, int quantity, UsrID orderId); // used when fetching active orders
		explicit Order(String symbol, UsrID usrId, OrderType type, uint64_t ts, double unit_price, int quantity, UsrID orderId, uint64_t end_ts, double end_price); //used when fetching inactive orders
		
		String to_string(); //returns formatted string for network communication
		//order byte layout [1 byte symbol length | symbol | 4 bytes order id | 1 byte order type (e.g. long, short, etc...) | 8 bytes entry price as double | 8 bytes entry ts as unsigned long | 4 bytes quantity | 1 byte is order active (0 inactive, 1 active) | (if order inactive) 8 bytes end price as double | 8 bytes end ts as unsigned long]

		bool is_active();

		friend class OrdersTable;
	};

	struct OrderFilters {
    UsrID userId;
    std::optional<std::string> symbol;
    std::optional<OrderView> view;
    uint32_t limit = 100;
    uint32_t offset = 0;
	};



	static OrderType orderTypeFromInt(int type){
		switch(type){
			case 0:
				return OrderType::LONG;
			case 1:
				return OrderType::SHORT;
			default:
				throw std::invalid_argument("order type only ranges from 0 to 1");
		}
	}

	class OrdersTable : public Table_helper
	{
	public:
		OrdersTable();
		OrdersTable(sqlite3 *db);
		~OrdersTable() = default;
		uint32_t setOrder(Order &order);  // returns order id on success
		double closeOrder(UsrID orderId, double end_price, uint64_t end_ts); // returns the profit/loss for the order (checks the order type then return p/l as signed double), throws if there are errors
		std::vector<Order> fetchOrders(const OrderFilters& filter); //fetches all users orders according to action
	};

	//table creation query
	inline const String create_order_table_query = R"(
	CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL, 
    symbol TEXT NOT NULL,
	type INTEGER NOT NULL, -- 0: Long, 1: Short
    entry_price REAL NOT NULL,
	entry_ts INTEGER NOT NULL,
	quantity INTEGER NOT NULL,
    active BOOLEAN DEFAULT 1 NOT NULL,
    end_ts INTEGER,
    end_price REAL,
    
    -- THE CONSTRAINT:
    -- If active is 0 (false), the second half of the OR must be true.
    -- If active is 1 (true), the constraint passes regardless of the nulls.
    CONSTRAINT portfolio_closure_check 
    CHECK (active = 1 OR (end_ts IS NOT NULL AND end_price IS NOT NULL))
	);
	)";
}