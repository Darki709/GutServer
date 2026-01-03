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
		static Stock_helper& getInstance();
		void fetchLiveData(String& ticker, uint32_t interval);
		void streamingLoop();
		StockData getLastRowFromDB(String& symbol);
	private:
		Stock_helper();
		~Stock_helper();
		std::mutex fetchMutex;
		PyObject* m_pModule = nullptr;
    	PyObject* m_pFunc = nullptr;
		sqlite3 *db;
	};
}