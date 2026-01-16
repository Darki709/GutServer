## Secure Stock API Server

This is a personal project I built to handle secure stock data fetching. It uses a **C++ backend** to manage network connections and an **embedded Python instance** to handle the actual data scraping and API calls.

---

### How it works
The core idea was to combine C++ networking with Python’s financial libraries without running them as separate processes.

#### 1. Networking & Multi-threading
The server is written in C++ using **WinSock**. It uses a worker thread pool to handle connections so that the main "accept" loop doesn't get blocked.

* **Concurrency:** It's currently tuned to handle around **500-1000 concurrent connections** (based on local tests using a Python stress-test script).
* **Framing:** I’m using a basic framing protocol (**length-prefixed packets**) to make sure messages don't get cut off or merged over the TCP stream.



#### 2. Embedded Python
Instead of calling a `.py` file through the command line, the server embeds the **Python 3.13 interpreter** directly into the C++ code.

* **GIL Management:** The C++ workers manage the **GIL (Global Interpreter Lock)** whenever they need to call the `stockapi.py` script.
* **Libraries:** Data fetching is handled by `yfinance` because it's the only free api that met my development needs.



#### 3. The Secure Tunnel
Since the data is sensitive, I implemented a custom secure tunnel for the client.

* **Handshake:** It starts with an **RSA key exchange** to set up a session.
* **Encryption:** Everything after the handshake is encrypted with **AES-256-GCM**.
* **Library:** I used **OpenSSL** to handle all the cryptography.



---

### The Workflow

1. **Connection:** Client connects via TCP and performs an RSA handshake.
2. **Key Exchange:** Once the AES session key is set, the client sends an encrypted request.
3. **Processing:** The C++ server decrypts the request, grabs the Python GIL, and runs the scraping logic.
4. **Response:** The result is encrypted and sent back to the client.
