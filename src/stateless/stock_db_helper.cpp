#include "stock_db_helper.hpp"

Gut::Stock_helper::Stock_helper()
{
	Py_Initialize();
	std::cout << " py initialized " << std::endl;

	// set up paths and import the modules and functions
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("import os");
	PyRun_SimpleString("sys.path.append(os.getcwd())");
	PyRun_SimpleString("sys.path.append(os.path.join(os.getcwd(), 'stockapi'))");
	PyRun_SimpleString(R"(sys.path.append(r'C:\Users\NASASuperPC\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\site-packages'))");
	m_pModule = PyImport_ImportModule("stockapi.stockapi");
	if (m_pModule)
	{
		// 2. Cache the function pointer once
		m_pFunc = PyObject_GetAttrString(m_pModule, "fetch_live_data");
	}

	if (!m_pFunc || !PyCallable_Check(m_pFunc))
	{
		PyErr_Print();
		throw std::runtime_error("Critical: Could not initialize Python stock API");
	}

	PyEval_SaveThread();

	//get db connection
	// get the db path
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::filesystem::path exePath(path);
	std::filesystem::path exeDir = exePath.parent_path();								// This is build/Debug/
	std::filesystem::path dbPath = exeDir.parent_path() / "database" / "stock_data.db"; // Move up one level from Debug to build, then into database
	std::string db_path = dbPath.string();

	// connect to db
	int rc = sqlite3_open(db_path.c_str(), &db);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
	}
	sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
}

Gut::Stock_helper::~Stock_helper()
{
	Py_XDECREF(m_pFunc);
	Py_XDECREF(m_pModule);
	Py_Finalize();
	//clsoe db connection
	if (db) {
        sqlite3_close(db);
    }
}

void Gut::Stock_helper::fetchLiveData(String &ticker, uint32_t interval)
{
	std::cout << "C++: Entering fetchLiveData" << std::endl;
	// aquire GIL safely with RAII
	struct GILRelease
	{
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

Gut::StockData Gut::Stock_helper::getLastRowFromDB(String &symbol)
{
	std::cout << "fetching streaming data for " << symbol << std::endl;
	StockData data = {0}; // Initialize with zeros

	sqlite3_stmt *stmt;

	// Query to get the latest record based on timestamp (date)
	const char *sql = "SELECT date, open, high, low, close, volume FROM price_history "
					  "WHERE ticker = ? ORDER BY date DESC LIMIT 1;";

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_STATIC);

		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			// Mapping columns to the StockData struct
			data.ts = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
			data.open = sqlite3_column_double(stmt, 1);
			data.high = sqlite3_column_double(stmt, 2);
			data.low = sqlite3_column_double(stmt, 3);
			data.close = sqlite3_column_double(stmt, 4);
			data.volume = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
		}
		sqlite3_finalize(stmt);
	}
	else
	{
		throw std::runtime_error("Failed to prepare SQLite statement" + std::string(sqlite3_errmsg(db)));
	}
	return data;
}