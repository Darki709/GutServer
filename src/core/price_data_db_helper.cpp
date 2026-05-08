// price_data_db_helper.cpp

#include "price_data_db_helper.hpp"
#include "db_utilities.hpp"
#include <cstdio>
#include <cstring>

namespace Gut
{
    // ─────────────────────────────────────────────────────────────────────────
    //  init_database
    // ─────────────────────────────────────────────────────────────────────────
    int Price_data_db_helper::init_database(const std::string& /*db_path*/)
    {
        try
        {
            // fetch_time stored as INTEGER unix timestamp (not a formatted string)
            // so get_last_fetch_time never needs localtime/mktime and is timezone-proof.
            exec(R"sql(
                CREATE TABLE IF NOT EXISTS fetch_history (
                    ticker     TEXT,
                    interval   TEXT,
                    fetch_time INTEGER NOT NULL,
                    PRIMARY KEY (ticker, interval)
                );
            )sql");

            exec(R"sql(
                CREATE TABLE IF NOT EXISTS price_history (
                    ticker   TEXT,
                    interval TEXT,
                    date     BIGINT  NOT NULL,
                    open     REAL,
                    high     REAL,
                    low      REAL,
                    close    REAL,
                    volume   INTEGER,
                    PRIMARY KEY (ticker, interval, date)
                );
            )sql");

            exec(R"sql(
                CREATE INDEX IF NOT EXISTS idx_price_ticker_interval_date
                    ON price_history (ticker, interval, date);
            )sql");

            std::cout << "[DB] Tables initialised.\n";
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DB][init_database] " << e.what() << '\n';
            return -1;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  get_last_fetch_time
    //  Returns the unix timestamp of the last stored candle, or 0 if none.
    // ─────────────────────────────────────────────────────────────────────────
    uint64_t Price_data_db_helper::get_last_fetch_time(const std::string& ticker,
                                                        const std::string& interval)
    {
        // fetch_time is stored as a raw INTEGER unix timestamp — no string parsing,
        // no mktime(), no timezone sensitivity.
        const char* sql =
            "SELECT fetch_time FROM fetch_history WHERE ticker=? AND interval=?;";

        SecureStmt stmt(db, sql);
        sqlite3_bind_text(stmt.stmt, 1, ticker.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt.stmt) == SQLITE_ROW)
        {
            uint64_t ts = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 0));

            // Human-readable log
            std::time_t t = static_cast<std::time_t>(ts);
            char buf[32]; struct tm lt{};
#ifdef _WIN32
            localtime_s(&lt, &t);
#else
            localtime_r(&t, &lt);
#endif
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
            std::cout << "[DB] Last fetch for " << ticker << " [" << interval
                      << "] = " << buf << " (unix=" << ts << ")\n";
            return ts;
        }

        std::cout << "[DB] No fetch history for " << ticker
                  << " [" << interval << "], will fetch full range.\n";
        return 0;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  insert_price_data
    //
    //  Key correctness note:
    //  fetch_history stores the timestamp of the LAST candle already in the DB.
    //  The fetcher adds one interval to that value to use as period1, so we
    //  never re-download the candle we already have.
    //  Here we store the raw last-candle timestamp; the +interval offset is
    //  applied in the fetcher before constructing the URL.
    // ─────────────────────────────────────────────────────────────────────────
    int Price_data_db_helper::insert_price_data(
        const std::string&           ticker,
        const std::string&           interval,
        const std::vector<StockData>& rows)
    {
        if (rows.empty())
        {
            std::cout << "[DB] insert_price_data: 0 rows for "
                      << ticker << " [" << interval << "], nothing to do.\n";
            return 0;
        }

        std::cout << "[DB] Inserting " << rows.size() << " rows for "
                  << ticker << " [" << interval << "] ...\n";

        try
        {
            // TransactionGuard issues BEGIN IMMEDIATE on construction and
            // ROLLBACK on destruction unless commit() has been called.
            TransactionGuard txn(db);

            // ── Upsert rows ──────────────────────────────────────────────────
            const char* upsert_sql = R"sql(
                INSERT INTO price_history (ticker, interval, date, open, high, low, close, volume)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(ticker, interval, date) DO UPDATE SET
                    open   = excluded.open,
                    high   = excluded.high,
                    low    = excluded.low,
                    close  = excluded.close,
                    volume = excluded.volume;
            )sql";

            SecureStmt stmt(db, upsert_sql);

            for (const auto& row : rows)
            {
                sqlite3_reset(stmt.stmt);
                sqlite3_bind_text  (stmt.stmt, 1, ticker.c_str(),   -1, SQLITE_STATIC);
                sqlite3_bind_text  (stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int64 (stmt.stmt, 3, static_cast<sqlite3_int64>(row.ts));
                sqlite3_bind_double(stmt.stmt, 4, row.open);
                sqlite3_bind_double(stmt.stmt, 5, row.high);
                sqlite3_bind_double(stmt.stmt, 6, row.low);
                sqlite3_bind_double(stmt.stmt, 7, row.close);
                sqlite3_bind_int64 (stmt.stmt, 8, static_cast<sqlite3_int64>(row.volume));

                int rc = sqlite3_step(stmt.stmt);
                if (rc != SQLITE_DONE && rc != SQLITE_ROW)
                    throw std::runtime_error(std::string("upsert step: ") + sqlite3_errmsg(db));
            }

            // ── Update fetch_history with the last candle's unix timestamp ────
            //    Store as raw INTEGER — no formatted string, no timezone dependency.
            sqlite3_int64 last_ts_int = static_cast<sqlite3_int64>(rows.back().ts);

            const char* hist_sql = R"sql(
                INSERT INTO fetch_history (ticker, interval, fetch_time)
                VALUES (?, ?, ?)
                ON CONFLICT(ticker, interval) DO UPDATE SET
                    fetch_time = excluded.fetch_time;
            )sql";

            SecureStmt hist_stmt(db, hist_sql);
            sqlite3_bind_text (hist_stmt.stmt, 1, ticker.c_str(),   -1, SQLITE_STATIC);
            sqlite3_bind_text (hist_stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(hist_stmt.stmt, 3, last_ts_int);

            if (sqlite3_step(hist_stmt.stmt) != SQLITE_DONE)
                throw std::runtime_error(std::string("hist_stmt step: ") + sqlite3_errmsg(db));

            // ── Commit ───────────────────────────────────────────────────────
            txn.commit();

            // Human-readable log
            {
                std::time_t t = static_cast<std::time_t>(last_ts_int);
                char buf[32]; struct tm lt{};
#ifdef _WIN32
                localtime_s(&lt, &t);
#else
                localtime_r(&t, &lt);
#endif
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
                std::cout << "[DB] Committed " << rows.size() << " rows for "
                          << ticker << " [" << interval << "]. "
                          << "fetch_history updated to " << buf
                          << " (unix=" << last_ts_int << ")\n";
            }

            return static_cast<int>(rows.size());
        }
        catch (const std::exception& e)
        {
            // TransactionGuard destructor issues ROLLBACK automatically.
            std::cerr << "[DB][insert_price_data] ROLLBACK — " << e.what() << '\n';
            return -1;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  getLastRow  — returns the most-recent candle for symbol+interval
    // ─────────────────────────────────────────────────────────────────────────
    std::optional<StockData> Price_data_db_helper::getLastRow(String& symbol,
                                                               const std::string& interval)
    {
        const char* sql =
            "SELECT date, open, high, low, close, volume FROM price_history "
            "WHERE ticker = ? AND interval = ? ORDER BY date DESC LIMIT 1;";

        SecureStmt stmt(db, sql);
        sqlite3_bind_text(stmt.stmt, 1, symbol.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt.stmt) == SQLITE_ROW)
        {
            StockData data{};
            data.ts     = static_cast<uint64_t>(sqlite3_column_int64  (stmt.stmt, 0));
            data.open   =                        sqlite3_column_double  (stmt.stmt, 1);
            data.high   =                        sqlite3_column_double  (stmt.stmt, 2);
            data.low    =                        sqlite3_column_double  (stmt.stmt, 3);
            data.close  =                        sqlite3_column_double  (stmt.stmt, 4);
            data.volume = static_cast<uint64_t>(sqlite3_column_int64  (stmt.stmt, 5));
            return data;
        }

        std::cout << "[DB] getLastRow: no data for " << symbol
                  << " [" << interval << "]\n";
        return std::nullopt;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  fetchPriceData
    //  Fixed: correct table (price_history) and column (date) names.
    // ─────────────────────────────────────────────────────────────────────────
    std::vector<StockData> Price_data_db_helper::fetchPriceData(
        String&                  symbol,
        Interval                 interval,
        std::optional<uint64_t>  start_ts,
        std::optional<uint64_t>  end_ts)
    {
        std::vector<StockData> results;

        // interval enum → string
        const std::string iv = seconds_to_interval(static_cast<int>(interval));

        std::string sql =
            "SELECT date, open, high, low, close, volume FROM price_history "
            "WHERE ticker = ? AND interval = ? ";

        if (start_ts.has_value()) sql += "AND date >= ? ";
        if (end_ts.has_value())   sql += "AND date <= ? ";
        sql += "ORDER BY date ASC;";

        try
        {
            SecureStmt ss(db, sql.c_str());

            int bindIdx = 1;
            sqlite3_bind_text (ss.stmt, bindIdx++, symbol.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text (ss.stmt, bindIdx++, iv.c_str(),     -1, SQLITE_TRANSIENT);

            if (start_ts.has_value())
                sqlite3_bind_int64(ss.stmt, bindIdx++,
                                   static_cast<sqlite3_int64>(start_ts.value()));
            if (end_ts.has_value())
                sqlite3_bind_int64(ss.stmt, bindIdx++,
                                   static_cast<sqlite3_int64>(end_ts.value()));

            while (sqlite3_step(ss.stmt) == SQLITE_ROW)
            {
                StockData data{};
                data.ts     = static_cast<uint64_t>(sqlite3_column_int64  (ss.stmt, 0));
                data.open   =                        sqlite3_column_double  (ss.stmt, 1);
                data.high   =                        sqlite3_column_double  (ss.stmt, 2);
                data.low    =                        sqlite3_column_double  (ss.stmt, 3);
                data.close  =                        sqlite3_column_double  (ss.stmt, 4);
                data.volume = static_cast<uint64_t>(sqlite3_column_int64  (ss.stmt, 5));
                results.push_back(data);
            }

            std::cout << "[DB] fetchPriceData: returned " << results.size()
                      << " rows for " << symbol << " [" << iv << "]\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DB][fetchPriceData] " << e.what() << '\n';
        }

        return results;
    }

} // namespace Gut