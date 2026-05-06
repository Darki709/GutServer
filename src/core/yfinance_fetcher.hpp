#pragma once

#include <string>
#include <ctime>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace Gut
{

	// ═══════════════════════════════════════════════════════════════════════════════
	//  Interval mapping
	// ═══════════════════════════════════════════════════════════════════════════════

	inline static const std::unordered_map<int, std::string> INTERVAL_MAP = {
		{60, "1m"},
		{300, "5m"},
		{900, "15m"},
		{3600, "1h"},
		{86400, "1d"},
	};

	static std::string seconds_to_interval(int sec)
	{
		auto it = INTERVAL_MAP.find(sec);
		return (it != INTERVAL_MAP.end()) ? it->second : "1m";
	}

	class YFinance_fetcher
	{
		public:
		/**
		 * @brief Fetch live OHLCV data for @p ticker at @p interval_sec resolution
		 *        and upsert into the local SQLite database.
		 * @param ticker       e.g. "AAPL", "BTC-USD"
		 * @param interval_sec one of: 60, 300, 900, 3600, 86400
		 * @param db_path      path to the SQLite database file
		 * @return int         0 on success, -1 on fetch/parse error, -2 on DB error
		 */
		static int	fetch_live_data(const std::string &ticker,
						int interval_sec,
						const std::string &db_path);
	};
}
