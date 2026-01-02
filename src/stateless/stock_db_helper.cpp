#include "stock_db_helper.hpp"

Gut::Stock_helper::Stock_helper()
{
	Py_Initialize();
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("import os");
	PyRun_SimpleString("sys.path.append(os.getcwd())");
	PyRun_SimpleString("sys.path.append(os.path.join(os.getcwd(), 'stockapi'))");
	PyRun_SimpleString(R"(sys.path.append(r'C:\Users\NASASuperPC\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\site-packages'))");
}

Gut::Stock_helper::~Stock_helper()
{
	Py_Finalize();
}

void Gut::Stock_helper::fetchLiveData(String &ticker, uint32_t interval)
{
	std::lock_guard<std::mutex> lock(fetchMutex); // prevent multiple concurrent calls to python script

	PyGILState_STATE gstate = PyGILState_Ensure(); // aquire GIL

	PyObject *pModule = PyImport_ImportModule("stockapi.stockapi");
	if (!pModule)
	{
		PyErr_Print();
		PyGILState_Release(gstate);
		throw std::runtime_error("Failed to import stockapi.py");
	}
	PyRun_SimpleString("import stockapi; print(f'DEBUG: Loading stockapi from {stockapi.__file__}')");

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
	std::cout << rowsFetched << std::endl;
	PyGILState_Release(gstate);

	if (rowsFetched < -1)
	{
		throw std::runtime_error("Failed to fetch live data from Python " + rowsFetched); //-1 for api error -2 for db error
	}
}

Gut::Stock_helper &Gut::Stock_helper::getInstance()
{
	static Stock_helper instance;
	return instance;
}
