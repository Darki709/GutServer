import yfinance as yf
import pandas as pd
import fetchdb


#fetch data from a range of dates
def fetch_history_data(ticker_symbol, interval, start_date, end_date):
	pass


#fetch price data for a live client, defualtly treating data as if client just
#connected and no data was loaded previously
#return -1 for error, and the last price for a successful fetch
def fetch_live_data(ticker,  interval, start_date="null"):
	ticker_obj = yf.Ticker(ticker)
	if start_date == "null":
		data = ticker_obj.history(interval=interval, period="max")
		if data is None or data.empty:
			return -1
	else :
		data = ticker_obj.history(interval=interval, start=start_date)
		if data is None or data.empty:
			return -1
		