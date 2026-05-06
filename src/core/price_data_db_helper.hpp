#pragma once

#include "../external/sqlite3.h"
#include "db_utilities.hpp"

namespace Gut
{

	struct OHLCVRow
	{
		uint64_t timestamp; // unix epoch
		double open;
		double high;
		double low;
		double close;
		uint64_t volume;
	};

	class Price_data_db_helper : public Table_helper
	{
	public:
		/**
		 * @brief Ensure both `fetch_history` and `price_history` tables exist.
		 *        Call once at application startup before any fetch_live_data() call.
		 */
		int init_database(const std::string &db_path);

		/**
		 * @brief Get the last fetch time for a given ticker+interval from the `fetch_history` table.
		 * @param ticker   e.g. "AAPL", "BTC-USD"
		 * @param interval e.g. "1m", "5m", "15m", "1h", "1d"
		 * @return Returns the last fetch time as unix timestamp, or 0 if never fetched.
		 */
		uint64_t get_last_fetch_time(const std::string &ticker,
									 const std::string &interval);

		/**
		 * @brief Upsert price rows + update fetch_history, all inside one transaction.
		 * @param ticker   e.g. "AAPL", "BTC-USD"
		 * @param interval e.g. "1m", "5m", "15m", "1h", "1d"
		 * @param rows     Vector of OHLCV rows to insert/update
		 * @return Returns number of rows processed, or -1 on error.
		 */
		int insert_price_data(const std::string &ticker,
							  const std::string &interval,
							  const std::vector<OHLCVRow> &rows);


							  
		Price_data_db_helper() = default;
		~Price_data_db_helper() = default;
	};
}
