import yfinance as yf
import pandas as pd
from .fetchdb import *


INTERVAL_MAP = {
    60 : "1m",
	300 : "5m",
	900 : "15m",
	3600 : "1h",
	86400 : "1d",
}

#ticker as a string and interval as integer in seconds (according to enum)
#fetch price data for a live client, defualtly treating data as if client just connected and no data was loaded previously
#return -1 for fetch error, -2 for db insert error, else returns number of price points fetched
def fetch_live_data(ticker,  interval ):
	interval = INTERVAL_MAP[interval]
	start_date = last_fetch_time(ticker, interval)
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



	
		