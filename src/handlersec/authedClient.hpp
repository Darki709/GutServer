#ifndef AUTHED_CLIENT
#define AUTHED_CLIENT

#include "../libraries.hpp"
#include "client.hpp"

namespace Gut{
	class AuthedClient : Client{
		private:
			UsrID usrId;
			String usrName;
			EncryptKey key;
		public:
			AuthedClient(UsrID id, Client client);
			~AuthedClient();
	};
}

#endif