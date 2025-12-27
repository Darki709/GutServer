#ifndef HANDSHAKEHELLO_HPP
#define HANDSHAKEHELLO_HPP

#include "../libraries.hpp"
#include "task.hpp"

namespace Gut{
	class HandShakeHello : public Task{
		private:
			//task inputs
			String clientPublicKey;
		public:
			HandShakeHello(Client& client, String clientPublicKey);
			Message&& execute() override;
	};
}

#endif

