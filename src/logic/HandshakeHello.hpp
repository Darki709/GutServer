#ifndef HANDSHAKEHELLO_HPP
#define HANDSHAKEHELLO_HPP

#include "../libraries.hpp"
#include "task.hpp"

namespace Gut{
	//responsible to recieve the public key of the client, generate a random sessionkey for
	//encryption encrypts the sessionkey using the client's public key and sends it back to the client
	class HandShakeHello : public Task{
		private:
			//task inputs
			String clientPublicKey;
		public:
			HandShakeHello(std::shared_ptr<Client>& client, String clientPublicKey);
			std::optional<Message> execute() override;
	};
}

#endif

