#pragma once

#include "../external/sqlite3.h"
#include "../libraries.hpp"
#include "db_utilities.hpp"

namespace Gut
{

	struct ListInfo
	{
		uint32_t id;
		String name;
	};

	class List_DB : public Table_helper
	{
	public:
		/**
		 * Default constructors/destructors
		 */
		List_DB() = default;
		~List_DB() = default;

		/**
		 * @brief inserts a ticker to a list
		 * @return 0 for success, 1 list not found, 2 ticker is already in list, 3 ticker doesnt exist
		 */
		int insert_new_ticekr(String ticker, String list);

		/**
		 * @brief fetches a list of all the lists a user have
		 * @returns the number of lists found, -1 db error
		 */
		int get_all_lists(uint32_t user_id, std::vector<ListInfo> &container);

		/**
		 * @brief create a new list for a user
		 * @return 0 for success, 1 if list already exists, 2 if user doesnt exist
		 */
		int create_new_list(uint32_t user_id, String list_name);

		/**
		 * @brief delete a ticker from a list
		 * @return 0 on success, 1 ticker not found, 2 list not found
		 */
		int delete_ticker_from_list(uint32_t user_id, String ticker, String list_name);

		/**
		 * @brief delete a list
		 * @return 0 for success, 1 list not found
		 */
		int delete_list(uint32_t user_id, String list_name);

		/**
		 * @brief fetches all tickers belonging to a specific list
		 * @return number of tickers found, -1 if list doesn't exist/db error
		 */
		int get_tickers_in_list(uint32_t user_id, String list_name, std::vector<String> &container);

		/**
		 * @brief renames an existing list
		 * @return 0 success, 1 new name already exists, 2 original list not found
		 */
		int rename_list(uint32_t user_id, String old_name, String new_name);

	private:
		int get_user_list_count(uint32_t user_id);
		int get_ticker_count_in_list(uint32_t user_id, String list_name);
	};

	inline const char *lists_table_layout = R"(
	CREATE TABLE IF NOT EXISTS lists(
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	list_name TEXT,
	user_id INTEGER,
	UNIQUE(user_id, list_name),
	FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
	);
	)";

	inline const char *items_table_layout = R"(
	CREATE TABLE IF NOT EXISTS list_items(
	ticker TEXT NOT NULL,
	list_id TEST NOT NULL,
	PRIMARY KEY (list_id, ticker),
    FOREIGN KEY(list_id) REFERENCES user_watchlists(id) ON DELETE CASCADE
	);
	)";

}
