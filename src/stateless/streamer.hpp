#pragma once

#include "../libraries.hpp"
#include "../core/message.hpp"
#include "stock_db_helper.hpp"

namespace Gut
{
	class Server;

	struct Ticket
	{
		SOCKET clientSocket;
		uint32_t reqId;
	};

	class Ticker
	{
	private:
		std::vector<Ticket> registeredClients;
	public:
		Ticker() = default;
		void addClient(Ticket client);
		void removeClient(SOCKET socket);
		bool isEmpty();
		void broadcast(StockData data, Gut::Server& server);		
	};

	// class encapsulating a thread responsible for fetching data to stream to clients
	class Streamer
	{
	public:
		// singleton access
		static Streamer &getInstance();
		// registers a client to streaming of a symbol
		void registerTicket(String symbol, Ticket ticket);
		//removes a client from all tickers he was registered to
		void removeClient(SOCKET socket);
		void shutDown();
		static void debugPrintContent(String& content);

	private:
		Streamer();
		~Streamer();
		void run();
		bool isMarketLive(uint64_t dataUnixTime);
		std::unordered_map<String,Ticker> streamingList;
		std::mutex streamingListMutex;
		std::thread thread;
		Server& server;
		std::atomic<bool> running{true};
		std::mutex sleepMutex;               // Mutex for the condition variable
        std::condition_variable m_cv;        // Used for interruptible sleep
	};
}