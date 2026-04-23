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



import requests
import time
from datetime import datetime
from collections import namedtuple
from .fetchdb import *

# We use a namedtuple to mimic the row structure your DB code expects from itertuples()
Row = namedtuple('Row', ['Index', 'Open', 'High', 'Low', 'Close', 'Volume'])

# A simple wrapper to satisfy the "data.empty" and "data.index[-1]" calls in your DB code
class MockDataFrame:
    def __init__(self, rows):
        self.rows = rows
        self.index = [r.Index for r in rows]
        self.empty = len(rows) == 0

    def itertuples(self):
        return self.rows

INTERVAL_MAP = {
    60 : "1m",
    300 : "5m",
    900 : "15m",
    3600 : "1h",
    86400 : "1d",
}

def fetch_live_data(ticker, interval_sec):
    interval = INTERVAL_MAP.get(interval_sec, "1m")
    start_date = last_fetch_time(ticker, interval)
    
    # Setup the API request parameters
    params = {
        "interval": interval,
        "includePrePost": "false"
    }

    # Replicate your logic: 1m is 1d period, no start_date is max, else use start_date
    if interval == "1m":
        params["range"] = "1d"
    elif not start_date:
        params["range"] = "max"
    else:
        # Convert datetime to unix timestamp for the Yahoo API
        params["period1"] = int(start_date.timestamp())
        params["period2"] = int(time.time())

    url = f"https://query1.finance.yahoo.com/v8/finance/chart/{ticker}"
    headers = {'User-Agent': 'Mozilla/5.0'}

    try:
        response = requests.get(url, params=params, headers=headers, timeout=10)
        if response.status_code != 200:
            return -1

        json_data = response.json()
        result = json_data.get('chart', {}).get('result', [None])[0]
        if not result:
            return -1

        timestamps = result.get('timestamp', [])
        indicators = result.get('indicators', {}).get('quote', [{}])[0]
        
        # Yahoo sometimes misses the 'volume' key or provides it as 'vol'
        vols = indicators.get('volume', [0] * len(timestamps))
        opens = indicators.get('open', [])
        highs = indicators.get('high', [])
        lows = indicators.get('low', [])
        closes = indicators.get('close', [])

        rows = []
        for i in range(len(timestamps)):
            # Skip entries with missing data to prevent DB errors
            if opens[i] is None or closes[i] is None:
                continue
                
            rows.append(Row(
                Index=datetime.fromtimestamp(timestamps[i]),
                Open=float(opens[i]),
                High=float(highs[i]),
                Low=float(lows[i]),
                Close=float(closes[i]),
                Volume=int(vols[i]) if vols[i] is not None else 0
            ))

        data = MockDataFrame(rows)
        
        if data.empty:
            return -1

        # Pass to your existing fetchdb.py function
        db_status = insert_price_data(ticker, interval, data)
        
        if db_status == -1:
            return -2
            
        return 0

    except Exception as e:
        print(f"Fetcher Crash avoided: {e}")
        return -1



	
		