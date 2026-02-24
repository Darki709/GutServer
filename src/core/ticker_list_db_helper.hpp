#include "../libraries.hpp"
#include "../external/sqlite3.h"

namespace Gut
{
	class TickerListDBHelper //db helper for the ticker name/symbol search, it will handle the database connection and queries for the SearchTicker task
	{
	private:
		sqlite3* db;
	public:
		const std::unique_ptr<std::vector<TickerInfo>> getTickerList(String& query, short limit, uint32_t lastTickerId);
		TickerListDBHelper();
		~TickerListDBHelper();
	};

	struct TickerInfo
	{
		const uint32_t id;
		const String name;
		const String symbol;
	};
}