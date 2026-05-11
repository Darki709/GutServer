#pragma once
#include "task.hpp" // or wherever Task is defined
#include "../core/list_db_helper.hpp"
#include "../libraries.hpp"

namespace Gut {
    class FetchWatchlistsTask : public Task {
    public:
        FetchWatchlistsTask(std::shared_ptr<Client>& client, uint32_t reqId);
        std::optional<Message> execute(ThreadResources& resources) override;
    };
}