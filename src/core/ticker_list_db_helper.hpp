#pragma once

#include "../libraries.hpp"
#include "../external/sqlite3.h"

namespace Gut
{
	enum class AssetType : uint8_t{
		STOCK,
		CRYPTO,
		ETF,
		FOREX		
	};
	struct TickerInfo
	{
		const uint32_t id = 0;
		const String name;
		const String symbol;
	};

	struct TickerInformation{
		String name;
		String exchange;
		AssetType type;
		String sector;
	};

	//convert string to AssetType
	static AssetType stoat(String& type);
	class TickerListDBHelper //db helper for the ticker name/symbol search, it will handle the database connection and queries for the SearchTicker task
	{
	private:
		sqlite3* db;
	public:
		std::unique_ptr<std::vector<TickerInfo>> getTickerList(String& query, short limit, uint32_t lastTickerId);
		TickerInformation getTickerInfo(String& symbol);
		TickerListDBHelper();
		~TickerListDBHelper();
	};
}