#include "list_db_helper.hpp"
#include "ticker_list_db_helper.hpp"

namespace Gut {

// --- Public API ---

int List_DB::create_new_list(uint32_t user_id, String list_name) {
    // Check if user exists (as per your return logic)
    // Note: This assumes a users table exists as defined in your User_table turn
    try {
        TransactionGuard guard(db);
        
        // 1. Check if user exists
        SecureStmt userCheck(db, "SELECT id FROM users WHERE id = ?;");
        sqlite3_bind_int(userCheck.stmt, 1, user_id);
        if (sqlite3_step(userCheck.stmt) != SQLITE_ROW) return 2;

        // 2. Insert new list
        SecureStmt stmt(db, "INSERT INTO lists (list_name, user_id) VALUES (?, ?);");
        sqlite3_bind_text(stmt.stmt, 1, list_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.stmt, 2, user_id);

        if (sqlite3_step(stmt.stmt) == SQLITE_DONE) {
            guard.commit();
            return 0;
        }
    } catch (...) {
        return 1; // Likely UNIQUE constraint violation (list already exists for this user)
    }
    return 1;
}

int List_DB::insert_new_ticekr(String ticker, String list_name) {
    // Note: Your header didn't include user_id here, so we find the list by name.
    // In a multi-user system, you'd usually want user_id here too to avoid ambiguity.
    
    // 1. Check if ticker exists in global database
    try {
		TickerListDBHelper tickers;
        tickers.getTickerInfo(ticker); 
    } catch (const std::invalid_argument&) {
        return 3; // Ticker doesn't exist
    }

    try {
        TransactionGuard guard(db);

        // 2. Find list ID by name
        SecureStmt listStmt(db, "SELECT id FROM lists WHERE list_name = ?;");
        sqlite3_bind_text(listStmt.stmt, 1, list_name.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(listStmt.stmt) != SQLITE_ROW) return 1; // List not found
        int list_id = sqlite3_column_int(listStmt.stmt, 0);

        // 3. Insert into items
        SecureStmt itemStmt(db, "INSERT INTO list_items (ticker, list_id) VALUES (?, ?);");
        sqlite3_bind_text(itemStmt.stmt, 1, ticker.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(itemStmt.stmt, 2, list_id);

        int rc = sqlite3_step(itemStmt.stmt);
        if (rc == SQLITE_DONE) {
            guard.commit();
            return 0;
        } else if (rc == SQLITE_CONSTRAINT) {
            return 2; // Ticker already in list
        }
    } catch (...) {
        return 1;
    }
    return 1;
}

int List_DB::get_all_lists(uint32_t user_id, std::vector<ListInfo> &container) {
    SecureStmt stmt(db, "SELECT id, list_name FROM lists WHERE user_id = ?;");
    sqlite3_bind_int(stmt.stmt, 1, user_id);

    int count = 0;
    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        uint32_t id = static_cast<uint32_t>(sqlite3_column_int(stmt.stmt, 0));
        String name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 1));
        container.push_back({id, name});
        count++;
    }
    return (sqlite3_errcode(db) == SQLITE_OK || sqlite3_errcode(db) == SQLITE_DONE) ? count : -1;
}

int List_DB::get_tickers_in_list(uint32_t user_id, String list_name, std::vector<String> &container, uint32_t offset, uint32_t limit) {
    SecureStmt stmt(db, 
        "SELECT i.ticker FROM list_items i "
        "JOIN lists l ON i.list_id = l.id "
        "WHERE l.user_id = ? AND l.list_name = ? LIMIT ? OFFSET ?;");
    
    sqlite3_bind_int(stmt.stmt, 1, user_id);
    sqlite3_bind_text(stmt.stmt, 2, list_name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt.stmt, 3, limit);
	sqlite3_bind_int(stmt.stmt, 4, offset);


    int count = 0;
    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        container.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 0)));
        count++;
    }
    
    // Check if list actually existed even if it was empty
    if (count == 0 && get_ticker_count_in_list(user_id, list_name) == -1) return -1;
    
    return count;
}

int List_DB::delete_ticker_from_list(uint32_t user_id, String ticker, String list_name) {
    TransactionGuard guard(db);
    
    SecureStmt stmt(db, 
        "DELETE FROM list_items WHERE ticker = ? AND list_id = ("
        "SELECT id FROM lists WHERE user_id = ? AND list_name = ?);");
    
    sqlite3_bind_text(stmt.stmt, 1, ticker.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.stmt, 2, user_id);
    sqlite3_bind_text(stmt.stmt, 3, list_name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.stmt) == SQLITE_DONE) {
        if (sqlite3_changes(db) > 0) {
            guard.commit();
            return 0;
        }
        // Logic check: was it the ticker missing or the list?
        if (get_ticker_count_in_list(user_id, list_name) == -1) return 2;
        return 1;
    }
    return 1;
}

int List_DB::delete_list(uint32_t user_id, String list_name) {
    TransactionGuard guard(db);
    SecureStmt stmt(db, "DELETE FROM lists WHERE user_id = ? AND list_name = ?;");
    sqlite3_bind_int(stmt.stmt, 1, user_id);
    sqlite3_bind_text(stmt.stmt, 2, list_name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.stmt) == SQLITE_DONE && sqlite3_changes(db) > 0) {
        guard.commit();
        return 0;
    }
    return 1;
}

int List_DB::rename_list(uint32_t user_id, String old_name, String new_name) {
    TransactionGuard guard(db);
    SecureStmt stmt(db, "UPDATE lists SET list_name = ? WHERE user_id = ? AND list_name = ?;");
    
    sqlite3_bind_text(stmt.stmt, 1, new_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.stmt, 2, user_id);
    sqlite3_bind_text(stmt.stmt, 3, old_name.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        if (sqlite3_changes(db) > 0) {
            guard.commit();
            return 0;
        }
        return 2; // Original list not found
    }
    return 1; // New name already exists (Unique constraint)
}

// --- Private Helpers ---

int List_DB::get_user_list_count(uint32_t user_id) {
    SecureStmt stmt(db, "SELECT COUNT(*) FROM lists WHERE user_id = ?;");
    sqlite3_bind_int(stmt.stmt, 1, user_id);
    return (sqlite3_step(stmt.stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt.stmt, 0) : 0;
}

int List_DB::get_ticker_count_in_list(uint32_t user_id, String list_name) {
    SecureStmt stmt(db, 
        "SELECT COUNT(*) FROM list_items WHERE list_id = ("
        "SELECT id FROM lists WHERE user_id = ? AND list_name = ?);");
    sqlite3_bind_int(stmt.stmt, 1, user_id);
    sqlite3_bind_text(stmt.stmt, 2, list_name.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt.stmt) == SQLITE_ROW) return sqlite3_column_int(stmt.stmt, 0);
    return -1; // List doesn't exist
}

}