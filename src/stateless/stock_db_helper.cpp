#include "stock_db_helper.hpp"

Gut::Stock_helper::Stock_helper()
{
	Py_Initialize();
}

Gut::Stock_helper::~Stock_helper()
{
	Py_Finalize();
}

void Gut::Stock_helper::fetchLiveData(String &ticker, uint32_t interval)
{
	std::lock_guard<std::mutex> lock(fetchMutex); // prevent multiple concurrent calls to python script

	PyGILState_STATE gstate = PyGILState_Ensure(); // aquire GIL

	PyObject *pModule = PyImport_ImportModule("stockapi");
	if (!pModule)
	{
		PyGILState_Release(gstate);
		throw std::runtime_error("Failed to import stockapi.py");
	}

	PyObject *pFunc = PyObject_GetAttrString(pModule, "fetch_live_data");
	if (!pFunc || !PyCallable_Check(pFunc))
	{
		Py_DECREF(pModule);
		PyGILState_Release(gstate);
		throw std::runtime_error("fetch_live_data not found");
	}

	PyObject *pArgs = PyTuple_Pack(2, PyUnicode_FromString(ticker.c_str()), PyLong_FromUnsignedLong(interval));
	PyObject *pValue = PyObject_CallObject(pFunc, pArgs);
	Py_DECREF(pArgs);

	if (!pValue)
	{
		Py_DECREF(pFunc);
		Py_DECREF(pModule);
		PyGILState_Release(gstate);
		throw std::runtime_error("Python fetch_live_data call failed");
	}

	int rowsFetched = (int)PyLong_AsLong(pValue);
	Py_DECREF(pValue);
	Py_DECREF(pFunc);
	Py_DECREF(pModule);

	PyGILState_Release(gstate);

	if (rowsFetched < 0)
	{
		throw std::runtime_error("Failed to fetch live data from Python " + rowsFetched); //-1 for api error -2 for db error
	}
}

Gut::Stock_helper& Gut::Stock_helper::getInstance(){
	static Stock_helper instance;
	return instance;
}
