#include "LoginTask.hpp"

Gut::LoginTask::LoginTask(std::shared_ptr<Client> &client, uint32_t reqId, String content) : Task(client, reqId)
{
	if (static_cast<int>(Task::getClient()->getState()) < 2)
		throw Errors::ILLEGALACCESS;
	if (content.length() > 512)
		throw Errors::INVALIDREQUEST;

	// 1. Extract Username
	if (content.empty())
		throw Errors::INVALIDREQUEST;
	uint8_t userLen = static_cast<uint8_t>(content[0]);
	content.erase(0, 1);

	if (content.length() < userLen)
		throw Errors::INVALIDREQUEST;
	username.assign(content.data(), userLen);
	content.erase(0, userLen);

	// 2. Extract Password
	if (content.empty())
		throw Errors::INVALIDREQUEST;
	uint8_t passLen = static_cast<uint8_t>(content[0]);
	content.erase(0, 1);

	if (content.length() < passLen)
		throw Errors::INVALIDREQUEST;
	password.assign(content.data(), passLen);

	std::cout << "Loging user " << username << " in" << std::endl;
}

std::optional<Gut::Message> Gut::LoginTask::execute()
{
	std::shared_ptr<Client> client;
	if ((client = Task::getClient()) == nullptr)
		throw Errors::CLIENTNOTFOUND;
	User_table helper;
	UsrID usrId = helper.authenticateUser(username, password);
	String content;
	content.push_back(static_cast<char>(MsgType::LOGIN));
	if (usrId == -1)
	{
		content.push_back(static_cast<char>(LoginFlags::WRONGPASSWORD));
		std::cout << "failed login attempt for " << username << std::endl;
	}
	else if (usrId == -2)
	{
		content.push_back(static_cast<char>(LoginFlags::USERDOESNTEXIST));
		std::cout << "user doesn't exist " << username << std::endl;
	}
	else
	{
		content.push_back(static_cast<char>(LoginFlags::SUCCESS));
		client->setAuthenticated(username, usrId);
		std::cout << "user is logged in " << username << "with user id " << std::to_string(usrId) << std::endl;
	}
	return Message{content, client->getSocket()};
}