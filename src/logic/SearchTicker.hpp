#include "../libraries.hpp"
#include "task.hpp"
#include "../core/ticker_list_db_helper.hpp"

namespace Gut
{
	class SearchTicker : public Task
	{
	private:
		std::string query;//the ticker name/symbol that is searched for, can be a partial name/symbol, the server will return all tickers that match the query
		uint32_t lastTickerId = 0;//required for pagination, the id of the last ticker sent to the client, so the server can send the next tickers in the next request, default is 0, which means the server will send the first tickers in the database
		uint8_t pageSize = 5;//maximum number of results return by a search request, default is 5 this is also the number used in fast mode

	public:
		SearchTicker(std::shared_ptr<Client>& client, uint32_t reqId, std::string content);
		std::optional<Message> execute() override;
	};
}