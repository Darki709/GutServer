#include "task.hpp"

Gut::Task::Task(std::shared_ptr<Client>& client):client(std::move(client)){
	std::cout << "task created" << std::endl;
}

std::shared_ptr<Gut::Client> Gut::Task::getClient() {
    return client.lock(); // Returns a shared_ptr, or nullptr if Client is gone
}

