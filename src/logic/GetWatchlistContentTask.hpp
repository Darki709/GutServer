#pragma once
#include "task.hpp"
#include "../core/list_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut {
    class GetWatchlistContentTask : public Task {
    private:
        String listName;
        uint32_t offset;
        uint32_t limit;
    public:
        GetWatchlistContentTask(std::shared_ptr<Client>& client, uint32_t reqId, String& content);
        std::optional<Message> execute(ThreadResources& resources) override;
    };
}