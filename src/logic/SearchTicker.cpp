#include "SearchTicker.hpp"

namespace Gut
{
	SearchTicker::SearchTicker(std::shared_ptr<Client>& client, uint32_t reqId, std::string content)
		: Task(client, reqId) {
			std::cout << "started searching" << std::endl;
			uint8_t querySize = content[0];
			if (content.size() < querySize + 1) {
				throw std::runtime_error("Invalid content for SearchTicker task, query size is larger than content size");
			}
			query = content.substr(1, querySize);
			std::cout << query << std::endl;
			try{
				content.erase(0, querySize+1);
			} catch (...) {
				throw std::runtime_error("Invalid content for SearchTicker task, query size is larger than content size");
			}
			if(content.empty()) {
				throw std::runtime_error("Invalid content for SearchTicker task, missing pagination info.");
			}
			uint8_t isFastMode;
			memcpy(&isFastMode, content.data(), 1);
			content.erase(0, 1);
			if(isFastMode == 0) {
				if(content.size() < 5) {
					throw std::runtime_error("Invalid content for SearchTicker task, missing pagination info");
				}
				memcpy(&pageSize, content.data(), 1);
				content.erase(0, 1);
				memcpy(&lastTickerId, content.data(), 4);
				lastTickerId = ntohl(lastTickerId);
			}
			else if(isFastMode != 1) {
				throw std::runtime_error("Invalid content for SearchTicker task, invalid fast mode flag");
			}
			std::cout << "SearchTicker task created with query: " << query << " pageSize: " << static_cast<int>(pageSize) << " lastTickerId: " << lastTickerId << std::endl;
		}

	std::optional<Message> SearchTicker::execute() {
		if(Task::getClient() == nullptr) {
			throw std::runtime_error("Client not found for SearchTicker task");
		}
		//call the database helper to search for your ticker
		TickerListDBHelper dbHelper;
		const std::unique_ptr<std::vector<TickerInfo>> tickerList = dbHelper.getTickerList(query, pageSize, lastTickerId);
		//create a message with the ticker list
		String msgContent;
		msgContent.reserve(6);
		msgContent.push_back(static_cast<char>(MsgType::SEARCHTICKER));
		uint32_t netReqId = htonl(Task::getReqId());
		msgContent.append(reinterpret_cast<char *>(&netReqId), 4);
		msgContent.push_back(static_cast<char>(tickerList->size()));
		for(int i=0; i < tickerList->size(); i++){
			TickerInfo& ticker = tickerList->at(i);
			msgContent.push_back(static_cast<char>(ticker.name.size()));
			msgContent.append(ticker.name.data());
			msgContent.push_back(static_cast<char>(ticker.symbol.size()));
			msgContent.append(ticker.symbol.data());
			msgContent.append(reinterpret_cast<const char *>(&ticker.id), 4);
		}
		return std::make_optional(Message{std::move(msgContent), Task::getClient()->getSocket()});
	}
}