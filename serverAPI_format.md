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
