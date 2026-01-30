from ast import If
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


def validate_date_string(date_obj):
    # Since date_obj is already a datetime object, we skip strptime
    date_format = "%Y-%m-%d"
    
    # 1. Get the current time and calculate the 8-day threshold
    now = datetime.now()
    cutoff_date = now - timedelta(days=8)
    
    # 2. Check if the object is older than 8 days
    if date_obj < cutoff_date:
        # It's too old: return the cutoff point as a string for yfinance/SQL
        return cutoff_date.strftime(date_format)
    # 3. If it's valid, just convert it to the required string format
    return date_obj.strftime(date_format)


#ticker as a string and interval as integer in seconds (according to enum)
#fetch price data for a live client, defualtly treating data as if client just connected and no data was loaded previously
#return -1 for fetch error, -2 for db insert error, else returns number of price points fetched
def fetch_live_data(ticker,  interval ):
	interval = INTERVAL_MAP[interval]
	start_date = last_fetch_time(ticker, interval)
	if interval == "1m":
		start_date = validate_date_string(start_date)
	ticker_obj = yf.Ticker(ticker)
	if not start_date:
		data = ticker_obj.history(interval=interval, period="max")
		if data is None or data.empty:
			return -1
	else:
		data = ticker_obj.history(interval=interval, start=start_date)
		if data is None or data.empty:
			return -1
	if insert_price_data(ticker, interval, data) == -1:
		return -2	
	return len(data) if data is not None else 0



	
		