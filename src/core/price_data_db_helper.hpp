#pragma once

#include "../external/sqlite3.h"
#include "db_utilities.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Gut
{
    // ── Interval string helpers ───────────────────────────────────────────────
    // Defined here so both the fetcher and db helper share one copy.

    inline static const std::unordered_map<int, std::string> INTERVAL_MAP = {
        {    60, "1m"  },
        {   300, "5m"  },
        {   900, "15m" },
        {  3600, "1h"  },
        { 86400, "1d"  },
    };

    inline static std::string seconds_to_interval(int sec)
    {
        auto it = INTERVAL_MAP.find(sec);
        return (it != INTERVAL_MAP.end()) ? it->second : "1m";
    }

    // ─────────────────────────────────────────────────────────────────────────

    struct StockData
    {
        uint64_t ts     = 0;
        double   open   = 0.0;
        double   high   = 0.0;
        double   low    = 0.0;
        double   close  = 0.0;
        uint64_t volume = 0;
    };

    enum class Interval : uint32_t
    {
        MIN_1  =    60,
        MIN_5  =   300,
        MIN_15 =   900,
        HOUR_1 =  3600,
        DAY_1  = 86400
    };

    class Price_data_db_helper : public Table_helper
    {
    public:
        Price_data_db_helper()  = default;
        ~Price_data_db_helper() = default;

        /** Create tables if they don't exist. Call once at startup. */
        int init_database(const std::string& db_path);

        /**
         * Returns the unix timestamp of the last stored candle for this
         * ticker+interval, or 0 if no history exists.
         */
        uint64_t get_last_fetch_time(const std::string& ticker,
                                     const std::string& interval);

        /**
         * Upsert rows into price_history and update fetch_history.
         * Returns number of rows written, or -1 on error.
         */
        int insert_price_data(const std::string&           ticker,
                              const std::string&           interval,
                              const std::vector<StockData>& rows);

        /**
         * Returns the most-recent candle for symbol+interval, or nullopt.
         * interval defaults to "1m" if omitted.
         */
        std::optional<StockData> getLastRow(String& symbol,
                                            const std::string& interval = "1m");

        /**
         * Fetch a range of candles from price_history.
         * start_ts / end_ts are optional unix timestamps (inclusive).
         */
        std::vector<StockData> fetchPriceData(String&                  symbol,
                                              Interval                 interval,
                                              std::optional<uint64_t>  start_ts,
                                              std::optional<uint64_t>  end_ts);
    };

} // namespace Gut