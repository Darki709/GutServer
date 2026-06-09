#include "db_initializer.hpp"

#include "../external/sqlite3.h"
#include "db_utilities.hpp"

// Each table's DDL is owned by its helper header — include them so this stays the
// single bootstrap point without duplicating any schema.
#include "user_db_helper.hpp"            // users:       query
#include "user_orders_table_helper.hpp"  // orders:      create_order_table_query
#include "list_db_helper.hpp"            // lists/items: lists_table_layout, items_table_layout
#include "chart_state_db_helper.hpp"     // chart_state: chart_state_table_layout
#include "alert_state_db_helper.hpp"     // alert_state: alert_state_table_layout
#include "price_data_db_helper.hpp"      // price:       fetch_/price_history + index layouts

namespace Gut
{
	bool DB_Initializer::initialize()
	{
		// ── Resolve <exe dir>/database/stock_data.db (same convention as Table_helper) ──
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(NULL, path, MAX_PATH);
		std::filesystem::path exeDir = std::filesystem::path(path).parent_path();
		std::filesystem::path dbDir  = exeDir / "database";

		// Ensure the directory exists so sqlite3_open can create the file on a fresh box.
		// (No-op if it already exists, e.g. because tickers.db was placed there first.)
		std::error_code ec;
		std::filesystem::create_directories(dbDir, ec);
		if (ec)
		{
			std::cerr << "[DB][init] could not create directory " << dbDir.string()
					  << ": " << ec.message() << "\n";
			return false;
		}

		std::filesystem::path dbPath = dbDir / "stock_data.db";

		sqlite3 *db = nullptr;
		if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK)
		{
			std::cerr << "[DB][init] cannot open " << dbPath.string() << ": "
					  << (db ? sqlite3_errmsg(db) : "unknown") << "\n";
			if (db) sqlite3_close(db);
			return false;
		}

		// Small local exec helper with clear per-table error reporting.
		auto run = [&](const char *sql, const char *name) -> bool
		{
			char *err = nullptr;
			if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK)
			{
				std::cerr << "[DB][init] failed creating " << name << ": "
						  << (err ? err : "unknown") << "\n";
				sqlite3_free(err);
				return false;
			}
			return true;
		};

		// Match the runtime journaling mode used by Table_helper.
		run("PRAGMA journal_mode=WAL;", "pragma:journal_mode");

		// DDL is transactional in SQLite — create everything atomically.
		bool ok = true;
		sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);

		// NOTE: the `tickers` table is intentionally absent — it lives in tickers.db and
		// is loaded manually before the server starts.
		ok = ok && run(query,                       "users");
		ok = ok && run(create_order_table_query.c_str(), "orders");
		ok = ok && run(lists_table_layout,          "lists");
		ok = ok && run(items_table_layout,          "list_items");
		ok = ok && run(chart_state_table_layout,    "chart_state");
		ok = ok && run(alert_state_table_layout,    "alert_state");
		ok = ok && run(fetch_history_table_layout,  "fetch_history");
		ok = ok && run(price_history_table_layout,  "price_history");
		ok = ok && run(price_history_index_layout,  "price_history index");

		if (ok)
			sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
		else
			sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);

		sqlite3_close(db);

		if (ok)
			std::cout << "[DB][init] stock_data.db ready (all tables except tickers).\n";
		else
			std::cerr << "[DB][init] initialization failed — see errors above.\n";
		return ok;
	}
}
