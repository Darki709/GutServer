#include "GetBalance.hpp"

namespace Gut{
	GetBalance::GetBalance(std::shared_ptr<Client>& client, uint32_t reqId) : Task(client, reqId) {
		std::cout << Task::getClient()->getCredentials().username << " is checking his balance" << std::endl;
	}
	std::optional<Message> GetBalance::execute(){
		User_table helper;
		double balance = helper.getBalance(Task::getClient()->getCredentials().userId);
		String content;
		content.push_back(static_cast<uint8_t>(MsgType::GETBALANCE));
		uint32_t reqid = Task::getReqId();
		uint32_t n_reqId = htonl(reqid);
		content.append(reinterpret_cast<char *>(&n_reqId));
		append_8bytes_num(content, balance);
		return Message{content ,Task::getClient()->getSocket()};
	}
}