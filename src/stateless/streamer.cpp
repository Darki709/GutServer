#include "streamer.hpp"
#include "../program/server.hpp" //allows streamer to add messages to the queue
#include <algorithm>

Gut::Streamer::Streamer() : server(Server::getInstance())
{
	// start the streamer when some creates it for the first time
	thread = std::thread(&Streamer::run, this);
	std::cout << "streamer started" << std::endl;
}

Gut::Streamer &Gut::Streamer::getInstance()
{
	static Streamer instance;
	return instance;
}

Gut::Streamer::~Streamer()
{
	if (thread.joinable())
		thread.join();
}

void Gut::Streamer::registerTicket(String symbol, Ticket ticket)
{
	std::lock_guard<std::mutex> lock(streamingListMutex);

	// check if a list for the ticker already exists and creates a new ticker if required
	streamingList[symbol].addClient(ticket);
	std::cout << "new client added to " << symbol << std::endl;

	try
	{
		// everytime a new client registers to stream we send him the last price data we have for that ticker so he gets an instant update without waiting for the next streaming update

		std::optional<StockData> lastData = price_helper.getLastRow(symbol);
		if (lastData.has_value())
		{
			streamingList[symbol].broadcast(lastData.value(), server);
		}

		std::cout << "Instant price sent to new client for " << symbol << std::endl;
	}
	catch (...)
	{
		std::cerr << "Failed to send instant update for " << symbol << std::endl;
	}
}

void Gut::Streamer::removeClient(SOCKET socket)
{
	std::lock_guard<std::mutex> lock(streamingListMutex);

	// We use an iterator-based loop so we can safely delete empty tickers
	for (auto it = streamingList.begin(); it != streamingList.end();)
	{

		// Remove the client from this specific ticker's vector
		it->second.removeClient(socket);

		// If no one is watching this ticker anymore, remove it from the map
		if (it->second.isEmpty())
		{
			it = streamingList.erase(it); // erase returns the next valid iterator
			std::cout << "Ticker removed from stream (no active clients)." << std::endl;
		}
		else
		{
			++it;
		}
	}
}

void Gut::Ticker::addClient(Ticket client)
{
	registeredClients.push_back(client);
	std::cout << "client " << client.clientSocket << " registered to streaming" << std::endl;
}

void Gut::Ticker::removeClient(SOCKET socket)
{
	// The Erase-Remove Idiom:
	// 1. std::remove_if moves all 'matching' elements to the end of the vector
	// 2. .erase() actually removes them from memory
	std::erase_if(registeredClients, [socket](const Ticket &t)
				  { return t.clientSocket == socket; });
}

bool Gut::Ticker::isEmpty()
{
	return registeredClients.empty();
}

void Gut::Ticker::broadcastToSingleClient(Ticket ticket, StockData data, Gut::Server &server)
{
	String candle;
	candle.reserve(48);
	append_bytes(candle, htonll(data.ts));
	append_8bytes_num(candle, data.open);
	append_8bytes_num(candle, data.high);
	append_8bytes_num(candle, data.low);
	append_8bytes_num(candle, data.close);
	append_bytes(candle, htonll(data.volume));

	String content;
	content.reserve(53);
	content += static_cast<uint8_t>(MsgType::STREAM);
	uint32_t reqId = htonl(ticket.reqId);
	content.append(reinterpret_cast<char *>(&reqId), 4);
	content.append(candle.data(), candle.size());

	server.addMessage(Message{content, ticket.clientSocket});
}

// recieve the ts ohlc volume data [uint64_t|double|double|double|double|uint64_t]
void Gut::Ticker::broadcast(StockData data, Gut::Server &server)
{
	// std::cout << "framing streaming message" << std::endl;
	//  frame candle data
	String candle;
	candle.reserve(48);
	append_bytes(candle, htonll(data.ts));
	append_8bytes_num(candle, data.open);
	append_8bytes_num(candle, data.high);
	append_8bytes_num(candle, data.low);
	append_8bytes_num(candle, data.close);
	append_bytes(candle, htonll(data.volume));
	// pass data to each client
	for (auto &ticket : registeredClients)
	{
		// frame the message payload
		String content;
		content.reserve(53);
		content += static_cast<uint8_t>(MsgType::STREAM);
		uint32_t reqId = htonl(ticket.reqId);
		content.append(reinterpret_cast<char *>(&reqId), 4);
		// Streamer::debugPrintContent(candle);
		content.append(candle.data(), candle.size());
		// Streamer::debugPrintContent(content);
		//  We pass the socket and the message to your server's outgoing queue
		server.addMessage(Message{content, ticket.clientSocket});
	}
}

void Gut::Streamer::debugPrintContent(String &content)
{
	std::cout << "--- Binary Message (Size: " << content.size() << ") ---" << std::endl;

	for (size_t i = 0; i < content.size(); ++i)
	{
		// Convert the byte to an unsigned hex value
		unsigned char byte = static_cast<unsigned char>(content[i]);

		// Print as 2-digit hex
		printf("%02X ", byte);

		// Add a vertical bar every 4 bytes for readability
		if ((i + 1) % 4 == 0)
			std::cout << "| ";

		// New line every 16 bytes
		if ((i + 1) % 16 == 0)
			std::cout << std::endl;
	}
	std::cout << "\n--------------------------------------------" << std::endl;
}

void Gut::Streamer::run()
{
	while (running)
	{
		// Get a snapshot of tickers currently being watched
		std::vector<String> symbols;
		{
			std::lock_guard<std::mutex> lock(streamingListMutex);
			for (auto const &[symbol, tickerObj] : streamingList)
			{
				symbols.push_back(symbol);
			}
		}

		// Process each ticker
		for (String symbol : symbols)
		{
			try
			{
				// Update DB via Python (Stock_helper)
				int state = YFinance_fetcher::fetch_price_data(symbol, Interval::MIN_1);
				if (state != 0)
				{
					std::cerr << "Failed to fetch live data for " << symbol << " with error code: " << state << std::endl;
					continue; // Skip broadcasting if we can't fetch the latest data
				}

				// Fetch the row we just saved from SQLite
				// Assuming this returns a struct with date, open, high, low, close, volume
				std::optional<StockData> maybeData = price_helper.getLastRow(symbol);

				// Market Status Check
				// Only broadcast if the timestamp is within the last 5 minutes
				if (maybeData.has_value())
				{
					std::lock_guard<std::mutex> lock(streamingListMutex);
					if (streamingList.contains(symbol))
					{
						std::cout << "data is valid to broadcast" << std::endl;
						// This calls the binary formatting logic we built
						streamingList[symbol].broadcast(maybeData.value(), server);
					}
				}
			}
			catch (std::exception e)
			{
				std::cerr << "Streamer error for " << symbol << ": " << e.what() << std::endl;
			}
		}

		// Wait for the next minute (interruptible)
		std::unique_lock<std::mutex> lock(sleepMutex);
		m_cv.wait_for(lock, std::chrono::seconds(60), [this]
					  { return !running; });
	}
}

void Gut::Streamer::shutDown()
{
	running = false;   // 1. Change the condition
	m_cv.notify_all(); // 2. Interrupt the sleep immediately
	thread.join();	   // 3. Wait for the thread to finish its last tasks
}

// call only under locked mutex
void Gut::Ticker::removeClient(SOCKET socket, uint32_t reqId)
{
	std::erase_if(registeredClients, [socket, reqId](const Ticket &t)
				  {
					  if (t.clientSocket == socket && t.reqId == reqId)
					  {
						  std::cout << "Removing specific request " << reqId << " for socket " << socket << std::endl;
						  return true; // Match found: delete this one
					  }
					  return false; // Not a match: keep this one
				  });
}

void Gut::Streamer::cancelRequest(String symbol, SOCKET socket, uint32_t reqId)
{
	std::cout << "canceling streaming request" << std::endl;
	std::lock_guard<std::mutex> lock(streamingListMutex);
	auto it = streamingList.find(symbol);
	if (it != streamingList.end())
	{
		it->second.removeClient(socket, reqId); // removes the client
		if (it->second.isEmpty())
		{
			streamingList.erase(it); // erase the ticker because no client is requesting steaming for it
			std::cout << "Ticker removed from stream (no active clients)." << std::endl;
		}
	}
}