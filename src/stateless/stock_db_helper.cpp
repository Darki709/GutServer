#include "stock_db_helper.hpp"

Gut::Stock_helper::Stock_helper()
{
	Py_Initialize();
	std::cout << " py initialized " << std::endl;

	//set up paths and import the modules and functions
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("import os");
	PyRun_SimpleString("sys.path.append(os.getcwd())");
	PyRun_SimpleString("sys.path.append(os.path.join(os.getcwd(), 'stockapi'))");
	PyRun_SimpleString(R"(sys.path.append(r'C:\Users\NASASuperPC\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\site-packages'))");
	m_pModule = PyImport_ImportModule("stockapi.stockapi");
    if (m_pModule) {
        // 2. Cache the function pointer once
        m_pFunc = PyObject_GetAttrString(m_pModule, "fetch_live_data");
    }

    if (!m_pFunc || !PyCallable_Check(m_pFunc)) {
        PyErr_Print();
        throw std::runtime_error("Critical: Could not initialize Python stock API");
    }
	
	PyEval_SaveThread();
}

Gut::Stock_helper::~Stock_helper()
{
	Py_Finalize();	
}

void Gut::Stock_helper::fetchLiveData(String &ticker, uint32_t interval)
{
	std::cout << "C++: Entering fetchLiveData" << std::endl;
	std::lock_guard<std::mutex> lock(fetchMutex); // prevent multiple concurrent calls to python script
	std::cout << "C++: Mutex Locked" << std::endl;
	//aquire GIL safely with RAII
	struct GILRelease {
        PyGILState_STATE state;
        GILRelease() { state = PyGILState_Ensure(); }
        ~GILRelease() { PyGILState_Release(state); }
    } gil;
	std::cout << "C++: GIL Acquired" << std::endl;

	PyObject *pArgs = PyTuple_Pack(2, PyUnicode_FromString(ticker.c_str()), PyLong_FromUnsignedLong(interval));
	PyObject *pValue = PyObject_CallObject(m_pFunc, pArgs);
	Py_DECREF(pArgs);
	std::cout << "C++: Python Call Returned" << std::endl;
	if (!pValue)
	{
		Py_XDECREF(m_pFunc);
		Py_XDECREF(m_pModule);
		throw std::runtime_error("Python fetch_live_data call failed");
	}
	
	int rowsFetched = (int)PyLong_AsLong(pValue);
	Py_XDECREF(pValue);
	std::cout << rowsFetched << std::endl;

	if (rowsFetched < -1)
	{
		throw std::runtime_error("Failed to fetch live data from Python " + std::to_string(rowsFetched)); //-1 for api error -2 for db error
	}	
}

Gut::Stock_helper &Gut::Stock_helper::getInstance()
{
	static Stock_helper instance;
	return instance;
}
