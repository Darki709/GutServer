#include "stock_db_helper.hpp"
#include <filesystem>

namespace fs = std::filesystem;

PyThreadState *Gut::Stock_helper::s_main_tstate = nullptr; // Initialize static member

void Gut::Stock_helper::init()
{
	// --- 1. PRE-INITIALIZATION (Environment & DLL Injection) ---
	_putenv("OMP_NUM_THREADS=1");
	_putenv("OPENBLAS_NUM_THREADS=1");

	wchar_t rawPath[MAX_PATH];
	GetModuleFileNameW(NULL, rawPath, MAX_PATH);
	fs::path exeDir = fs::path(rawPath).parent_path();
	fs::path pyHome = exeDir / "python_runtime";
	fs::path stdlibZip = pyHome / "python313.zip";
	fs::path curlCffiDir = pyHome / "Lib" / "site-packages" / "curl_cffi";

	// --- 2. THE C++ DLL FORCE-LOAD (The Secret Sauce) ---
	// We manually load libcurl to resolve the "specified module could not be found" error.
	std::vector<std::wstring> criticalDlls = {
		L"libcrypto-3-x64.dll",
		L"libssl-3-x64.dll",
		L"libcurl.dll" // Most important one for curl_cffi
	};

	std::cout << "--- Diagnostic: Probing C-Dependencies ---" << std::endl;
	for (const auto &dllName : criticalDlls)
	{
		fs::path dllPath = curlCffiDir / dllName;
		HMODULE hMod = LoadLibraryExW(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (hMod)
		{
			std::wcout << L"SUCCESS: Pre-loaded " << dllName << std::endl;
		}
		else
		{
			DWORD err = GetLastError();
			std::wcout << L"ERROR: Failed to load " << dllName << L" (Code: " << err << L")" << std::endl;
			std::cout << "Target Path: " << dllPath.string() << std::endl;
		}
	}

	// --- 3. STANDARD PYTHON CONFIGURATION ---
	PyConfig config;
	PyConfig_InitIsolatedConfig(&config);
	config.isolated = 1;
	config.use_environment = 0;
	config.site_import = 1;

	PyConfig_SetString(&config, &config.home, pyHome.wstring().c_str());
	PyConfig_SetString(&config, &config.base_executable, rawPath);

	// IMPORTANT: This tells Python where to find the 'encodings' module
	config.module_search_paths_set = 1;
	PyWideStringList_Append(&config.module_search_paths, stdlibZip.wstring().c_str()); // Points to python313.zip
	PyWideStringList_Append(&config.module_search_paths, (pyHome / "DLLs").wstring().c_str());
	PyWideStringList_Append(&config.module_search_paths, (pyHome / "Lib").wstring().c_str());
	PyWideStringList_Append(&config.module_search_paths, (pyHome / "Lib" / "site-packages").wstring().c_str());
	PyWideStringList_Append(&config.module_search_paths, exeDir.wstring().c_str());

	fs::path stockapiPath = exeDir / "stockapi";
	PyWideStringList_Append(&config.module_search_paths, stockapiPath.wstring().c_str());

	PyStatus status = Py_InitializeFromConfig(&config);

	s_main_tstate = PyEval_SaveThread(); // Save the main thread state for sub-interpreters
	if (PyStatus_Exception(status))
	{
		std::cerr << "Python initialization failed: " << status.err_msg << std::endl;
		throw std::runtime_error("Failed to initialize Python interpreter");
	}
}

void Gut::Stock_helper::shutdown()
{
	PyEval_RestoreThread(s_main_tstate); // Switch to main thread state to finalize
	Py_Finalize();
	s_main_tstate = nullptr;
	std::cout << "Python Environment Shutdown Successfully." << std::endl;
}

Gut::Stock_helper::Stock_helper()
{

	PyEval_RestoreThread(s_main_tstate);
	m_tstate = PyThreadState_New(s_main_tstate->interp); 
	PyThreadState* oldState = PyThreadState_Swap(m_tstate);

	if (m_tstate == nullptr)
	{
		// If this fails, the main lock is still held!
		// We must release it before crashing.
		s_main_tstate = PyEval_SaveThread();
		throw std::runtime_error("Python: Failed to create sub-interpreter");
	}


	// --- 4. PYTHON-SIDE PATH REGISTRATION ---
	std::string forceLoadScript = R"(
import os, sys, ctypes
from pathlib import Path

exe_dir = Path(sys.executable).parent

# 1. Force-load the Microsoft C++ Runtimes FIRST
runtimes = ['vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll']
for dll in runtimes:
	try: ctypes.WinDLL(str(exe_dir / dll))
	except Exception as e: print(f'Failed to load runtime {dll}: {e}')

# 2. Now load the Curl dependencies
curl_dlls = ['zlib.dll', 'libcrypto-3-x64.dll', 'libssl-3-x64.dll', 'libcurl.dll']
for dll in curl_dlls:
	try: ctypes.WinDLL(str(exe_dir / dll))
	except Exception as e: print(f'Failed to load curl component {dll}: {e}')
	)";
	PyRun_SimpleString(forceLoadScript.c_str());

	std::string portableFix = R"(
import os, sys, ctypes
from pathlib import Path

# 1. Detect where we are
exe_dir = Path(sys.executable).parent
runtime_lib = exe_dir / 'python_runtime' / 'Lib' / 'site-packages'
curl_dir = runtime_lib / 'curl_cffi'

# 2. Add DLL search paths dynamically
if curl_dir.exists():
	os.add_dll_directory(str(curl_dir))
	print(f"Portable: Registered {curl_dir}")

# 3. Locate the wrapper (using a wildcard to find the .pyd even if the suffix changes)
try:
	pyd_files = list(curl_dir.glob('_wrapper*.pyd'))
	if pyd_files:
		pyd_path = pyd_files[0]
		# Use the same 'ALTERED_SEARCH_PATH' logic but with the dynamic path
		handle = ctypes.windll.kernel32.LoadLibraryExW(str(pyd_path), None, 0x00000008)
		if handle:
			print(f"SUCCESS: Portable load of {pyd_path.name} at {handle}")
		else:
			err = ctypes.windll.kernel32.GetLastError()
			print(f"KERNEL ERROR: {err} at path {pyd_path}")
	else:
		print("ERROR: Could not find _wrapper.pyd in curl_cffi folder.")
except Exception as e:
	print(f"PORTABLE LOAD ERROR: {e}")
	)";

	PyRun_SimpleString(portableFix.c_str());

	std::string linkTest = R"(
import os, sys, ctypes
from pathlib import Path
exe_dir = Path(sys.executable).parent
curl_dir = exe_dir / 'python_runtime' / 'Lib' / 'site-packages' / 'curl_cffi'

os.add_dll_directory(str(curl_dir))
pyd_path = curl_dir / '_wrapper.cp313-win_amd64.pyd'

try:
	# Attempting to load the binary extension directly
	ctypes.PyDLL(str(pyd_path))
	print('SUCCESS: _wrapper.pyd binary is loadable.')
except Exception as e:
	print(f'LINK FAILURE: {e}')
	# Check if a specific dependency is missing
	for dll in ['libcurl.dll', 'libcrypto-3.dll', 'libssl-3.dll']:
		if not (curl_dir / dll).exists():
			print(f'MISSING FROM FOLDER: {dll}')
	)";
	PyRun_SimpleString(linkTest.c_str());

	// --- 5. THE ACTUAL IMPORT ---
	std::cout << "Final attempt to import stockapi..." << std::endl;
	std::string cmd = "import traceback\ntry:\n    import stockapi.stockapi\n    print('--- SYSTEM ONLINE ---')\nexcept Exception:\n    traceback.print_exc()";
	PyRun_SimpleString(cmd.c_str());

	// Get the module handle from sys.modules
	m_pModule = PyImport_ImportModule("stockapi.stockapi");

	if (!m_pModule)
	{
		PyErr_Print();
		throw std::runtime_error("Failed to import stockapi.");
	}

	m_pFunc = PyObject_GetAttrString(m_pModule, "fetch_live_data");

	if (!m_pFunc || !PyCallable_Check(m_pFunc))
	{
		Py_XDECREF(m_pFunc);
		Py_DECREF(m_pModule);
		throw std::runtime_error("fetch_live_data missing or not callable.");
	}

	PyThreadState_Swap(oldState);
	s_main_tstate = PyEval_SaveThread(); // release worker
	std::cout << "Python Environment Initialized Successfully for worker." << std::endl;
}

Gut::Stock_helper::~Stock_helper()
{

	if (s_main_tstate)
	{
		// Restore the state to clean it up
		PyEval_RestoreThread(m_tstate);

		Py_XDECREF(m_pFunc);
		Py_XDECREF(m_pModule);

		// Delete the thread state
		PyThreadState_Clear(m_tstate);
		PyThreadState_Delete(m_tstate);
		
		m_tstate = nullptr;
	}
}

int Gut::Stock_helper::fetchLiveData(String &ticker, uint32_t interval)
{
	std::cout << "C++: Entering fetchLiveData" << std::endl;
	// aquire GIL safely with RAII
	class PythonContextGuard
	{
		PyThreadState *state;

	public:
		PythonContextGuard(PyThreadState *s) : state(s) { PyEval_RestoreThread(state); }
		~PythonContextGuard() { state = PyEval_SaveThread(); }
	} guard(this->m_tstate);

	std::cout << "Sub-interpreter context acquired." << std::endl;
	if (m_pFunc == nullptr)
	{
		fprintf(stderr, "CRASH PREVENTION: m_pFunc is NULL!\n");
		throw std::runtime_error("fetch_live_data function not found in Python module");
	}
	if (!PyCallable_Check(m_pFunc))
	{
		fprintf(stderr, "CRASH PREVENTION: m_pFunc is not callable!\n");
		throw std::runtime_error("fetch_live_data is not callable");
	}
	try
	{
		PyObject *pArgs = PyTuple_Pack(2, PyUnicode_FromString(ticker.c_str()), PyLong_FromUnsignedLong(interval));
		PyObject *pValue = PyObject_CallObject(m_pFunc, pArgs);
		Py_DECREF(pArgs);
		std::cout << "C++: Python Call Returned" << std::endl;
		if (pValue)
		{
			int status = (int)PyLong_AsLong(pValue);
			Py_XDECREF(pValue);
			std::cout << "Python fetch_live_data returned status: " << status << std::endl;
			return status;
		}
	}
	catch (...)
	{}
	PyErr_Print();
	std::cout << "Exception occurred while calling Python function." << std::endl;
	return -1; // Indicate failure to fetch data
}

std::optional<Gut::StockData> Gut::Stock_helper::getLastRowFromDB(String &symbol)
{
	std::cout << "fetching streaming data for " << symbol << std::endl;
	StockData data = {0}; // Initialize with zeros
	struct DB_Connection
	{
		sqlite3 *db;

		DB_Connection()
		{
			// Connect to SQLite
			wchar_t rawPath[MAX_PATH];
			GetModuleFileNameW(NULL, rawPath, MAX_PATH);
			fs::path exeDir = fs::path(rawPath).parent_path();
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
		~DB_Connection()
		{
			if (db)
			{
				sqlite3_close(db);
			}
		}
	} db_conn;

	sqlite3 *db = db_conn.db;

	sqlite3_stmt *stmt;

	// Query to get the latest record based on timestamp (date)
	const char *sql = "SELECT date, open, high, low, close, volume FROM price_history "
					  "WHERE ticker = ? AND interval = '1m' ORDER BY date DESC LIMIT 1;";

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
		else
		{
			std::cout << "No data found for symbol: " << symbol << std::endl;
			return std::nullopt; // No data found for this symbol
		}
		sqlite3_finalize(stmt);
	}
	else
	{
		throw std::runtime_error("Failed to prepare SQLite statement" + std::string(sqlite3_errmsg(db)));
	}
	return std::make_optional(data); // Return the fetched data wrapped in an optional
}