#pragma once

#include "../external/sqlite3.h"
#include "../libraries.hpp"
#include <openssl/rand.h>

#define SALT_SIZE 16

//defined in server for simplicity of transporting the source code move if you ever actually deploy


namespace Gut{
	class User_table{
		private:
			const String PEPPER = "pepper";
			sqlite3* db;

			bool isSecurePassword(const String& password, String& feedback);
		public:
			User_table();
			~User_table();
			int addUser(String username, String password); //0 for success, -1 for user exists , throws back the feedback if password is insecure
			int authenticateUser(String username, String password);//return usrid for success, -1 for wrong password, -2 for failure
	};

}