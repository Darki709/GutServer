import yfinance as yf
import pandas as pd
from fetchdb import *


#fetch price data for a live client, defualtly treating data as if client just
#connected and no data was loaded previously
#return -1 for fetch error, -2 for db insert error, else returns number of prive points fetched
def fetch_live_data(ticker,  interval):
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
	
if __name__ == "__main__":
	fetch_history_create()
	price_history_create()
	fetch_live_data("AAPL", "1d")
		