#pragma once
#include "task.hpp"
#include "../core/list_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut {
    class ModifyWatchlistItemsTask : public Task {
    private:
        uint8_t action;
        String listName;
        String symbol;
    public:
        ModifyWatchlistItemsTask(std::shared_ptr<Client>& client, uint32_t reqId, String& content);
        std::optional<Message> execute(ThreadResources& resources) override;
    };
}