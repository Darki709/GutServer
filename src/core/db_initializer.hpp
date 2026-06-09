#pragma once

#include "../libraries.hpp"

namespace Gut
{
	/**
	 * DB_Initializer — one-shot, idempotent bootstrap for the shared stock_data.db.
	 *
	 * Creates every application table the server owns IF it does not already exist:
	 *   users, orders, lists, list_items, chart_state, alert_state,
	 *   fetch_history, price_history (+ its index).
	 *
	 * It deliberately does NOT create or touch the `tickers` table — that lives in a
	 * separate database file (database/tickers.db) and is populated manually (loaded
	 * with the full ticker universe) before the server is run.
	 *
	 * Each table's DDL is reused from its owning helper header, so there is a single
	 * source of truth for every schema. Safe to call on every startup.
	 */
	class DB_Initializer
	{
	public:
		/**
		 * Create the database directory + stock_data.db and ensure all tables exist.
		 * Runs inside a transaction. Never throws — logs and returns false on failure
		 * so the caller can decide how to proceed.
		 * @return true on success, false if the database could not be initialised.
		 */
		static bool initialize();
	};
}
