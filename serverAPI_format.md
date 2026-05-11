# Server-Client Communication Protocol (Updated)

## General Rules
1. All client messages include a unique Request ID (uint32).
2. Server messages may be encrypted using AES-256-GCM.
3. Nonces:
   - send_nonce: increments for every client message sent with encryption.
   - recv_nonce: increments for every decrypted server message received.
4. Flags:
   - 0x00: Plaintext
   - 0x01: Encrypted (AES-GCM)
5. Debug: every message should log Flag, Message Type (encrypted if flag=1), Payload length/hex, Nonces.

## Message Format

### Client-to-Server
[4B Total Length][1B Flag][Payload]

- Total Length: includes Flag + Payload
- Flag:
    - 0x00: plaintext
    - 0x01: encrypted (AES-GCM, uses send_nonce)
- Payload:
    - If plaintext (0x00): [1B Outgoing Message Type][4B Request ID][Plaintext Payload]
    - If encrypted (0x01): [Encrypted(1B Outgoing Message Type + 4B ReqID + Payload)][16B GCM tag]

### Server-to-Client
[4B Total Length][1B Flag][Payload]

- Total Length: includes Flag + Payload
- Flag:
    - 0x00: plaintext
    - 0x01: encrypted (AES-GCM, uses recv_nonce)
- Payload:
    - If plaintext (0x00): [1B Incoming Message Type][Payload]
    - If encrypted (0x01): [Encrypted(1B Incoming Message Type + Payload)][16B GCM tag]

## Handshake Sequence

1. **HandshakeHello (Client → Server)**
    - Client generates RSA keypair (PEM base64 encoded rsa key).
    - Sends: Outgoing Type 0 (HANDSHAKEHELLO), Request ID, client public key (PEM)
    - Flag: plaintext (0x00)

2. **Server Response: AES Key**
    - Server generates random AES-256 session key.
    - Encrypts session key with client’s RSA public key (OAEP padding with SHA1).
    - Sends message: [AES key] as payload
    - Flag: plaintext (0x00)
    - Incoming Type 0 (VERIFY)

3. **Client AES Session Initialization**
    - Client decrypts AES key with private RSA key.
    - AES key stored for future messages.
    - send_nonce = 0, recv_nonce = 0

4. **Handshake Verify / Test Message (Client → Server)**
    - Client encrypts [1B Outgoing MsgType + 4B ReqID + payload]
    - Payload = the word "encrypted" encrypted in AES ECB with the new 	  AES key
    - **Flag remains plaintext (0x00)** for this handshake step
    - Outgoing Type 1 (TEST / VERIFY)

5. **Server Handshake Response**
    - Server decrypts client message using AES-256-EVP
    - Validates message
    - Sends confirmation: Incoming Type 1 (HANDSHAKE_SUCCESS)
    - Payload: optional
    - Flag: encrypted (0x01)
    - send_nonce increments on the server and so the client will increment it's recv_nonce when decrypting this message


# Price Data Request Format

**Task Type:** `4` (Tasktype::REQUESTTICKERDATA)

This request is sent by a client to retrieve historical or streaming price data for a given ticker symbol.

---

## Payload Structure

| Offset | Size (bytes) | Field        | Description |
|--------|--------------|--------------|-------------|
| 0      | 1            | `symbolLen`  | Length of the ticker symbol (max 255). |
| 1      | `symbolLen`  | `symbol`     | ASCII string representing the ticker symbol (e.g., `"AAPL"`). |
| ?      | 4            | `interval`   | Time interval of the candles, `uint32_t`. Valid values:  
|        |              |              | - `60` → 1 minute (`Interval::MIN_1`)  
|        |              |              | - `300` → 5 minutes (`Interval::MIN_5`)  
|        |              |              | - `900` → 15 minutes (`Interval::MIN_15`)  
|        |              |              | - `3600` → 1 hour (`Interval::HOUR_1`)  
|        |              |              | - `86400` → 1 day (`Interval::DAY_1`) |
| ?      | 8            | `start_ts`   | Start timestamp (seconds since Unix epoch, `uint64_t`). |
| ?      | 8            | `end_ts`     | End timestamp (seconds since Unix epoch, `uint64_t`). |
| ?      | 1            | `flags`      | Bitwise flags (`uint8_t`) specifying request options:  
|        |              |              | - `0x01` → `SNAPSHOT` (historical snapshot)  
|        |              |              | - `0x02` → `STREAM` (live streaming feed)  

---

## Notes

- `interval` determines the granularity of returned candlestick data.
- `start_ts` and `end_ts` define the range for historical data.
- Flags can be combined (e.g., `SNAPSHOT | STREAM`).
- Client privileges are checked; insufficient privileges result in `ILLEGALACCESS`.
- Invalid intervals will result in `INVALIDREQUEST`.

---

## Example

Request 5-minute candles for `"AAPL"` from `1 Jan 2026 00:00:00 UTC` to `2 Jan 2026 00:00:00 UTC` as a snapshot:

| Field       | Value                      |
|-------------|----------------------------|
| `symbolLen` | `4`                        |
| `symbol`    | `"AAPL"`                   |
| `interval`  | `300`                      |
| `start_ts`  | `1767225600`               |
| `end_ts`    | `1767312000`               |
| `flags`     | `0x01` (`SNAPSHOT`)        |

---

## Byte Layout Diagram

+---------+----------------+--------------------+
| Offset | Field | Notes |
+---------+----------------+--------------------+
| 0 | symbolLen | 1 byte |
| 1 | symbol | symbolLen bytes |
| ? | interval | 4 bytes, uint32_t |
| ? | start_ts | 8 bytes, uint64_t |
| ? | end_ts | 8 bytes, uint64_t |
| ? | flags | 1 byte |
+---------+----------------+--------------------+

# Price Data Response Message Format

This document defines the **server → client** message format used to deliver price (candle) data in response to a price data request.

---

## Message Overview

Each response message contains **one chunk** of candle data.  
Large datasets are split across **multiple messages**.

┌──────────┬──────────────┬──────────┬──────────┬─────────┬──────────────┬──────────────┐
│ Length │ Encrypted │ MsgType │ ReqID │ IsLast │ CandleCount │ Candle Data │
└──────────┴──────────────┴──────────┴──────────┴─────────┴──────────────┴──────────────┘


---

## Header Fields

### 1. Length
- **Size:** 4 bytes (uint32)
- **Description:** Total message length in bytes (including header)
- **Byte order:** Network (big endian)

---

### 2. Encrypted
- **Size:** 1 byte
- **Values:**
  - `0x00` – plaintext
  - `0x01` – encrypted

---

### 3. MsgType
- **Size:** 1 byte
- **Value:** `0x02`
- **Description:** Price Data Response

---

### 4. ReqID
- **Size:** 4 bytes (uint32)
- **Description:** Request ID of the original price data request
- **Byte order:** Network (big endian)

---

### 5. IsLast
- **Size:** 1 byte
- **Values:**
  - `0x00` – more messages will follow
  - `0x01` – this is the final message for this request
- **Purpose:** Allows the client to detect completion and handle retransmission on timeout

---

### 6. CandleCount
- **Size:** 2 bytes (uint16)
- **Description:** Number of candles contained in this message
- **Maximum recommended:** `256`
- **Byte order:** Network (big endian)

---

## Candle Data Section

Immediately follows the header.  
Candles are packed **back-to-back** with no separators.

---

## Candle Format (48 bytes each)

┌────────────┬──────┬──────┬──────┬──────┬────────┐
│ Timestamp │ Open │ High │ Low │ Close│ Volume │
└────────────┴──────┴──────┴──────┴──────┴────────┘

### Fields

| Field      | Size | Type     | Description                          |
|-----------|------|----------|--------------------------------------|
| Timestamp | 8    | uint64   | Unix timestamp (seconds, UTC)        |
| Open      | 8    | double   | Opening price                        |
| High      | 8    | double   | Highest price                        |
| Low       | 8    | double   | Lowest price                         |
| Close     | 8    | double   | Closing price                        |
| Volume    | 8    | uint64   | Trade volume                         |

- Timestamp and Volume are **network byte order**
- Doubles are sent as raw IEEE-754 (both sides must agree on endianness)

---

## Message Chunking Rules

- Large datasets **must** be split into multiple messages
- Each message carries its own `CandleCount`
- `IsLast = 1` only on the final chunk
- Client assumes failure if `IsLast` is not received within timeout

---

## Parsing Notes (Client)

- Read fixed header first
- Allocate `CandleCount × 48` bytes
- Parse candles using fixed offsets
- No dynamic field lengths
- No delimiters
- If the candle count is 0 and isDone is set to true it means that there is no available data for the exact ticket interval pair

#Stream response message

message type: 3 

[4 bytes length | is_encrypted flags 1 byte set to encrypted][1 byte message type 3 STREAM | 4 bytes requests id used by client to ask for the streaming | 8 bytes unix ts | 8 bytes open | 8 bytes high | 8 bytes low | 8 bytes close | 8 bytes volume]



#Cancel Ticker Stream

task type number: 5

[header as usual set to notify encrypted message][1 byte task type set to 5 CANCELTIKCERSTREAM |4 bytes client request id | 4 bytes request id of the original streaming request (network byte order) |1 bytes length of the symbol | the symbol in length bytes]

# Register User request

Task type: 3

[usual header set flag to encrypted][1 byte task type set to 3 REGISTER |  4 bytes client request id | 1 byte username length | username | 1 byte password length | password]
maximum payload length is 1 (username length) + 1 (password length) + 255 (username max length) + 255 (password max length) = 512 bytes longer requests will be regarded as INVALIDREQUEST and will be dropped

#Register message response 

Message type: 4
[usual header set flag to encrypted][1 byte task type set to 4 REGISTER | 1 byte register response flag:
if flag is set to 0 SUCCESS it menas user is successfully registered, user must send a login request to continue \
if flag is set to 1 it means the username is already taken, user must send a new register request with a different username \
if flag is set to 2 it means the pass is insecure, the remainder of te message is now the password strength checker feedback ]

# Login request

Task type: 2

[usual header set flag to encrypted][1 byte task type set to 2 LOGIN |  4 bytes client request id | 1 byte username length | username | 1 byte password length | password]
maximum payload length is 1 (username length) + 1 (password length) + 255 (username max length) + 255 (password max length) = 512 bytes longer requests will be regarded as INVALIDREQUEST and will be dropped

#Login response

Message type: 5
[usual header set flag to encrypted][1 byte message type set to 5 LOGIN | 1 byte login response flag:
if flag is set to 0 SUCCESS it menas user is successfully logged in and can start sending requests to the server \
if flag is set to 1 i means that the password that was given is wrong, user is required to send the correct password \
if flag is set to 2 it means that the user that has requested to be logged in isn't registered in the server]


# Search for ticker request

Task type: 6

[usual header set flag to encrypted][1 byte task type set to 6 SEARCHTICKER | 4 bytes client request id | 1 byte length of search word | the word that is searched | 1 byte 0 or 1 to toggle fast search (isFast) | 1 byte limit | 4 bytes id of last seen ticker for pagination]
* if isFast is true then the limit is capped at 5 and anything tha comes after this flag won't be read, isFast search is of client can choose how many results to get per request (up to 255) and is required to set the last ticker on his list for faster lookup time because the server uses pagination, the client should use the id of the ticker and not the name of symbol


# Search ticker response

Message Type: 6

[usual header set flag to encrypted][1 byte message type to 6 SEARCHTICKER | 4 bytes client request id | 1 byte count | [count] tickers in this format: 1 byte ticker name length | ticker name | 1 byte symbol length | symbol | 4 bytes ticker id network byte order]

# Tickers database structure

creation query: 

CREATE TABLE IF NOT EXISTS tickers 
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL UNIQUE,
            name TEXT NOT NULL,
            exchange TEXT,        # e.g., NASDAQ, NYSE, BINANCE
            asset_type TEXT,      # e.g., STOCK, CRYPTO, FOREX
            sector TEXT,          # e.g., Technology, Energy
            is_active INTEGER DEFAULT 1


# Get Ticker information

Task type: 7

send a symbol get: ticker name, exchange, asset type and sector (if they do not length will be 0)

[usual header set flag to encrypted][1 byte Task type set to 7 TICKERINFO | 4 bytes client request id | 1 byte symbol length | symbol]


# Get Ticker Information response

Message type: 7

[usual header set flag to encrypted][1 byte Message type set to 7 TICKERINFO | 4 bytes client request id | 1 byte name length | name | 1 byte exchange length | exchange | 1 byte asset type according to enum | 1 byte sector length | sector]

if name length is 0 it means the symbol doesn't exist in the database


# Get Balance request

Task type: 8

client uses this to get its updated balance

[usual header set flag to encrypted][1 byte Task type set to 8 GETBALANCE | 4 bytes request id]


# Get Balance response

Message type: 8

[usual header set flag to encrypted][1 byte Message type set to 8 GETBALANCE | 4 bytes client request id |8 bytes balance as a double]


# Send Order

Task type: 9

[usual header set flag to encrypted][1 byte Task type set to 9 SENDORDER | 4 bytes request id | 1 byte order type | 1 byte symbol length | symbol | 4 bytes quantity | 8 bytes asking price as double | 1 byte password length | password ]

Order types: 0 Long, 1 Short

server will check the updated price of the symbol when proccessing the order, if the asking price isn't within 1% range of the updated price the server will not commit the order (prevents slippage)

the client will need to send its password in order to authenticate every action

# Order responses:

Message type: 9 ORDERCOMMITED

[usual header set flag to encrypted][1 byte Message type set to 9 ORDERCOMMITED | 4 bytes client request i | 8 bytes paid price per unit in double | 8 bytes the commit ts in seconds | 4 bytes order id]

if order was commited the server will return the price payed for each unit, the srver will calculate the clients new balance and the client will do the same on its side.

Message type: 10 INVALIDORDER

[usual header set flag to encrypted][1 byte Message type set to 10 INVALIDORDER | 4 bytes client request i | 1 byte status]

statuses:
	INVALIDBALANCE 0, not enough money in users balance to execute the order
	INVALIDSYMBOL 1, symbol doesnt exist in the server's database or isn't available for orders

Message type: 11 ORDERSLIPPED

[usual header set flag to encrypted][1 byte Message type set to 11 ORDERSLIPPED | client request id]

means the asking price is not within 1% range of the market price, order isn't executed to prevent slippage


# Fetch User Orders
Task type: 10

Requests a list of orders associated with the current user. Filters can be applied for specific symbols or views (Active/Inactive).

Request Layout:
[usual header set flag to encrypted]
[1 byte Task type set to 10 FETCH_ORDERS | 4 bytes client request id | 1 byte symbol length (0 for all) | symbol (optional) | 1 byte view filter (0: All, 1: Active, 2: Inactive) | 4 bytes limit | 4 bytes offset]

# Fetch User Orders Response
Message type: 12

Returns a serialized list of orders. If the user has no orders, the order count will be 0.

Response Layout:
[usual header set flag to encrypted]
[1 byte Message type set to 12 FETCH_ORDERS | 4 bytes client request id | 4 bytes order count (N)]

Followed by N instances of the Order Block:
[1 byte symbol length | symbol | 4 bytes order id | 1 byte order type (0: LONG, 1: SHORT) | 8 bytes entry price (double) | 8 bytes entry ts (uint64) | 4 bytes quantity | 1 byte is active (1: active, 0: inactive)]

If and only if is active is 0 (Inactive), the following bytes are appended to that specific order block:
[8 bytes end price (double) | 8 bytes end ts (uint64)]

# End Order 
Task type: 11

Request for user to exit an existing order and take Profit/Loss

[usual header set flag to encrypted]
[1 byte task type 11 ENDORDER | 4 bytes client request id | 4 bytes order id | 8 bytes expected price as double | 1 byte password length | password]

# End Order response

Message type: 13 ORDEREXISTTED
order successfully ended and profit/loss taken

[usual header set flag to encrypted][1 byte message type set to 13 ORDEREXISTTED | 4 bytes client request id | 8 bytes double end_price | 8 bytes end ts]

Message type: 14 ORDERFAILEDEXIT
order failed to exit for a specfic reason
[usual header set flag to encrypted][1 byte message type set to 14 ORDERFAILEDEXIT | 4 bytes client request id | 1 byte status]
Statuses:
0 ORDERNOTEXIST order id doesnt match to an existing active order for the user
1 EXPECTEDPRICEOUTOFRANGE the actual prive slipped out of the expected price range, the order wasnt exsited to prevent unwanted behaviour

# Watchlist API
---

## Standard Status Codes (1 Byte)
The following status codes are returned in the `Status` field of most response messages:

| Status | Label | Description |
| :--- | :--- | :--- |
| `0` | **SUCCESS** | Operation completed successfully. |
| `1` | **NOT_FOUND** | The specified list or ticker was not found. |
| `2` | **DUPLICATE** | List name already exists or Ticker is already in the list. |
| `3` | **INVALID_TICKER** | Ticker does not exist in the Global Master Tickers database. |
| `4` | **DB_ERROR** | Internal database failure or SQL constraint violation. |
| `5` | **UNAUTHORIZED** | User is not logged in or session has expired. |

---

## 1. Fetch All Watchlists
**Task Type: 12 (FETCH_WATCHLISTS)** Requests a summary of all lists owned by the authenticated user.

### Client-to-Server (Request)
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `TaskType` | uint8 | Set to `12` |
| 1 | 4 | `ReqID` | uint32 | Client-generated request ID (Network Order) |

### Server-to-Client (Response)
**Message Type: 15 (WATCHLIST_SUMMARY)**
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `MsgType` | uint8 | Set to `15` |
| 1 | 4 | `ReqID` | uint32 | Matching Request ID |
| 5 | 1 | `Status` | uint8 | Status code (0, 4, or 5) |
| 6 | 1 | `Count` | uint8 | Number of lists (N). 0 if status != 0 |
| 7 | Var | `ListBlocks` | Byte[] | N instances of the **List Block** |

**List Block Structure:**
`[4B ListID (Network Order)][1B NameLen][Name String]`

---

## 2. Manage Watchlist Entities
**Task Type: 13 (MANAGE_WATCHLIST)** Handles list creation, renaming, and deletion.

### Client-to-Server (Request)
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `TaskType` | uint8 | Set to `13` |
| 1 | 4 | `ReqID` | uint32 | Client-generated request ID |
| 5 | 1 | `Action` | uint8 | `0: Create`, `1: Rename`, `2: Delete` |
| 6 | 1 | `NameLen` | uint8 | Length of the list name |
| 7 | Var | `Name` | String | Current/Original list name |
| ? | 1 | `NewNameLen`| uint8 | *Action 1 only:* Length of new name |
| ? | Var | `NewName` | String | *Action 1 only:* New name string |

### Server-to-Client (Response)
**Message Type: 16 (WATCHLIST_ACTION_STATUS)**
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `MsgType` | uint8 | Set to `16` |
| 1 | 4 | `ReqID` | uint32 | Matching Request ID |
| 5 | 1 | `Status` | uint8 | See Status Table (Success, Not Found, Duplicate, etc.) |

---

## 3. Modify Watchlist Contents
**Task Type: 14 (MODIFY_WATCHLIST_ITEMS)** Adds or removes specific tickers from a list.

### Client-to-Server (Request)
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `TaskType` | uint8 | Set to `14` |
| 1 | 4 | `ReqID` | uint32 | Client-generated request ID |
| 5 | 1 | `Action` | uint8 | `0: Add`, `1: Remove` |
| 6 | 1 | `ListNmLen` | uint8 | Length of list name |
| 7 | Var | `ListName` | String | Target list name |
| ? | 1 | `SymLen` | uint8 | Length of ticker symbol |
| ? | Var | `Symbol` | String | The ticker symbol (e.g. "AAPL") |

### Server-to-Client (Response)
**Message Type: 16 (WATCHLIST_ACTION_STATUS)**
(Matches structure in Section 2)

---

## 4. Get Watchlist Content
**Task Type: 15 (GET_WATCHLIST_CONTENT)** Fetches the full list of symbols for a specific watchlist.

### Client-to-Server (Request)
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `TaskType` | uint8 | Set to `15` |
| 1 | 4 | `ReqID` | uint32 | Client-generated request ID |
| 5 | 1 | `NameLen` | uint8 | Length of list name |
| 6 | Var | `Name` | String | List name string |
| ?| Offset | Int (4B) | The starting index for pagination (Big Endian) |
| ? + 4| Limit | Int (4B) | Maximum number of items to return (Big Endian) |

### Server-to-Client (Response)
**Message Type: 17 (WATCHLIST_CONTENT)**
| Offset | Size | Field | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 1 | `MsgType` | uint8 | Set to `17` |
| 1 | 4 | `ReqID` | uint32 | Matching Request ID |
| 5 | 1 | `Status` | uint8 | 0 if found, 1 if list missing, 5 if unauthorized |
| 6 | 1 | `Count` | uint8 | Number of tickers (N). 0 if status != 0 |
| 7 | Var | `Symbols` | Byte[] | N instances of Symbol Block |

**Symbol Block Structure:**
`[1B SymbolLen][Symbol String]`