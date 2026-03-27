#pragma once

#include "../external/sqlite3.h"
#include "../libraries.hpp"
#include <openssl/rand.h>
#include "db_utilities.hpp"

#define SALT_SIZE 16

//defined in server for simplicity of transporting the source code move if you ever actually deploy


namespace Gut{
	class User_table : protected Table_helper{
		private:
			const String PEPPER = "pepper";
			bool isSecurePassword(const String& password, String& feedback);
		
			public:
			User_table();
			~User_table() = default;
			int addUser(String username, String password); //0 for success, -1 for user exists , throws back the feedback if password is insecure
			uint32_t authenticateUser(String username, String password);//return usrid for success, -1 for wrong password, -2 for user doesn't exist
			double getBalance(uint32_t client_id);
	};

	const char *query = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE, password TEXT NOT NULL, salt TEXT NOT NULL, money REAL NOT NULL)";
}