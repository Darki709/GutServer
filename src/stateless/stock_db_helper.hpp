#pragma once

#include "../libraries.hpp"
#ifdef _DEBUG
  #undef _DEBUG
  #include <Python.h>
  #define _DEBUG
#else
  #include <Python.h>
#endif
#include "../external/sqlite3.h"
#include "../core/yfinance_fetcher.hpp"
#include "../core/price_data_db_helper.hpp"

namespace Gut
{
	/**
	 * @deprecated
	 * Was used when i used the yfinance python library for price fetching
	 */
	class Stock_helper
	{
	public:
		static void init();
		static void shutdown();
		int fetchLiveData(String& ticker, uint32_t interval); // Returns 0 on success, -1 on Python error, -2 on other errors
		std::optional<StockData> getLastRowFromDB(String& symbol);
		Stock_helper();
		~Stock_helper();
	private:
		// The "Mother" state that workers use to spawn sub-interpreters
        static PyThreadState* s_main_tstate; 
        
        // This worker's specific interpreter state
        PyThreadState* m_tstate;

		PyObject* m_pModule = nullptr;
    	PyObject* m_pFunc = nullptr;
	};
}