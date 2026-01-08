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
    - Client generates RSA keypair.
    - Sends: Outgoing Type 0 (HANDSHAKEHELLO), Request ID, client public key (PEM)
    - Flag: plaintext (0x00)

2. **Server Response: AES Key**
    - Server generates random AES-256 session key.
    - Encrypts session key with client’s RSA public key.
    - Sends message: [AES key] as payload
    - Flag: plaintext (0x00)
    - Incoming Type 0 (VERIFY)

3. **Client AES Session Initialization**
    - Client decrypts AES key with private RSA key.
    - AES key stored for future messages.
    - send_nonce = 0, recv_nonce = 0

4. **Handshake Verify / Test Message (Client → Server)**
    - Client encrypts [1B Outgoing MsgType + 4B ReqID + plaintext payload] with AES-GCM
    - Payload = ciphertext + 16B GCM tag
    - **Flag remains plaintext (0x00)** for this handshake step
    - Outgoing Type 1 (TEST / VERIFY)
    - Request ID: incremented

5. **Server Handshake Response**
    - Server decrypts client message using AES-256-GCM and recv_nonce
    - Validates message
    - Sends confirmation: Incoming Type 1 (HANDSHAKE_SUCCESS)
    - Payload: optional
    - Flag: encrypted (0x01)
    - recv_nonce incremented


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

The format follows the **core Gut protocol rules**: fixed binary layout, no delimiters, no text encoding.

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
- **Value:** `0x04`
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


#Stream response message

message type: 3 

[4 bytes length | is_encrypted flags 1 byte set to encrypted][1 byte message type 3 STREAM | 4 bytes requests id used by client to ask for the streaming | 8 bytes unix ts | 8 bytes open | 8 bytes high | 8 bytes low | 8 bytes close | 8 bytes volume]



#Cancel Ticker Stream

task type number: 5

[header as usual set to notify encrypted message][1 byte task type set to 5 CANCELTIKCERSTREAM |4 bytes client request id | 4 bytes request id of the original streaming request (network byte order) |1 bytes length of the symbol | the symbol in length bytes]