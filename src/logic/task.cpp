#include "task.hpp"

Gut::Task::Task(Client& client):client(client.weak_from_this()){}

std::shared_ptr<Gut::Client> Gut::Task::getClient() {
    return client.lock(); // Returns a shared_ptr, or nullptr if Client is gone
}

