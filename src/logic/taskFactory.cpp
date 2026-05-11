#include "HandshakeHello.hpp"
#include "HandshakeVerify.hpp"
#include "taskFactory.hpp"
#include "RequestTickerData.hpp"
#include "CancelTickerStream.hpp"
#include "registerTask.hpp"
#include "LoginTask.hpp"
#include "SearchTicker.hpp"
#include "GetTickerInfo.hpp"
#include "../runtime/client.hpp"
#include "GetBalance.hpp"
#include "SendOrder.hpp"
#include "fetchorders.hpp"
#include "endOrder.hpp"
#include "FetchWatchlistsTask.hpp"
#include "ModifyWatchlistItemsTask.hpp"
#include "ManageWatchlistTask.hpp"
#include "GetWatchlistContentTask.hpp"


std::unique_ptr<Gut::Task> Gut::TaskFactory::createTask(Message message, std::shared_ptr<Client> &client)
{
	String &content = message.getContent();

	// extract task type
	uint8_t taskType;
	memcpy(&taskType, content.data(), 1);
	content.erase(0, 1);

	// extract request id
	uint32_t reqId;
	memcpy(&reqId, content.data(), 4);
	reqId = ntohl(reqId);
	std::cout << "request id" << std::to_string(reqId) << std::endl;
	content.erase(0, 4);

	std::cout << static_cast<int>(taskType) << std::endl;
	try
	{
		// switch on task type, content sent to tasks is without task type and request id
		switch (static_cast<int>(taskType))
		{
		case static_cast<int>(TaskType::HANDSHAKEHELLO):
			return std::make_unique<HandShakeHello>(client, reqId, content);
		case static_cast<int>(TaskType::HANDSHAKEVERIFY):
			return std::make_unique<HandShakeVerify>(client, reqId, content);
		case static_cast<int>(TaskType::REQUESTTICKERDATA):
			return std::make_unique<RequestTickerData>(client, reqId, content);
		case static_cast<int>(TaskType::CANCELTICKERSTREAM):
			return std::make_unique<CancelTickerStream>(client, reqId, content);
		case static_cast<int>(TaskType::REGISTER):
			return std::make_unique<RegisterTask>(client, reqId, content);
		case static_cast<int>(TaskType::LOGIN):
			return std::make_unique<LoginTask>(client, reqId, content);
		case static_cast<int>(TaskType::SEARCHTICKER):
			return std::make_unique<SearchTicker>(client, reqId, content);
		case static_cast<int>(TaskType::TICKERINFO):
			return std::make_unique<GetTickerInfo>(client, reqId, content);
		case static_cast<int>(TaskType::GETBALANCE):
			return std::make_unique<GetBalance>(client, reqId);
		case static_cast<int>(TaskType::SENDORDER):
			return std::make_unique<SendOrder>(client, reqId, content);
		case static_cast<int>(TaskType::FETCHORDERS):
			return std::make_unique<FetchOrdersTask>(client, reqId, content);
		case static_cast<int>(TaskType::ENDORDER):
			return std::make_unique<EndOrder>(client, reqId, content);
		case static_cast<int>(TaskType::FETCH_WATCHLISTS):
			return std::make_unique<FetchWatchlistsTask>(client, reqId);
		case static_cast<int>(TaskType::MANAGE_WATCHLIST):
			return std::make_unique<ManageWatchlistTask>(client, reqId, content);
		case static_cast<int>(TaskType::MODIFY_WATCHLIST_ITEMS):
			return std::make_unique<ModifyWatchlistItemsTask>(client, reqId, content);
		case static_cast<int>(TaskType::GET_WATCHLIST_CONTENT):
			return std::make_unique<GetWatchlistContentTask>(client, reqId, content);
		default:
			return nullptr;
		}
	}
	catch (Errors err)
	{
		// add specialized error handling later
		if (err == Errors::ILLEGALACCESS)
		{
			std::cout << "illegal access from " << std::to_string(client->getSocket()) << std::endl;
			return nullptr;
		}
		if (err == Errors::INVALIDREQUEST)
		{
			std::cout << "invalid message from " << std::to_string(client->getSocket()) << std::endl;
			return nullptr;
		}
	}
	return nullptr;
}
