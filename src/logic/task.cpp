#include "task.hpp"

Gut::Task::Task(std::shared_ptr<Client>& client, uint64_t reqid):client(std::move(client))
 , reqId(reqId){
	std::cout << "task created" << std::endl;
}

std::shared_ptr<Gut::Client> Gut::Task::getClient() {
    return client.lock(); // Returns a shared_ptr, or nullptr if Client is gone
}

uint32_t Gut::Task::getReqId(){
	return reqId;
}

Gut::Server* Gut::Task::getServer(){
	return server;
}

void Gut::Task::setServer(Server* server){
	this->server = server;
}
