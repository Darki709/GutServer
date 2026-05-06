#include "price_data_db_helper.hpp"

namespace Gut
{
	int Price_data_db_helper::init_database(const std::string &db_path)
	{
		try
		{
			exec(R"sql(
            CREATE TABLE IF NOT EXISTS fetch_history (
                ticker     TEXT,
                interval   TEXT,
                fetch_time DATETIME NOT NULL,
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

			// Index for fast range scans
			exec(R"sql(
            CREATE INDEX IF NOT EXISTS idx_price_ticker_interval_date
                ON price_history (ticker, interval, date);
        )sql");

			return 0;
		}
		catch (const std::exception &e)
		{
			std::cerr << "[init_database] " << e.what() << '\n';
			return -1;
		}
	}

	uint64_t Price_data_db_helper::get_last_fetch_time(const std::string &ticker,
													   const std::string &interval)
	{
		const char *sql =
			"SELECT fetch_time FROM fetch_history WHERE ticker=? AND interval=?;";

		SecureStmt stmt(db, sql);
		sqlite3_bind_text(stmt.stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);

		if (sqlite3_step(stmt.stmt) == SQLITE_ROW)
		{
			// Stored as "YYYY-MM-DD HH:MM:SS"
			const char *text =
				reinterpret_cast<const char *>(sqlite3_column_text(stmt.stmt, 0));
			if (!text)
				return 0;

			struct tm t{};
			sscanf_s(text, "%d-%d-%d %d:%d:%d",
					 &t.tm_year, &t.tm_mon, &t.tm_mday,
					 &t.tm_hour, &t.tm_min, &t.tm_sec);
			t.tm_year -= 1900;
			t.tm_mon -= 1;
			t.tm_isdst = -1;
			return mktime(&t);
		}
		return 0;
	}

	int Price_data_db_helper::insert_price_data(
		const std::string &ticker,
		const std::string &interval,
		const std::vector<OHLCVRow> &rows)
	{
		if (rows.empty())
			return 0;

		try
		{
			// ── Begin transaction ──────────────────────────────────────────────
			char *errmsg = nullptr;
			if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg) != SQLITE_OK)
			{
				std::string e(errmsg ? errmsg : "unknown");
				sqlite3_free(errmsg);
				throw std::runtime_error("BEGIN: " + e);
			}

			// ── Upsert price rows ──────────────────────────────────────────────
			const char *upsert_sql = R"sql(
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

			for (const auto &row : rows)
			{
				sqlite3_reset(stmt.stmt);
				sqlite3_bind_text(stmt.stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
				sqlite3_bind_text(stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);
				sqlite3_bind_int64(stmt.stmt, 3, static_cast<sqlite3_int64>(row.timestamp));
				sqlite3_bind_double(stmt.stmt, 4, row.open);
				sqlite3_bind_double(stmt.stmt, 5, row.high);
				sqlite3_bind_double(stmt.stmt, 6, row.low);
				sqlite3_bind_double(stmt.stmt, 7, row.close);
				sqlite3_bind_int64(stmt.stmt, 8, static_cast<sqlite3_int64>(row.volume));

				int rc = sqlite3_step(stmt.stmt);
				if (rc != SQLITE_DONE && rc != SQLITE_ROW)
					throw std::runtime_error(std::string("step: ") + sqlite3_errmsg(db));
			}

			// ── Update fetch_history with the last timestamp ───────────────────
			std::time_t last_ts = rows.back().timestamp;
			struct tm *lt = std::localtime(&last_ts);
			char time_buf[32];
			std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", lt);

			const char *hist_sql = R"sql(
            INSERT INTO fetch_history (ticker, interval, fetch_time)
            VALUES (?, ?, ?)
            ON CONFLICT(ticker, interval) DO UPDATE SET
                fetch_time = excluded.fetch_time;
        )sql";

			SecureStmt hist_stmt(db, hist_sql);
			sqlite3_bind_text(hist_stmt.stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(hist_stmt.stmt, 2, interval.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(hist_stmt.stmt, 3, time_buf, -1, SQLITE_STATIC);

			if (sqlite3_step(hist_stmt.stmt) != SQLITE_DONE)
				throw std::runtime_error(std::string("hist step: ") + sqlite3_errmsg(db));

			// ── Commit ─────────────────────────────────────────────────────────
			if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg) != SQLITE_OK)
			{
				std::string e(errmsg ? errmsg : "unknown");
				sqlite3_free(errmsg);
				throw std::runtime_error("COMMIT: " + e);
			}

			return static_cast<int>(rows.size());
		}
		catch (const std::exception &e)
		{
			sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
			std::cerr << "[insert_price_data] " << e.what() << '\n';
			return -1;
		}
	}
}