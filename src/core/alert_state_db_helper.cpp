#include "alert_state_db_helper.hpp"

namespace Gut
{
	Alert_State_DB::Alert_State_DB()
	{
		// Table_helper() (base) already opened the shared DB connection.
		exec(alert_state_table_layout);
	}

	int Alert_State_DB::get_all(uint32_t user_id, std::vector<AlertStateRow> &out)
	{
		try
		{
			SecureStmt stmt(db,
				"SELECT uuid, payload, updated_at, deleted "
				"FROM alert_state WHERE user_id = ?;");
			sqlite3_bind_int(stmt.stmt, 1, user_id);

			int count = 0;
			while (sqlite3_step(stmt.stmt) == SQLITE_ROW)
			{
				AlertStateRow r;
				r.uuid = reinterpret_cast<const char *>(sqlite3_column_text(stmt.stmt, 0));
				const unsigned char *pay = sqlite3_column_text(stmt.stmt, 1);
				r.payload    = pay ? reinterpret_cast<const char *>(pay) : "";
				r.updated_at = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 2));
				r.deleted    = sqlite3_column_int(stmt.stmt, 3) != 0;
				out.push_back(std::move(r));
				count++;
			}
			return count;
		}
		catch (...)
		{
			return -1;
		}
	}

	int Alert_State_DB::upsert_lww(uint32_t user_id, const String &uuid, const String &payload,
								   uint64_t updated_at, bool deleted)
	{
		try
		{
			TransactionGuard guard(db);
			// On conflict, only overwrite when the incoming copy is newer-or-equal.
			// A stale incoming row hits the WHERE guard and is silently ignored (still SQLITE_DONE).
			SecureStmt stmt(db,
				"INSERT INTO alert_state (user_id, uuid, payload, updated_at, deleted) "
				"VALUES (?, ?, ?, ?, ?) "
				"ON CONFLICT(user_id, uuid) DO UPDATE SET "
				"  payload    = excluded.payload, "
				"  updated_at = excluded.updated_at, "
				"  deleted    = excluded.deleted "
				"WHERE excluded.updated_at >= alert_state.updated_at;");

			sqlite3_bind_int(stmt.stmt, 1, user_id);
			sqlite3_bind_text(stmt.stmt, 2, uuid.c_str(), -1, SQLITE_TRANSIENT);
			// Bind the payload with an explicit length (not -1/strlen) so a JSON blob that
			// happens to contain an embedded NUL is stored whole rather than truncated. The
			// cast is safe: the receive layer caps a single frame at 32 MB.
			sqlite3_bind_text(stmt.stmt, 3, payload.data(), static_cast<int>(payload.size()), SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt.stmt, 4, static_cast<sqlite3_int64>(updated_at));
			sqlite3_bind_int(stmt.stmt, 5, deleted ? 1 : 0);

			if (sqlite3_step(stmt.stmt) == SQLITE_DONE)
			{
				guard.commit();
				return 0;
			}
			return -1;
		}
		catch (...)
		{
			return -1;
		}
	}
}
