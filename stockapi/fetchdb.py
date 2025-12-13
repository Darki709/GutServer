import sqlite3
from datetime import datetime
import os


BASE_DIR = os.path.dirname(__file__)  # folder where fetchdb.py lives
DB_DIR = os.path.join(BASE_DIR, "..", "build")  # parent folder + build
os.makedirs(DB_DIR, exist_ok=True)  # make sure folder exists
DB_PATH = os.path.join(DB_DIR, "stock_data.db")


#creates the fetch history db
def fetch_history_create():
	conn = sqlite3.connect(DB_PATH)
	c = conn.cursor()
	c.execute('''CREATE TABLE IF NOT EXISTS fetch_history
				 (ticker TEXT,
		   		  interval TEXT,	
				  fetch_time DATETIME NOT NULL,
		   	 	  PRIMARY KEY (ticker, interval))''')
	conn.commit()
	conn.close()

#creates the price history db
def price_history_create():
	conn = sqlite3.connect(DB_PATH)
	c = conn.cursor()
	c.execute('''CREATE TABLE IF NOT EXISTS price_history
				 (ticker TEXT,
		   		  interval TEXT,
				  date DATETIME NOT NULL,
				  open REAL,
				  high REAL,
				  low REAL,
				  close REAL,
				  volume INTEGER,
		   	 	  PRIMARY KEY (ticker, interval, date))''')
	conn.commit()
	conn.close()


# Function to get the last fetch time to know range for updating data
def last_fetch_time(ticker, interval):
	conn = sqlite3.connect(DB_PATH)
	c = conn.cursor()
	c.execute('''SELECT fetch_time FROM fetch_history
				 WHERE ticker = ? AND interval = ?
				 ''', (ticker, interval))
	result = c.fetchone()
	conn.close()
	if result:
		print("Last fetch time for {ticker} {interval}: {result[0]}")
		date = datetime.strptime(result[0], "%Y-%m-%d %H:%M:%S")
		return date
	else:
		return None
	
def update_fetch_time(ticker, interval, fetch_time):
	conn = sqlite3.connect(DB_PATH)
	c = conn.cursor()
	c.execute('''INSERT INTO fetch_history (ticker, interval, fetch_time)
				VALUES (?, ?, ?) ON CONFLICT(ticker, interval) DO UPDATE SET
   				fetch_time = excluded.fetch_time;
				''', (ticker, interval, fetch_time))
	conn.commit()
	conn.close()

# Function to insert fetched price data into the database retunrns -1 on error 0 on success
def insert_price_data(ticker, interval, data):
    if data.empty:
        return 0  # nothing to insert, not an error

    conn = None
    try:
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()

        rows = [
            (
                ticker,
                interval,
                idx.strftime("%Y-%m-%d %H:%M:%S"),
                float(row.Open),
                float(row.High),
                float(row.Low),
                float(row.Close),
                int(row.Volume)
            )
            for idx, row in data.itertuples()
        ]

        c.executemany("""
            INSERT INTO price_history
                (ticker, interval, date, open, high, low, close, volume)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(ticker, interval, date) DO UPDATE SET
                open   = excluded.open,
                high   = excluded.high,
                low    = excluded.low,
                close  = excluded.close,
                volume = excluded.volume
        """, rows)

        conn.commit()
        update_fetch_time(ticker, interval, data.index[-1].strftime("%Y-%m-%d %H:%M:%S"))
        print(f"Inserted/Updated {len(rows)} rows for {ticker} {interval} \n")
        print(rows)
        return len(rows)

    except Exception as e:
        if conn:
            conn.rollback()
        print(f"Error inserting price data: {e}")
        return -1

    finally:
        if conn:
            conn.close()
    