#pragma once 
#include "../external/sqlite3.h"
#include "../libraries.hpp"

namespace Gut
{
	struct SecureStmt{
		sqlite3_stmt *stmt;
		~SecureStmt();
	};

	class Table_helper{
		protected:
			sqlite3* db;
			Table_helper();
			~Table_helper();
	};
}
