import socket
import struct
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# ================= CONFIG =================
SERVER_IP = "127.0.0.1"
SERVER_PORT = 6767

MSG_HANDSHAKE_HELLO = 0
MSG_TEST = 1
MSG_HANDSHAKE_SUCCESS = 1
MSG_PRICE_REQUEST = 4
MSG_PRICE_RESPONSE = 2
MSG_PRICE_STREAM = 3
MSG_CANCEL_TICKER_STREAM = 5

FLAG_PLAINTEXT = 0x00
FLAG_ENCRYPTED = 0x01

REQUEST_ID_COUNTER = 1000
send_nonce = 0
recv_nonce = 0

# ================ UTILITIES =================
def next_reqid():
    global REQUEST_ID_COUNTER
    REQUEST_ID_COUNTER += 1
    return REQUEST_ID_COUNTER

def encrypt_aes_gcm(key: bytes, nonce: int, plaintext: bytes) -> bytes:
    nonce_bytes = nonce.to_bytes(12, "big")
    encryptor = Cipher(algorithms.AES(key), modes.GCM(nonce_bytes)).encryptor()
    ciphertext = encryptor.update(plaintext) + encryptor.finalize()
    return ciphertext + encryptor.tag

def decrypt_aes_gcm(key: bytes, nonce: int, payload: bytes) -> bytes:
    if len(payload) < 16:
        raise ValueError("Payload too short for GCM tag")
    ciphertext, tag = payload[:-16], payload[-16:]
    print(f"[DEBUG] ciphertext_len={len(ciphertext)} tag={tag.hex()} nonce={nonce}")
    nonce_bytes = nonce.to_bytes(12, "big")
    decryptor = Cipher(algorithms.AES(key), modes.GCM(nonce_bytes, tag)).decryptor()
    return decryptor.update(ciphertext) + decryptor.finalize()

def parse_server_response(response, aes_key=None):
    global recv_nonce
    flag = response[0]
    print(flag)
    payload = response[1:]
    print(len(payload))
    if flag == FLAG_ENCRYPTED:
        if aes_key is None:
            raise RuntimeError("Encrypted message received without AES key")
        print(f"[DEBUG] flag={flag} payload_len={len(payload)} recv_nonce={recv_nonce}")
        payload = decrypt_aes_gcm(aes_key, recv_nonce, payload)
        recv_nonce += 1
    msg_type = payload[0]
    print(msg_type)
    body = payload[1:]
    print(f"[RECV] Flag={flag} MsgType={msg_type} PayloadLen={len(body)} RecvNonce={recv_nonce-1 if flag else 'N/A'}")
    return msg_type, body

# --- Handshake ---
def create_handshake_packet(private_key):
    public_pem = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    reqid = next_reqid()
    payload = struct.pack("!BI", MSG_HANDSHAKE_HELLO, reqid) + public_pem
    total_len_val = 1 + len(payload)
    return struct.pack("!I", total_len_val) + struct.pack("!B", FLAG_PLAINTEXT) + payload

# --- Send plaintext (for handshake verify) ---
def send_plaintext(sock, msg_type, reqid, payload):
    total_len_val = 1 + 1 + 4 + len(payload)  # flag + type + reqid + payload
    packet = struct.pack("!I", total_len_val)
    packet += struct.pack("!B", FLAG_PLAINTEXT)
    packet += struct.pack("!B", msg_type)
    packet += struct.pack("!I", reqid)
    packet += payload
    sock.sendall(packet)
    print(f"[SEND] Plaintext MsgType={msg_type} ReqID={reqid}")

# --- Send GCM message ---
def send_gcm_message(sock, aes_key, msg_type, reqid, payload):
    global send_nonce
    ciphertext = encrypt_aes_gcm(aes_key, send_nonce, struct.pack("!BI", msg_type, reqid) + payload)
    total_len_val = 1 + len(ciphertext)
    packet = struct.pack("!I", total_len_val) + struct.pack("!B", FLAG_ENCRYPTED) + ciphertext
    sock.sendall(packet)
    print(f"[SEND] GCM MsgType={msg_type} ReqID={reqid} SendNonce={send_nonce}")
    send_nonce += 1

# --- Build price request payload ---
def build_price_request(symbol: str, interval: str):
    payload = struct.pack("B", len(symbol)) + symbol.encode()
    interval_sec = {
        "1m": 60,
        "5m": 300,
        "15m": 900,
        "1h": 3600,
        "1d": 86400
    }[interval]
    payload += struct.pack("!I", interval_sec)
    payload += struct.pack("!Q", 0)  # start_ts
    payload += struct.pack("!Q", 0)  # end_ts
    payload += struct.pack("B", 0x02)  # flags = SNAPSHOT
    return payload

# --- Parse candles ---
import struct

def parse_candles(body):
    CANDLE_SIZE = 48
    FORMAT = "!QddddQ"

    reqId = body[:3]
    print(reqId)
    isLast = body[4]
    print(isLast)
    candleCount = body[5:6]
    print(candleCount)
    body = body[7:]
    print(len(body))

    for i in range(0, len(body), CANDLE_SIZE):
        ts, open_, high, low, close, volume = struct.unpack(
            FORMAT, body[i:i + CANDLE_SIZE]
        )

        print(
            f"Timestamp={ts} "
            f"Open={open_} "
            f"High={high} "
            f"Low={low} "
            f"Close={close} "
            f"Volume={volume}"
        )

def parse_candles_s(body):
    CANDLE_SIZE = 48
    FORMAT = "!QddddQ"

    reqId = body[:3]
    print(reqId)
    body = body[4:]
    print(len(body))

    for i in range(0, len(body), CANDLE_SIZE):
        ts, open_, high, low, close, volume = struct.unpack(
            FORMAT, body[i:i + CANDLE_SIZE]
        )

        print(
            f"Timestamp={ts} "
            f"Open={open_} "
            f"High={high} "
            f"Low={low} "
            f"Close={close} "
            f"Volume={volume}"
        )


def send_handshake_verify_ecb(sock, aes_key):
    """
    Send handshake verify message encrypted with AES-256-ECB.
    This message is plaintext flag (0x00) for the protocol.
    """
    reqid = next_reqid()
    msg_type = MSG_TEST
    plaintext = b"encrypted"

    # ECB encrypt
    cipher = Cipher(algorithms.AES(aes_key), modes.ECB())
    encryptor = cipher.encryptor()
    # Pad to 16 bytes
    pad_len = 16 - (len(plaintext) % 16)
    padded = plaintext + bytes([pad_len] * pad_len)
    ciphertext = encryptor.update(padded) + encryptor.finalize()

    # flag = 0x00 (plaintext according to protocol)
    total_len = len(ciphertext) + 6
    packet = (struct.pack("!I", total_len) + struct.pack("!B", FLAG_PLAINTEXT)
              + struct.pack("!BI", msg_type, reqid) + ciphertext)
    sock.sendall(packet)
    print(f"[SEND] Handshake verify (AES-ECB, plaintext flag) msgType={msg_type} reqId={reqid}")


def cancel_ticker_stream(sock, aes_key, original_req_id, symbol: str):
    """
    Sends a request to stop a specific ticker stream.
    :param sock: The active socket
    :param aes_key: The established AES key
    :param original_req_id: The reqId used when you STARTED the stream
    :param symbol: The ticker symbol (e.g., "TSLA")
    """
    # Generate a new unique ID for this cancellation request itself
    cancel_req_id = next_reqid()

    # 1 byte (Type) + 4 bytes (Cancel ReqID) + 4 bytes (Original ReqID) + 1 byte (Len)
    # The Task Type is actually handled inside send_gcm_message usually,
    # but based on your description, the body starts with the original_req_id.

    # Structure: [Original ReqID (4B)] + [Symbol Len (1B)] + [Symbol (NB)]
    payload = struct.pack("!I", original_req_id)
    payload += struct.pack("B", len(symbol))
    payload += symbol.encode('utf-8')

    # send_gcm_message will automatically prepend MSG_CANCEL_TICKER_STREAM (1B)
    # and the new cancel_req_id (4B) to this payload before encrypting.
    send_gcm_message(sock, aes_key, MSG_CANCEL_TICKER_STREAM, cancel_req_id, payload)

    print(f"[CLIENT] Requested cancellation for {symbol} (Orig ID: {original_req_id})")
    return cancel_req_id

# ================= MAIN =================
def main():
    global send_nonce, recv_nonce
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((SERVER_IP, SERVER_PORT))
        print("[CLIENT] Connected")

        # --- Handshake Hello ---
        sock.sendall(create_handshake_packet(private_key))
        response_data = sock.recv(8192)

        # --- AES key from server ---
        aes_key = private_key.decrypt(
            response_data[6:],  # skip len + flag + msgtype
            padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA1()), algorithm=hashes.SHA1(), label=None)
        )
        print(f"[CLIENT] AES key established: {aes_key.hex()}")

        # --- Handshake verify (plaintext!) ---
        send_handshake_verify_ecb(sock, aes_key)

        # --- Wait for handshake success ---
        len = sock.recv(4)
        len = struct.unpack("!I", len[:4])[0]
        response = sock.recv(len)
        msg_type, body = parse_server_response(response, aes_key)
        if msg_type != MSG_HANDSHAKE_SUCCESS:
            raise RuntimeError("Handshake failed")
        print("[CLIENT] Handshake completed")

        # --- Send first real message: price request (AES-GCM) ---
        stream_reqid = next_reqid()
        payload = build_price_request("TSLA", "5m")
        send_gcm_message(sock, aes_key, MSG_PRICE_REQUEST, stream_reqid, payload)

        message_count = 0
        # --- Receive and print price data ---
        while True:
            response = sock.recv(4)
            total_len = struct.unpack("!I", response[:4])[0]
            print(total_len)
            response = sock.recv(total_len)
            if not response:
                break
            msg_type, body = parse_server_response(response, aes_key)
            if msg_type == MSG_PRICE_RESPONSE:
                parse_candles(body)
            if msg_type == MSG_PRICE_STREAM:
                parse_candles_s(body)
                message_count += 1

            if message_count == 1:
                cancel_ticker_stream(sock, aes_key, stream_reqid, "TSLA")




if __name__ == "__main__":
    main()
