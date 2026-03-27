#include "ticker_list_db_helper.hpp"

namespace Gut
{
	TickerListDBHelper::TickerListDBHelper()
	{
		// get db connection
		//  get the db path
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(NULL, path, MAX_PATH);
		std::filesystem::path exePath(path);
		std::filesystem::path exeDir = exePath.parent_path();			   // This is build/Debug/
		std::filesystem::path dbPath = exeDir / "database" / "tickers.db"; // db path
		std::string db_path = dbPath.string();

		// connect to db
		int rc = sqlite3_open(db_path.c_str(), &db);
		if (rc != SQLITE_OK)
		{
			throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
		}
		sqlite3_exec(db, "PRAGMA cache_size = -2000;", nullptr, nullptr, nullptr);
	}
	TickerListDBHelper::~TickerListDBHelper()
	{
		// close db connection
		if (db)
		{
			sqlite3_close(db);
		}
	}

	std::unique_ptr<std::vector<TickerInfo>> TickerListDBHelper::getTickerList(String &query, short limit, uint32_t lastTickerId)
	{
		const char *sqlQuery = "SELECT id, name, symbol FROM tickers WHERE (name LIKE ? OR symbol LIKE ?) AND id > ? ORDER BY id ASC LIMIT ?";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, nullptr) != SQLITE_OK)
			throw std::runtime_error("Failed to prepare statement: " + String(sqlite3_errmsg(db)));

		String likeQuery = query + "%";
		sqlite3_bind_text(stmt, 1, likeQuery.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 2, likeQuery.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 3, lastTickerId);
		sqlite3_bind_int(stmt, 4, limit);

		auto tickerList = std::make_unique<std::vector<TickerInfo>>();
		try
		{
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				uint32_t id = sqlite3_column_int(stmt, 0);
				String name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
				String symbol = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
				tickerList->emplace_back(TickerInfo{id, name, symbol});
			}
		}
		catch (...)
		{
			sqlite3_finalize(stmt);
			std::cout << "Failed to execute query: " << sqlQuery << " with error: " << sqlite3_errmsg(db) << std::endl;
			throw;
		}
		if (sqlite3_finalize(stmt) != SQLITE_OK)
			throw std::runtime_error("Failed to finalize statement: " + String(sqlite3_errmsg(db)));
		return tickerList;
	}

	TickerInformation TickerListDBHelper::getTickerInfo(String &symbol)
	{
		const char *query = "SELECT name, exchange, asset_type, sector FROM tickers WHERE symbol = ?";
		sqlite3_stmt *stmt = nullptr;

		if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK)
		{
			throw std::runtime_error("Failed to prepare: " + String(sqlite3_errmsg(db)));
		}
		sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_STATIC);

		TickerInformation result;
		bool found = false;

		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			auto raw_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
			auto raw_exch = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
			auto raw_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
			auto raw_sect = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

			result.name = raw_name ? raw_name : "";
			result.exchange = raw_exch ? raw_exch : "";
			String s_type = (raw_type ? raw_type : "");
			result.sector = raw_sect ? raw_sect : "N/A";

			found = true;
		}
		sqlite3_finalize(stmt);

		if (!found)
		{
			throw std::invalid_argument("Ticker " + symbol + " not found in database");
		}

		return result;
	}

	static AssetType stoat(String type)
	{
		if (type == "STOCK")
			return AssetType::STOCK;
		if (type == "CRYPTO")
			return AssetType::CRYPTO;
		if (type == "ETF")
			return AssetType::ETF;
		if (type == "FOREX")
			return AssetType::FOREX;
	}
}