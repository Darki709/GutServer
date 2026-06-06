#pragma once

#include "../libraries.hpp"
#include "task.hpp"
#include "../stateless/stock_db_helper.hpp"
#include "../external/sqlite3.h"
#include "../stateless/streamer.hpp"
#include "../core/price_data_db_helper.hpp"


namespace Gut
{
	enum class Flags : uint8_t
	{
		SNAPSHOT = 1 << 0, // first bit
		STREAM = 1 << 1,   // second bit
	};

	inline uint8_t operator&(uint8_t a, Flags b)
	{
		return static_cast<uint8_t>(a & static_cast<uint8_t>(b));
	}

	class RequestTickerData : public Task
	{
	private:
		String symbol;
		Interval interval;

		//unix timestamp use 
		uint64_t start_ts; //0 for the earliest date
		uint64_t end_ts;//0 for current date

		//candle count limit
		uint32_t limit;

		// does client want price streaming
		bool stream;
		// does client want historical data
		bool snapshot;

	public:
		RequestTickerData(std::shared_ptr<Client> &client, uint32_t reqId, String content);
		std::optional<Message> execute(ThreadResources& resources) override;
	};
}