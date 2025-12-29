import socket
import struct
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# --- Configuration ---
SERVER_IP = "127.0.0.1"
SERVER_PORT = 4213

MSG_HANDSHAKE_HELLO = 0
MSG_TEST = 1
MSG_HANDSHAKE_SUCCESS = 2

FLAG_PLAINTEXT = 0x00
FLAG_ENCRYPTED = 0x01

REQUEST_ID_COUNTER = 1000

send_nonce = 0
recv_nonce = 0


# --- Utility Functions ---
def next_reqid():
    global REQUEST_ID_COUNTER
    REQUEST_ID_COUNTER += 1
    return REQUEST_ID_COUNTER


def encrypt_aes_ecb(key: bytes, plaintext: bytes) -> bytes:
    cipher = Cipher(algorithms.AES(key), modes.ECB())
    encryptor = cipher.encryptor()
    pad_len = 16 - (len(plaintext) % 16)
    padded = plaintext + bytes([pad_len] * pad_len)
    return encryptor.update(padded) + encryptor.finalize()


from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

def decrypt_aes_gcm(key: bytes, nonce: int, payload: bytes) -> bytes:
    """
    Decrypts AES-256-GCM payload where the last 16 bytes are the tag.

    :param key: 32-byte AES key
    :param nonce: numeric nonce (incremented per message)
    :param payload: ciphertext + 16-byte GCM tag
    :return: decrypted plaintext
    """
    if len(payload) < 16:
        raise ValueError("Payload too short to contain GCM tag")

    # Split ciphertext and tag
    ciphertext, tag = payload[:-16], payload[-16:]

    # Convert numeric nonce to 12-byte GCM nonce
    nonce_bytes = nonce.to_bytes(12, "big")

    decryptor = Cipher(
        algorithms.AES(key),
        modes.GCM(nonce_bytes, tag)
    ).decryptor()

    plaintext = decryptor.update(ciphertext) + decryptor.finalize()
    return plaintext



def parse_server_response(response_data, aes_key=None):
    global recv_nonce

    total_len = struct.unpack("!I", response_data[:4])[0]
    flag = response_data[4]
    payload = response_data[5:]

    print("-" * 50)
    print(f"[RECV] total_len={total_len} flag={flag} payload_len={len(payload)}")

    if flag == FLAG_ENCRYPTED:
        if aes_key is None:
            raise RuntimeError("Encrypted message received without AES key")

        print(f"[RECV] decrypting with recv_nonce={recv_nonce}")
        payload = decrypt_aes_gcm(aes_key, recv_nonce,payload)
        recv_nonce += 1
        print(f"[RECV] recv_nonce incremented -> {recv_nonce}")

    msg_type = payload[0]
    body = payload[1:]

    print(f"[RECV] msg_type={msg_type}")
    print(f"[RECV] body(hex first 32)={body[:32].hex()}")
    print("-" * 50)

    return flag, msg_type, body


# --- Handshake Packet ---
def create_handshake_packet(private_key):
    public_pem = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )

    reqid = next_reqid()
    payload = struct.pack("!BI", MSG_HANDSHAKE_HELLO, reqid) + public_pem

    total_len_val = 1 + len(payload)
    return struct.pack("!I", total_len_val) + struct.pack("!B", FLAG_PLAINTEXT) + payload


# --- Send AES message ---
def send_aes_message(sock, aes_key, msg_type, plaintext):
    global send_nonce

    reqid = next_reqid()
    ciphertext = encrypt_aes_ecb(aes_key, plaintext)

    payload = struct.pack("!BI", msg_type, reqid) + ciphertext
    total_len_val = 1 + len(payload)

    # plaintext flag for now
    packet = struct.pack("!I", total_len_val) + struct.pack("!B", FLAG_PLAINTEXT) + payload
    sock.sendall(packet)

    print(f"[SEND] msgType={msg_type} reqId={reqid} flag=PLAINTEXT send_nonce={send_nonce}")
    # send_nonce NOT incremented because flag != FLAG_ENCRYPTED


def main():
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            print(f"Connecting to {SERVER_IP}:{SERVER_PORT}...")
            sock.connect((SERVER_IP, SERVER_PORT))

            # --- HandshakeHello ---
            sock.sendall(create_handshake_packet(private_key))
            print("[CLIENT] HandshakeHello sent")

            response_data = sock.recv(4096)
            if not response_data:
                print("[!] Server closed")
                return

            # --- Receive AES key (plaintext server message) ---
            encrypted_payload = response_data[6:]  # skip len + flag + msgType

            aes_key = private_key.decrypt(
                encrypted_payload,
                padding.OAEP(
                    mgf=padding.MGF1(algorithm=hashes.SHA1()),
                    algorithm=hashes.SHA1(),
                    label=None
                )
            )
            print(f"[CLIENT] AES key established: {aes_key.hex()}")

            # --- Send verify ---
            send_aes_message(sock, aes_key, MSG_TEST, b"encrypted")

            # --- Receive verify result ---
            response = sock.recv(4096)
            if response:
                _, msg_type, _ = parse_server_response(response, aes_key)

                if msg_type in (MSG_TEST, MSG_HANDSHAKE_SUCCESS):
                    print("[SUCCESS] Handshake completed")

    except Exception as e:
        print(f"[!] Error: {e}")


if __name__ == "__main__":
    main()
