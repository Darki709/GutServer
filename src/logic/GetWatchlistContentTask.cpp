#include "GetWatchlistContentTask.hpp"

namespace Gut
{
    GetWatchlistContentTask::GetWatchlistContentTask(std::shared_ptr<Client> &client, uint32_t reqId, String &content)
        : Task(client, reqId)
    {
        // content layout: [NameLen(1B)][Name(Var)][Offset(4B)][Limit(4B)]
        uint8_t len = static_cast<uint8_t>(content[0]);
        listName = content.substr(1, len);
        
        // Pointer arithmetic to find the pagination bytes after the variable name
        const char* pData = content.data() + 1 + len;
        
        // Extract Offset and Limit (Network Byte Order to Host Byte Order)
        uint32_t rawOffset, rawLimit;
        memcpy(&rawOffset, pData, 4);
        memcpy(&rawLimit, pData + 4, 4);
        
        offset = ntohl(rawOffset);
        limit = ntohl(rawLimit);
    }

    std::optional<Message> GetWatchlistContentTask::execute(ThreadResources &resources)
    {
        auto client = getClient();
        SOCKET sock = client->getSocket();

        String payload;
        uint8_t msgType = 17; // WATCHLIST_CONTENT response
        uint32_t rId = htonl(getReqId());
        
        payload.append(reinterpret_cast<char *>(&msgType), 1);
        payload.append(reinterpret_cast<char *>(&rId), 4);

        // Security Check: Ensure authenticated
        if (client->getState() != ClientState::AUTHENTICATED)
        {
            uint8_t status = 5; // Unauthorized
            payload.append(reinterpret_cast<char *>(&status), 1);
            return Message(payload, sock);
        }

        List_DB listDb;
        std::vector<String> symbols;
        
        // Use the updated DB helper with pagination support
        int result = listDb.get_tickers_in_list(
            client->getCredentials().userId, 
            listName, 
            symbols, 
            offset, 
            limit
        );

        // Map DB result to protocol status
        // result is count of symbols, -1 if list doesn't exist
        uint8_t status = (result >= 0) ? 0 : 1;
        uint8_t count = (result >= 0) ? static_cast<uint8_t>(symbols.size()) : 0;

        payload.append(reinterpret_cast<char *>(&status), 1);
        payload.append(reinterpret_cast<char *>(&count), 1);

        if (status == 0)
        {
            for (const auto &sym : symbols)
            {
                uint8_t symLen = static_cast<uint8_t>(sym.length());
                payload.append(reinterpret_cast<char *>(&symLen), 1);
                payload.append(sym);
            }
        }

        return Message(payload, sock);
    }
}