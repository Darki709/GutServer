#include "HandshakeHello.hpp"
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/rand.h>


Gut::HandShakeHello::HandShakeHello(std::shared_ptr<Client> &client, uint32_t reqId  , String clientpublickey)
	: Task(client, reqId), clientPublicKey(std::move(clientpublickey))
{
	std::cout << "handshakehello started" << std::endl;
}

std::optional<Gut::Message> Gut::HandShakeHello::execute()
{
	// check client is still valid
	std::shared_ptr<Client> client;
	if ((client = Task::getClient()) == nullptr)
	{
		throw CLIENTNOTFOUND;
	}

	// generate aes-256 session keys
	Gut::SessionKey sessionKey;
	if (RAND_bytes(sessionKey.data(), sessionKey.size()) != 1)
	{
		throw std::runtime_error("Failed to generate random AES key");
	}

	// Store session key in the client for symmetric encryption
	client->getCipher().cipher.init(sessionKey);

	// load client's public rsa key
	BIO *bio = BIO_new_mem_buf(clientPublicKey.data(), static_cast<int>(clientPublicKey.size()));
	if (!bio)
		throw std::runtime_error("Failed to create BIO for RSA key");

	RSA *rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
	BIO_free(bio);
	if (!rsa)
		throw std::runtime_error("Failed to parse client RSA public key");

	// encrypt the session key with rsa
	std::vector<unsigned char> encryptedKey(RSA_size(rsa));
	int len = RSA_public_encrypt(
		static_cast<int>(sessionKey.size()),
		sessionKey.data(),
		encryptedKey.data(),
		rsa,
		RSA_PKCS1_OAEP_PADDING);
	RSA_free(rsa);
	if (len == -1)
		throw std::runtime_error("RSA encryption failed");

	// advance client handshake state
	client->setState(ClientState::HANDSHAKE_VERIFY);

	// build the response message
	String content;
	// append message type
	content.push_back(static_cast<char>(MsgType::HANDSHAKEVERIFY));
	// append encrypted key
	content.append(reinterpret_cast<const char *>(encryptedKey.data()), len);
	std::cout << "AES-256 Key: ";
	for (auto b : sessionKey)
	{
		std::cout << std::hex << std::setw(2) << std::setfill('0')
				  << static_cast<int>(b) << " ";
	}
	std::cout << std::endl;
	Message message(std::move(content), client->getSocket());
	return message;
}