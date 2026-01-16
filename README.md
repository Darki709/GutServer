Secure Stock API Server
This is a personal project I built to handle secure stock data fetching. It uses a C++ backend to manage network connections and an embedded Python instance to handle the actual data scraping/API calls.

How it works
The core idea was to combine C++ networking with Python’s financial libraries without running them as separate processes.

1. Networking & Multi-threading
The server is written in C++ using WinSock. It uses a worker thread pool to handle connections so that the main "accept" loop doesn't get blocked.

It's currently tuned to handle around 500-1000 concurrent connections (based on local test using the test python script).

I’m using a basic framing protocol (length-prefixed packets) to make sure messages don't get cut off or merged over the TCP stream.

2. Embedded Python
Instead of calling a .py file through the command line, the server embeds the Python 3.13 interpreter directly into the C++ code.

The C++ workers manage the GIL (Global Interpreter Lock) whenever they need to call the stockapi.py script.

Data fetching is handled by yfinance and curl_cffi to get around some of the request limitations you usually hit with standard libraries.

3. The Secure Tunnel
Since the data is sensitive, I implemented a custom secure tunnel for the Android client.

Handshake: It starts with an RSA key exchange to set up a session.

Encryption: Everything after the handshake is encrypted with AES-256-GCM.

I used OpenSSL to do all the cryptography.

The Workflow

Client connects via TCP and does an RSA handshake.

Once the AES session key is set, the client sends an encrypted request.

The C++ server decrypts it, grabs the Python GIL, and runs the scraping logic.

The result is encrypted and sent back to the client.
