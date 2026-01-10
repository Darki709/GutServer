#include "RequestTickerData.hpp"
#include "../program/server.hpp"

Gut::RequestTickerData::RequestTickerData(std::shared_ptr<Client> &client, uint32_t reqId, String content) : Task(client, reqId)
{
	// check client privileges
	if (static_cast<int>(client->getState()) < 3)
		throw Errors::ILLEGALACCESS;

	// get ticker symbol from the data
	uint8_t symbolLen = static_cast<uint8_t>(content[0]);
	content.erase(0, 1);
	String symbol = content.substr(0, symbolLen);
	content.erase(0, symbolLen);
	this->symbol = symbol;

	// get ticker interval from the data
	uint32_t interval;
	memcpy(&interval, content.data(), 4);
	content.erase(0, 4);
	interval = ntohl(interval);
	switch (interval)
	{
	case 60:
		this->interval = Interval::MIN_1;
		break;
	case 300:
		this->interval = Interval::MIN_5;
		break;
	case 900:
		this->interval = Interval::MIN_15;
		break;
	case 3600:
		this->interval = Interval::HOUR_1;
		break;
	case 86400:
		this->interval = Interval::DAY_1;
		break;
	default:
		throw Errors::INVALIDREQUEST;
	}

	// get timestamps for the data
	uint64_t start_ts;
	uint64_t end_ts;
	memcpy(&start_ts, content.data(), 8);
	content.erase(0, 8);
	memcpy(&end_ts, content.data(), 8);
	content.erase(0, 8);
	this->start_ts = ntohll(start_ts);
	this->end_ts = ntohll(end_ts);

	// parse flags
	uint8_t flags = static_cast<uint8_t>(content[0]);
	this->stream = (flags & Flags::STREAM) != 0;
	this->snapshot = (flags & Flags::SNAPSHOT) != 0;

	std::cout << "RequestTickerData started: " << symbol << interval << start_ts << end_ts << stream << snapshot << std::endl;
}

std::optional<Gut::Message> Gut::RequestTickerData::execute()
{
	// check if client still lives
	std::shared_ptr<Client> client = Task::getClient();
	SOCKET socket;
	if (client)
		socket = client->getSocket();
	else
		throw Errors::CLIENTNOTFOUND;

	// prepare reqId
	uint32_t reqId = Task::getReqId();
	std::cout << "proccessing " << std::to_string(reqId) << std::endl;

	// check if user asked for snaphot of historical data
	if (snapshot)
	{
		// load price data from api, if nothig was thrown in means the fetch is ok,
		// errors at execution are handled by the worker
		Stock_helper::getInstance().fetchLiveData(symbol, static_cast<int>(interval));

		// read data from database

		std::cout << "finished api call" << std::endl;
		// get db path
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(NULL, path, MAX_PATH);
		std::filesystem::path exePath(path);
		std::filesystem::path exeDir = exePath.parent_path();								// This is build/Debug/
		std::filesystem::path dbPath = exeDir.parent_path() / "database" / "stock_data.db"; // Move up one level from Debug to build, then into database
		std::string db_path = dbPath.string();

		std::cout << "db path: " << db_path << std::endl;

		// connect to db
		sqlite3 *db;

		int rc = sqlite3_open(db_path.c_str(), &db);
		if (rc != SQLITE_OK)
		{
			throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
		}
		// This allows your Python thread to WRITE while your C++ thread READS
		sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
		std::cout << "conected to db" << std::endl;
		const char *sql = R"(
        SELECT date, open, high, low, close, volume
        FROM price_history
        WHERE ticker = ? AND interval = ?
		AND ( ? = 0 OR date >= ? )
		AND ( ? = 0 OR date <= ? )
		ORDER BY date ASC)";

		// prepare sql query
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
		{
			sqlite3_close(db);
			throw std::runtime_error("Failed to prepare statement");
		}

		String interval;
		switch (this->interval)
		{
		case Interval::MIN_1:
			interval = "1m";
			break;
		case Interval::MIN_5:
			interval = "5m";
			break;
		case Interval::MIN_15:
			interval = "15m";
			break;
		case Interval::HOUR_1:
			interval = "1h";
			break;
		case Interval::DAY_1:
			interval = "1d";
			break;
		default:
			throw Errors::INVALIDREQUEST;
		}

		sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 2, interval.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 3, start_ts);
		sqlite3_bind_int64(stmt, 4, start_ts);
		sqlite3_bind_int64(stmt, 5, end_ts);
		sqlite3_bind_int64(stmt, 6, end_ts);

		std::vector<PriceData> data;
		data.reserve(8000); // preallocate to increase efficiency

		try
		{
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				int64_t date = sqlite3_column_int64(stmt, 0);
				double open = sqlite3_column_double(stmt, 1);
				double high = sqlite3_column_double(stmt, 2);
				double low = sqlite3_column_double(stmt, 3);
				double close = sqlite3_column_double(stmt, 4);
				int64_t volume = sqlite3_column_int64(stmt, 5);
				data.push_back(PriceData{static_cast<uint64_t>(date), open, close, low, high, static_cast<uint64_t>(volume)});
			}
		}
		catch (...)
		{
			sqlite3_finalize(stmt);
			sqlite3_close(db);
			throw std::runtime_error("failed to read from db");
		}
		sqlite3_finalize(stmt);
		sqlite3_close(db);

		int count = 0;
		// send messages to the client
		while (count < data.size())
		{
			String content;
			uint16_t candle_count = static_cast<uint16_t>(std::min<size_t>(256, data.size() - count));
			std::cout << candle_count << std::endl;
			content.reserve(8 + candle_count * 48); // preallocate the string 8 is number of header bytes and candles is the number of candles (out of 255 max per message) times 48 bytes per candle
			content.push_back(static_cast<char>(MsgType::SNAPSHOT));
			uint32_t network_reqId = htonl(reqId);
			content.append(reinterpret_cast<char *>(&network_reqId), 4);
			content.push_back((data.size() - count) <= 255 ? 1 : 0);
			uint16_t network_count = htons(candle_count); // Convert to Network Order
			content.append(reinterpret_cast<char *>(&network_count), 2);
			for (uint16_t i = 0; i < candle_count; ++i)
			{
				String row = data[count].messageFormat();
				content.append(row.data(), row.size());
				++count;
			}
			Server *server = Task::getServer();
			server->addMessage(Message{content, socket});
		}
	}

	// check if user wants to sign up for streaming
	if (stream)
	{
		// sign him up
		Streamer::getInstance().registerTicket(symbol, Ticket{socket, reqId});
	}

	// this execute sends the messages by itself
	return std::nullopt;
}

Gut::PriceData::PriceData(uint64_t date, double open, double close, double low,
						  double high, uint64_t volume) : date(date), open(open), close(close), low(low), high(high), volume(volume) {}

Gut::String Gut::PriceData::messageFormat()
{
	String content;
	content.reserve(48);
	append_bytes(content, htonll(date));
	append_double(content, open);
	append_double(content, high);
	append_double(content, low);
	append_double(content, close);
	append_bytes(content, htonll(volume));
	return content;
}
