#include "chart_state_db_helper.hpp"

namespace Gut
{
	Chart_State_DB::Chart_State_DB()
	{
		// Table_helper() (base) already opened the shared DB connection.
		exec(chart_state_table_layout);
	}

	int Chart_State_DB::get_all(uint32_t user_id, std::vector<ChartStateRow> &out)
	{
		try
		{
			SecureStmt stmt(db,
				"SELECT kind, item_key, payload, updated_at, deleted "
				"FROM chart_state WHERE user_id = ?;");
			sqlite3_bind_int(stmt.stmt, 1, user_id);

			int count = 0;
			while (sqlite3_step(stmt.stmt) == SQLITE_ROW)
			{
				ChartStateRow r;
				r.kind = reinterpret_cast<const char *>(sqlite3_column_text(stmt.stmt, 0));
				r.key  = reinterpret_cast<const char *>(sqlite3_column_text(stmt.stmt, 1));
				const unsigned char *pay = sqlite3_column_text(stmt.stmt, 2);
				r.payload    = pay ? reinterpret_cast<const char *>(pay) : "";
				r.updated_at = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 3));
				r.deleted    = sqlite3_column_int(stmt.stmt, 4) != 0;
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

	int Chart_State_DB::upsert_lww(uint32_t user_id, const String &kind, const String &key,
								   const String &payload, uint64_t updated_at, bool deleted)
	{
		try
		{
			TransactionGuard guard(db);
			// On conflict, only overwrite when the incoming copy is newer-or-equal.
			// A stale incoming row hits the WHERE guard and is silently ignored (still SQLITE_DONE).
			SecureStmt stmt(db,
				"INSERT INTO chart_state (user_id, kind, item_key, payload, updated_at, deleted) "
				"VALUES (?, ?, ?, ?, ?, ?) "
				"ON CONFLICT(user_id, kind, item_key) DO UPDATE SET "
				"  payload    = excluded.payload, "
				"  updated_at = excluded.updated_at, "
				"  deleted    = excluded.deleted "
				"WHERE excluded.updated_at >= chart_state.updated_at;");

			sqlite3_bind_int(stmt.stmt, 1, user_id);
			sqlite3_bind_text(stmt.stmt, 2, kind.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt.stmt, 3, key.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt.stmt, 4, payload.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt.stmt, 5, static_cast<sqlite3_int64>(updated_at));
			sqlite3_bind_int(stmt.stmt, 6, deleted ? 1 : 0);

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
