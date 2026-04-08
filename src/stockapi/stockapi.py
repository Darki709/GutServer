from ast import If

import os
import sys
from pathlib import Path

# Get the directory where this script is running
current_dir = Path(__file__).parent.resolve()

# Find the python_runtime relative to your build folder
# Adjust the number of .parent calls to reach the 'python_runtime' folder
runtime_dll_dir = current_dir.parent / "python_runtime" / "Lib" / "site-packages" / "curl_cffi"

if runtime_dll_dir.exists():
    # This is the magic fix for Windows 10/11 to find local DLLs
    os.add_dll_directory(str(runtime_dll_dir))    
    os.add_dll_directory(str(current_dir.parent / "python_runtime"))



import yfinance as yf
import pandas as pd
import time
from .fetchdb import *
from datetime import datetime, timedelta


INTERVAL_MAP = {
	60 : "1m",
	300 : "5m",
	900 : "15m",
	3600 : "1h",
	86400 : "1d",
}


#ticker as a string and interval as integer in seconds (according to enum)
#fetch price data for a live client, defualtly treating data as if client just connected and no data was loaded previously
#return -1 for fetch error, -2 for db insert error, else returns 0 for success
def fetch_live_data(ticker,  interval ):
	interval = INTERVAL_MAP[interval]
	#start_date = last_fetch_time(ticker, interval)
	ticker_obj = yf.Ticker(ticker)
	if interval == "1m": #it means that client requested streaming data we only return latest price point
		data = ticker_obj.history(interval=interval, period="1d")
	else:
		data = ticker_obj.history(interval=interval, period="max")
	#else:
	#	data = ticker_obj.history(interval=interval, start=start_date)
	if data is None or data.empty:
		return -1
	if insert_price_data(ticker, interval, data) == -1:
		return -2
	return 0



	
		