#pragma once

#include "../libraries.hpp"
#ifdef _DEBUG
  #undef _DEBUG
  #include <Python.h>
  #define _DEBUG
#else
  #include <Python.h>
#endif

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

	class Stock_helper
	{
	public:
		static Stock_helper& getInstance();
		void fetchLiveData(String& ticker, uint32_t interval);
	private:
		Stock_helper();
		~Stock_helper();
		std::mutex fetchMutex;
	};
}