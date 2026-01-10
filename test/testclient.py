import socket
import struct
import random
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# ================= CONFIG =================
SERVER_IP = "127.0.0.1"
SERVER_PORT = 6767

# Message Types (Client -> Server)
MSG_HANDSHAKE_HELLO = 0
MSG_TEST = 1
MSG_LOGIN_TASK = 2
MSG_REGISTER_TASK = 3
MSG_PRICE_REQUEST = 4
MSG_CANCEL_TICKER_STREAM = 5

# Message Types (Server -> Client)
MSG_HANDSHAKE_SUCCESS = 1
MSG_PRICE_RESPONSE = 2
MSG_PRICE_STREAM = 3
MSG_REGISTER_RESPONSE = 4
MSG_LOGIN_RESPONSE = 5

FLAG_PLAINTEXT = 0x00
FLAG_ENCRYPTED = 0x01

# Result Flags
REG_SUCCESS = 0
LOGIN_SUCCESS = 0

REQUEST_ID_COUNTER = random.randint(0, 1000000)
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
    nonce_bytes = nonce.to_bytes(12, "big")
    decryptor = Cipher(algorithms.AES(key), modes.GCM(nonce_bytes, tag)).decryptor()
    return decryptor.update(ciphertext) + decryptor.finalize()


def parse_server_response(response, aes_key=None):
    global recv_nonce
    flag = response[0]
    payload = response[1:]
    if flag == FLAG_ENCRYPTED:
        if aes_key is None:
            raise RuntimeError("Encrypted message received without AES key")
        payload = decrypt_aes_gcm(aes_key, recv_nonce, payload)
        recv_nonce += 1
    msg_type = payload[0]
    body = payload[1:]
    print(f"[RECV] MsgType={msg_type} BodyLen={len(body)} Nonce={recv_nonce - 1 if flag else 'N/A'}")
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


# --- Send GCM message ---
def send_gcm_message(sock, aes_key, msg_type, reqid, payload):
    global send_nonce
    # Format: [Type][ReqID][Payload]
    inner_payload = struct.pack("!BI", msg_type, reqid) + payload
    ciphertext = encrypt_aes_gcm(aes_key, send_nonce, inner_payload)
    total_len_val = 1 + len(ciphertext)
    packet = struct.pack("!I", total_len_val) + struct.pack("!B", FLAG_ENCRYPTED) + ciphertext
    sock.sendall(packet)
    print(f"[SEND] GCM MsgType={msg_type} ReqID={reqid} SendNonce={send_nonce}")
    send_nonce += 1


# --- Task Requests ---
def send_register_request(sock, aes_key, username, password):
    u_bytes, p_bytes = username.encode(), password.encode()
    payload = struct.pack(f"B{len(u_bytes)}sB{len(p_bytes)}s", len(u_bytes), u_bytes, len(p_bytes), p_bytes)
    return send_gcm_message(sock, aes_key, MSG_REGISTER_TASK, next_reqid(), payload)


def send_login_request(sock, aes_key, username, password):
    u_bytes, p_bytes = username.encode(), password.encode()
    payload = struct.pack(f"B{len(u_bytes)}sB{len(p_bytes)}s", len(u_bytes), u_bytes, len(p_bytes), p_bytes)
    return send_gcm_message(sock, aes_key, MSG_LOGIN_TASK, next_reqid(), payload)


def build_price_request(symbol: str, interval: str):
    u_symbol = symbol.encode()
    payload = struct.pack("B", len(u_symbol)) + u_symbol
    interval_sec = {"1m": 60, "5m": 300, "15m": 900, "1h": 3600, "1d": 86400}[interval]
    # [4B interval][8B start][8B end][1B flags]
    payload += struct.pack("!IQQB", interval_sec, 0, 0, 0x03)
    return payload


# --- Handshake Verify (Keep original ECB logic) ---
def send_handshake_verify_ecb(sock, aes_key):
    reqid = next_reqid()
    plaintext = b"encrypted"
    cipher = Cipher(algorithms.AES(aes_key), modes.ECB())
    encryptor = cipher.encryptor()
    pad_len = 16 - (len(plaintext) % 16)
    padded = plaintext + bytes([pad_len] * pad_len)
    ciphertext = encryptor.update(padded) + encryptor.finalize()
    total_len = len(ciphertext) + 6
    packet = struct.pack("!I", total_len) + struct.pack("!B", FLAG_PLAINTEXT) + struct.pack("!BI", MSG_TEST,
                                                                                            reqid) + ciphertext
    sock.sendall(packet)


CANDLE_FORMAT = "!QddddQ"  # 8B Timestamp, 8B Open, 8B High, 8B Low, 8B Close, 8B Volume
CANDLE_SIZE = 48


def parse_candles(body):
    CANDLE_SIZE = 48
    FORMAT = "!QddddQ"

    req_id, is_last, candle_count = struct.unpack("!IBH", body[:7])
    print(f"--- Snapshot Received ---")
    print(f"Request ID: {req_id}")
    print(f"Is Final Packet: {bool(is_last)}")
    print(f"Candle Count: {candle_count}")
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

    req_id = struct.unpack("!I", body[0:4])[0]
    print(f"--- Stream Update (ReqID: {req_id}) ---")

    # Slice the body to get just the candle data
    candle_data = body[4:]

    # Loop through the remaining data
    for i in range(0, len(candle_data), CANDLE_SIZE):
        # Ensure we have a full 48 bytes before attempting to unpack
        if i + CANDLE_SIZE > len(candle_data):
            break

        ts, open_, high, low, close, volume = struct.unpack(
            FORMAT, candle_data[i: i + CANDLE_SIZE]
        )

        print(
            f"  TS={ts} | O={open_:.2f} | H={high:.2f} | L={low:.2f} | C={close:.2f} | Vol={volume}"
        )


# ================= MAIN =================
def main():
    global send_nonce, recv_nonce
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    # Use unique username for testing
    test_user = "oer"
    test_pass = "Secureass123!"

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((SERVER_IP, SERVER_PORT))
        print(f"[CLIENT] Connected. Testing with user: {test_user}")

        # 1. Handshake
        sock.sendall(create_handshake_packet(private_key))
        resp_hdr = sock.recv(5)
        resp_body = sock.recv(struct.unpack("!I", resp_hdr[:4])[0] - 1)
        aes_key = private_key.decrypt(resp_body[1:],
                                      padding.OAEP(mgf=padding.MGF1(hashes.SHA1()), algorithm=hashes.SHA1(),
                                                   label=None))

        # 2. Verify Handshake
        send_handshake_verify_ecb(sock, aes_key)
        hdr = sock.recv(4)
        msg_type, body = parse_server_response(sock.recv(struct.unpack("!I", hdr)[0]), aes_key)

        if msg_type != MSG_HANDSHAKE_SUCCESS:
            print("Handshake failed")
            return

        # 3. Register
        send_register_request(sock, aes_key, test_user, test_pass)
        #send_login_request(sock, aes_key, test_user, test_pass)

        while True:
            header = sock.recv(4)
            if not header: break
            total_len = struct.unpack("!I", header)[0]
            msg_type, body = parse_server_response(sock.recv(total_len), aes_key)

            # Handle Register Response
            if msg_type == MSG_REGISTER_RESPONSE:
                if body[0] == REG_SUCCESS:
                    print("[SUCCESS] Registered. Now Logging in...")
                    send_login_request(sock, aes_key, test_user, test_pass)
                else:
                    print(f"[FAIL] Registration flag: {body[0]}")

            # Handle Login Response
            elif msg_type == MSG_LOGIN_RESPONSE:
                if body[0] == LOGIN_SUCCESS:
                    print("[SUCCESS] Logged in! Requesting Price Stream for TSLA...")
                    payload = build_price_request("BTC-USD", "15m")
                    send_gcm_message(sock, aes_key, MSG_PRICE_REQUEST, next_reqid(), payload)
                else:
                    print(f"[FAIL] Login flag: {body[0]}")

            # Handle Data
            elif msg_type == MSG_PRICE_STREAM:
                parse_candles_s(body)

            elif msg_type == MSG_PRICE_RESPONSE:
                parse_candles(body)



if __name__ == "__main__":
    main()