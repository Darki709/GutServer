#include "registerTask.hpp"

Gut::RegisterTask::RegisterTask(std::shared_ptr<Client>& client, uint32_t reqId, String content) : Task(client, reqId){
    if(static_cast<int>(Task::getClient()->getState()) < 2) throw Errors::ILLEGALACCESS;
    if(content.length() > 512) throw Errors::INVALIDREQUEST;

    // 1. Extract Username
    if (content.empty()) throw Errors::INVALIDREQUEST;
    uint8_t userLen = static_cast<uint8_t>(content[0]);
    content.erase(0, 1);

    if (content.length() < userLen) throw Errors::INVALIDREQUEST;
    username.assign(content.data(), userLen); 
    content.erase(0, userLen);

    // 2. Extract Password
    if (content.empty()) throw Errors::INVALIDREQUEST;
    uint8_t passLen = static_cast<uint8_t>(content[0]);
    content.erase(0, 1);

    if (content.length() < passLen) throw Errors::INVALIDREQUEST;
    password.assign(content.data(), passLen);

    std::cout << "Registering user " << username << " with password length " << (int)passLen << std::endl;
}

std::optional<Gut::Message> Gut::RegisterTask::execute(){
	std::shared_ptr<Client> client;
	if((client = Task::getClient()) == nullptr){
		throw CLIENTNOTFOUND;
	}
	User_table helper;
	String content;
	content.push_back(static_cast<char>(MsgType::REGISTER)); //create message
	int res;
	try{
		res = helper.addUser(username, password);
		if(res == 0){
			std::cout << "user " << username << " successfully registered" << std::endl;
			content.push_back(static_cast<char>(RegisterFlag::SUCCESS));
			return Message{content, client->getSocket()};
		}
		std::cout << "user with username " << username << " already exists" << std::endl;
		content.push_back(static_cast<char>(RegisterFlag::USERALREADYEXISTS));
		return Message{content, client->getSocket()};
	}
	catch(String feedback){
		std::cout << "insecure password " << feedback << std::endl;
		content.push_back(static_cast<char>(RegisterFlag::INSECUREPASSWORD));
		content.append(feedback);
		return Message{content, client->getSocket()};
	}
}