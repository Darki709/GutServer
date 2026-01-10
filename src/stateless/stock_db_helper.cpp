#include "stock_db_helper.hpp"
#include <filesystem>

namespace fs = std::filesystem;

Gut::Stock_helper::Stock_helper()
{
	PyStatus status;
	PyConfig config;
	PyConfig_InitIsolatedConfig(&config);

	// ------------------------------------
	// 1. Locate paths relative to EXE
	// ------------------------------------
	wchar_t rawPath[MAX_PATH];
	if (!GetModuleFileNameW(NULL, rawPath, MAX_PATH))
		throw std::runtime_error("GetModuleFileNameW failed");

	fs::path exeDir = fs::path(rawPath).parent_path();
	fs::path pyHome = exeDir / "python_runtime";
	fs::path stdlibZip = pyHome / "python313.zip";
	fs::path sitePackages = pyHome / "Lib" / "site-packages";

	// ------------------------------------
	// 2. Validate Runtime Files
	// ------------------------------------
	if (!fs::exists(stdlibZip))
		throw std::runtime_error("Critical: python313.zip missing from runtime folder.");

	// ------------------------------------
	// 3. Configure Isolated Environment
	// ------------------------------------
	config.isolated = 1;
	config.use_environment = 0;
	config.site_import = 0; // We will load 'site' manually after init
	config.module_search_paths_set = 1;

	PyConfig_SetString(&config, &config.home, pyHome.wstring().c_str());
	PyConfig_SetString(&config, &config.program_name, rawPath);
	PyConfig_SetString(&config, &config.stdlib_dir, stdlibZip.wstring().c_str());

	// Search path priority
	PyWideStringList_Append(&config.module_search_paths, sitePackages.wstring().c_str());
	PyWideStringList_Append(&config.module_search_paths, exeDir.wstring().c_str());
	PyWideStringList_Append(&config.module_search_paths, stdlibZip.wstring().c_str());
	PyWideStringList_Append(&config.module_search_paths, (pyHome / "DLLs").wstring().c_str());

	// ------------------------------------
	// 4. Initialize Interpreter
	// ------------------------------------
	status = Py_InitializeFromConfig(&config);
	PyConfig_Clear(&config);

	if (PyStatus_Exception(status))
		Py_ExitStatusException(status);

	// ------------------------------------
	// 5. Windows DLL & Site-Packages Fix
	// ------------------------------------
	std::string forceLoadScript =
		"import os, sys, ctypes\n"
		"from pathlib import Path\n"
		"exe_dir = Path(sys.executable).parent\n"
		"\n"
		"# 1. Force-load the Microsoft C++ Runtimes FIRST\n"
		"runtimes = ['vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll']\n"
		"for dll in runtimes:\n"
		"    try: ctypes.WinDLL(str(exe_dir / dll))\n"
		"    except Exception as e: print(f'Failed to load runtime {dll}: {e}')\n"
		"\n"
		"# 2. Now load the Curl dependencies\n"
		"curl_dlls = ['zlib.dll', 'libcrypto-3-x64.dll', 'libssl-3-x64.dll', 'libcurl.dll']\n"
		"for dll in curl_dlls:\n"
		"    try: ctypes.WinDLL(str(exe_dir / dll))\n"
		"    except Exception as e: print(f'Failed to load curl component {dll}: {e}')\n";

	PyRun_SimpleString(forceLoadScript.c_str());

	std::string absoluteFix =
		"import os, sys, ctypes\n"
		"from pathlib import Path\n"
		"curl_dir = Path(r'D:/tomer/Gut/Gut Webserver/build/Debug/python_runtime/Lib/site-packages/curl_cffi')\n"
		"pyd_path = curl_dir / '_wrapper.cp313-win_amd64.pyd'\n"
		"\n"
		"# Add directory to search path\n"
		"os.add_dll_directory(str(curl_dir))\n"
		"\n"
		"try:\n"
		"    # Load with a specific flag that tells Windows to look in the DLL's own folder\n"
		"    # 0x00000008 is LOAD_WITH_ALTERED_SEARCH_PATH\n"
		"    handle = ctypes.windll.kernel32.LoadLibraryExW(str(pyd_path), None, 0x00000008)\n"
		"    if handle:\n"
		"        print(f'SUCCESS: Windows Kernel loaded _wrapper.pyd at {handle}')\n"
		"    else:\n"
		"        err = ctypes.windll.kernel32.GetLastError()\n"
		"        print(f'KERNEL ERROR CODE: {err}')\n"
		"except Exception as e:\n"
		"    print(f'LOAD ERROR: {e}')\n";

	PyRun_SimpleString(absoluteFix.c_str());

	std::string linkTest =
		"import os, sys, ctypes\n"
		"from pathlib import Path\n"
		"exe_dir = Path(sys.executable).parent\n"
		"curl_dir = exe_dir / 'python_runtime' / 'Lib' / 'site-packages' / 'curl_cffi'\n"
		"\n"
		"os.add_dll_directory(str(curl_dir))\n"
		"pyd_path = curl_dir / '_wrapper.cp313-win_amd64.pyd'\n"
		"\n"
		"try:\n"
		"    # Attempting to load the binary extension directly\n"
		"    ctypes.PyDLL(str(pyd_path))\n"
		"    print('SUCCESS: _wrapper.pyd binary is loadable.')\n"
		"except Exception as e:\n"
		"    print(f'LINK FAILURE: {e}')\n"
		"    # Check if a specific dependency is missing\n"
		"    for dll in ['libcurl.dll', 'libcrypto-3.dll', 'libssl-3.dll']:\n"
		"        if not (curl_dir / dll).exists():\n"
		"            print(f'MISSING FROM FOLDER: {dll}')\n";

	PyRun_SimpleString(linkTest.c_str());

	// ------------------------------------
	// 6. Import your module
	// ------------------------------------
	m_pModule = PyImport_ImportModule("stockapi.stockapi");
	if (!m_pModule)
	{
		PyErr_Print();
		throw std::runtime_error("Failed to import stockapi.");
	}

	m_pFunc = PyObject_GetAttrString(m_pModule, "fetch_live_data");
	if (!m_pFunc || !PyCallable_Check(m_pFunc) || m_pFunc == nullptr)
	{
		throw std::runtime_error("fetch_live_data is missing or not callable");
	}

	// Release GIL for multi-threaded C++ apps
	PyEval_SaveThread();

	// 4. Connect to SQLite
	fs::path dbPath = exeDir / "database" / "stock_data.db";

	if (!fs::exists(dbPath.parent_path()))
	{
		fs::create_directories(dbPath.parent_path());
	}

	int rc = sqlite3_open(dbPath.string().c_str(), &db);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("SQLite Error: " + std::string(sqlite3_errmsg(db)));
	}

	// Performance settings for your DB
	sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
	sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
}

Gut::Stock_helper::~Stock_helper()
{
	Py_XDECREF(m_pFunc);
	Py_XDECREF(m_pModule);
	Py_Finalize();
	// clsoe db connection
	if (db)
	{
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
	if (m_pFunc == nullptr)
	{
		fprintf(stderr, "CRASH PREVENTION: m_pFunc is NULL!\n");
		return;
	}
	if (!PyCallable_Check(m_pFunc))
	{
		fprintf(stderr, "CRASH PREVENTION: m_pFunc is not callable!\n");
		return;
	}
	try
	{
		PyObject *pArgs = PyTuple_Pack(2, PyUnicode_FromString(ticker.c_str()), PyLong_FromUnsignedLong(interval));
		PyObject *pValue = PyObject_CallObject(m_pFunc, pArgs);
		Py_DECREF(pArgs);
		std::cout << "C++: Python Call Returned" << std::endl;
		if (!pValue)
		{
			Py_XDECREF(m_pFunc);
			Py_XDECREF(m_pModule);
			throw std::runtime_error("Python fetch_live_data call failed");
			int rowsFetched = (int)PyLong_AsLong(pValue);
			Py_XDECREF(pValue);
			std::cout << rowsFetched << std::endl;

			if (rowsFetched < -1)
			{
				throw std::runtime_error("Failed to fetch live data from Python " + std::to_string(rowsFetched)); //-1 for api error -2 for db error
			}
		}
	}
	catch (...)
	{
		PyErr_Print();
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