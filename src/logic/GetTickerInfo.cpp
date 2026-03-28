#include "GetTickerInfo.hpp"

namespace Gut
{
	GetTickerInfo::GetTickerInfo(std::shared_ptr<Client> &client, uint32_t reqId, String symbol) : Task(client, reqId)
	{
		this->symbol = symbol;
		std::cout << "started get ticker info for: " << symbol << std::endl;
	}
	
	std::optional<Message> GetTickerInfo::execute()
	{
		TickerListDBHelper helper;
		String content;
		uint32_t n_reqId = htonl(Task::getReqId());
		content.push_back(static_cast<uint8_t>(TaskType::TICKERINFO));
		content.append(reinterpret_cast<char *>(n_reqId), 4);
		TickerInformation info;
		try
		{
			info = helper.getTickerInfo(symbol);
		}
		catch(std::invalid_argument err){
			std::cout << "error: " << err.what() << std::endl;
			content.push_back(0);
			return Message{content, Task::getClient()->getSocket()};
		}
		content.push_back(info.name.size());
		content.append(info.name, info.name.size());
		content.push_back(info.exchange.size());
		content.append(info.exchange, info.exchange.size());
		content.push_back(static_cast<uint8_t>(info.type));
		content.push_back(info.sector.size());
		content.append(info.sector, info.sector.size());
		return Message(content, Task::getClient()->getSocket());
	}
}