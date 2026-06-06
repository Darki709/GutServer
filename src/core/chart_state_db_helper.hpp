#pragma once

#include "../external/sqlite3.h"
#include "../libraries.hpp"
#include "db_utilities.hpp"
#include <vector>

namespace Gut
{
	// One synced chart-state record. kind ∈ {drawings, indicators, preset}.
	struct ChartStateRow
	{
		String   kind;
		String   key;
		String   payload;     // JSON blob (drawings array / indicator-snapshot array)
		uint64_t updated_at;  // epoch millis (client clock) — drives last-write-wins
		bool     deleted;     // tombstone
	};

	// Wire kind codes shared by the pull/push tasks (must match the Android client).
	inline uint8_t kind_to_code(const String &kind)
	{
		if (kind == "indicators") return 1;
		if (kind == "preset")     return 2;
		return 0; // drawings
	}
	inline String code_to_kind(uint8_t code)
	{
		switch (code)
		{
		case 1:  return "indicators";
		case 2:  return "preset";
		default: return "drawings";
		}
	}

	/**
	 * Chart_State_DB — per-user storage for drawings/indicators/presets, synced with
	 * the Android client's local SQLite cache. Last-write-wins by updated_at.
	 *
	 * Like the other helpers, the default Table_helper() base opens the shared
	 * database/stock_data.db connection; the constructor ensures the table exists.
	 */
	class Chart_State_DB : public Table_helper
	{
	public:
		Chart_State_DB();
		~Chart_State_DB() = default;

		/** Fetch every row for a user. Returns row count, or -1 on error. */
		int get_all(uint32_t user_id, std::vector<ChartStateRow> &out);

		/**
		 * Insert or update a row, but only when the incoming copy is at least as new
		 * as the stored one (last-write-wins). Returns 0 on success, -1 on error.
		 */
		int upsert_lww(uint32_t user_id, const String &kind, const String &key,
					   const String &payload, uint64_t updated_at, bool deleted);
	};

	inline const char *chart_state_table_layout = R"(
	CREATE TABLE IF NOT EXISTS chart_state(
		user_id    INTEGER NOT NULL,
		kind       TEXT NOT NULL,
		item_key   TEXT NOT NULL,
		payload    TEXT NOT NULL DEFAULT '',
		updated_at INTEGER NOT NULL DEFAULT 0,
		deleted    INTEGER NOT NULL DEFAULT 0,
		PRIMARY KEY(user_id, kind, item_key),
		FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
	);
	)";
}
