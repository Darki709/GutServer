#pragma once

#include <string>
#include <ctime>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <stdexcept>

#include "../libraries.hpp"
#include "price_data_db_helper.hpp"  // brings in StockData, Interval,
                                      // seconds_to_interval, Table_helper,
                                      // SecureStmt, TransactionGuard

namespace Gut
{
    class YFinance_fetcher
    {
    public:
        /**
         * @brief Fetch live OHLCV data for @p ticker at @p interval resolution
         *        and upsert into the local SQLite database.
         *
         * @param ticker   e.g. "AAPL", "BTC-USD"
         * @param interval one of Interval::{MIN_1, MIN_5, MIN_15, HOUR_1, DAY_1}
         * @return  0  success
         *         -1  network / parse / no-new-data error
         *         -2  database error
         */
        static int fetch_price_data(String& ticker, Interval interval);
    };

} // namespace Gut