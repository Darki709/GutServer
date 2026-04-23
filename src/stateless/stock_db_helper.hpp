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

namespace Gut
{

	enum class Interval : uint32_t
	{
		MIN_1 = 60,
		MIN_5 = 300,
		MIN_15 = 900,
		HOUR_1 = 3600,
		DAY_1 = 86400
	};

	struct StockData{
		uint64_t ts;
		double open;
		double high;
		double low;
		double close;
		uint64_t volume;
	};

	class Stock_helper
	{
	public:
		static void init();
		static void shutdown();
		int fetchLiveData(String& ticker, uint32_t interval); // Returns 0 on success, -1 on Python error, -2 on other errors
		void streamingLoop();
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