#pragma once
#include "../external/sqlite3.h"
#include "../libraries.hpp"

namespace Gut
{
	struct SecureStmt
	{
		sqlite3_stmt *stmt;
		~SecureStmt();
	};

	class Table_helper
	{
	protected:
		sqlite3 *db;
		Table_helper();
		Table_helper(sqlite3 *db);
		~Table_helper();
	public:
		sqlite3* getHandle();
	};

	class TransactionGuard
	{
		sqlite3 *db;
		bool committed = false;

	public:
		TransactionGuard(sqlite3 *db);
		~TransactionGuard();
		void commit();
	};
}
