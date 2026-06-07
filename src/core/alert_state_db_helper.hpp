#pragma once

#include "../external/sqlite3.h"
#include "../libraries.hpp"
#include "db_utilities.hpp"
#include <vector>

namespace Gut
{
	// One synced alert record. The payload is the alert's JSON, opaque to the server —
	// only the client interprets it. Identity is the client-generated uuid.
	struct AlertStateRow
	{
		String   uuid;
		String   payload;     // JSON blob describing the alert (symbol, condition, etc.)
		uint64_t updated_at;  // epoch millis (client clock) — drives last-write-wins
		bool     deleted;     // tombstone
	};

	/**
	 * Alert_State_DB — per-user storage for alert definitions, synced with the Android
	 * client's local SQLite store. Last-write-wins by updated_at, keyed by (user_id, uuid).
	 *
	 * Like the other helpers, the default Table_helper() base opens the shared
	 * database/stock_data.db connection; the constructor ensures the table exists.
	 */
	class Alert_State_DB : public Table_helper
	{
	public:
		Alert_State_DB();
		~Alert_State_DB() = default;

		/** Fetch every alert row for a user. Returns row count, or -1 on error. */
		int get_all(uint32_t user_id, std::vector<AlertStateRow> &out);

		/**
		 * Insert or update a row, but only when the incoming copy is at least as new as the
		 * stored one (last-write-wins). Returns 0 on success, -1 on error.
		 */
		int upsert_lww(uint32_t user_id, const String &uuid, const String &payload,
					   uint64_t updated_at, bool deleted);
	};

	inline const char *alert_state_table_layout = R"(
	CREATE TABLE IF NOT EXISTS alert_state(
		user_id    INTEGER NOT NULL,
		uuid       TEXT NOT NULL,
		payload    TEXT NOT NULL DEFAULT '',
		updated_at INTEGER NOT NULL DEFAULT 0,
		deleted    INTEGER NOT NULL DEFAULT 0,
		PRIMARY KEY(user_id, uuid),
		FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
	);
	)";
}
