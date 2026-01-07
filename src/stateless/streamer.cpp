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
	// Check if the client is already in the list to prevent duplicates
	auto it = std::find_if(registeredClients.begin(), registeredClients.end(),
						   [&client](const Ticket &t)
						   {
							   return t.clientSocket == client.clientSocket;
						   });

	if (it == registeredClients.end())
	{
		registeredClients.push_back(client);
	}
	std::cout << "client " << client.clientSocket << " registered to streaming" << std::endl;
}

void Gut::Ticker::removeClient(SOCKET socket)
{
	// The Erase-Remove Idiom:
	// 1. std::remove_if moves all 'matching' elements to the end of the vector
	// 2. .erase() actually removes them from memory
	registeredClients.erase(
		std::remove_if(registeredClients.begin(), registeredClients.end(),
					   [socket](const Ticket &t)
					   {
						   return t.clientSocket == socket;
					   }),
		registeredClients.end());
}

bool Gut::Ticker::isEmpty()
{
	return registeredClients.empty();
}

// recieve the ts ohlc volume data [uint64_t|double|double|double|double|uint64_t]
void Gut::Ticker::broadcast(StockData data, Gut::Server &server)
{
	std::cout << "framing streaming message" << std::endl;
	// frame candle data
	String candle;
	candle.reserve(48);
	append_bytes(candle, htonll(data.ts));
	append_double(candle, data.open);
	append_double(candle, data.high);
	append_double(candle, data.low);
	append_double(candle, data.close);
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
		Streamer::debugPrintContent(candle);
		content.append(candle.data(), candle.size());
		Streamer::debugPrintContent(content);
		// We pass the socket and the message to your server's outgoing queue
		server.addMessage(Message{content, ticket.clientSocket});
	}
}

void Gut::Streamer::debugPrintContent(String& content) {
    std::cout << "--- Binary Message (Size: " << content.size() << ") ---" << std::endl;
    
    for (size_t i = 0; i < content.size(); ++i) {
        // Convert the byte to an unsigned hex value
        unsigned char byte = static_cast<unsigned char>(content[i]);
        
        // Print as 2-digit hex
        printf("%02X ", byte);

        // Add a vertical bar every 4 bytes for readability
        if ((i + 1) % 4 == 0) std::cout << "| ";
        
        // New line every 16 bytes
        if ((i + 1) % 16 == 0) std::cout << std::endl;
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
				Stock_helper::getInstance().fetchLiveData(symbol, 60);

				// Fetch the row we just saved from SQLite
				// Assuming this returns a struct with date, open, high, low, close, volume
				StockData data = Stock_helper::getInstance().getLastRowFromDB(symbol);

				// Market Status Check
				// Only broadcast if the timestamp is within the last 5 minutes
				std::cout << data.ts << data.open << data.high << data.low << data.close << data.volume << std::endl;
				if (isMarketLive(data.ts))
				{
					std::lock_guard<std::mutex> lock(streamingListMutex);
					if (streamingList.contains(symbol))
					{
						std::cout << "data is valid to broadcast" << std::endl;
						// This calls the binary formatting logic we built
						streamingList[symbol].broadcast(data, server);
					}
				}
				else
				{
					std::cout << "aquired data isn't live data" << std::endl;
				}
			}
			catch (std::exception e)
			{
				std::cerr << "Streamer error for " << symbol << ": " << e.what() << std::endl;
			}
		}

		// Wait for the next minute (interruptible)
		std::unique_lock<std::mutex> lock(sleepMutex);
		m_cv.wait_for(lock, std::chrono::minutes(1), [this]
					  { return !running; });
	}
}

// checks if a unix ts is live or not
bool Gut::Streamer::isMarketLive(uint64_t dataUnixTime)
{
	auto now = std::chrono::system_clock::now();
	uint64_t currentUnixTime = std::chrono::duration_cast<std::chrono::seconds>(
								   now.time_since_epoch())
								   .count();
	std::cout << currentUnixTime << std::endl;
	std::cout << dataUnixTime << std::endl;

	const uint64_t STALE_THRESHOLD = 300; // 5 minutes
	
	if (dataUnixTime > currentUnixTime)
		return true;
	return (currentUnixTime - dataUnixTime) < STALE_THRESHOLD;
}

void Gut::Streamer::shutDown()
{
	running = false;   // 1. Change the condition
	m_cv.notify_all(); // 2. Interrupt the sleep immediately
	thread.join();	   // 3. Wait for the thread to finish its last tasks
}