#include "db_utilities.hpp"

namespace Gut{
	SecureStmt::~SecureStmt(){
		if(stmt){
			sqlite3_finalize(stmt);
		}
	}

	Table_helper::~Table_helper(){
		if(db) sqlite3_close(db);
	}

	Table_helper::Table_helper(sqlite3 *db){
		this->db = db;
	}

	sqlite3* Table_helper::getHandle(){
		return this->db;
	}

	Table_helper::Table_helper(){
		// get db connection
		//  get the db path
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(NULL, path, MAX_PATH);
		std::filesystem::path exePath(path);
		std::filesystem::path exeDir = exePath.parent_path();				  // This is build/Debug/
		std::filesystem::path dbPath = exeDir / "database" / "stock_data.db"; // db path
		std::string db_path = dbPath.string();

		// connect to db
		int rc = sqlite3_open(db_path.c_str(), &db);
		if (rc != SQLITE_OK)
		{
			throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
		}
		sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
	}

	TransactionGuard::TransactionGuard(sqlite3* db) : db(db) {
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    }
    TransactionGuard::~TransactionGuard() {
        if (!committed) sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    void TransactionGuard::commit() {
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK) {
            committed = true;
        }
    }
}