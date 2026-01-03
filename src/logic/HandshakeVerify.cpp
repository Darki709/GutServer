#include "HandshakeVerify.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <iomanip>

Gut::HandShakeVerify::HandShakeVerify (std::shared_ptr<Client>& client, uint32_t reqId, String encryptedMessage)
    : Task(client, reqId), encryptedMessage(std::move(encryptedMessage)) {
		//check client privileges
		if(static_cast<int>(client->getState()) < 1) throw Errors::ILLEGALACCESS;
		std::cout << "handshakeverify started" << std::endl;
	}

std::optional<Gut::Message> Gut::HandShakeVerify::execute(){
	std::shared_ptr<Client> client;
	if((client = Task::getClient()) == nullptr){
		throw CLIENTNOTFOUND;
	}

	AESGCM& cipher = client->getCipher().cipher;
    auto& key = cipher.getKey(); // raw 32-byte AES key
	for (auto b : key)
	{
		std::cout << std::hex << std::setw(2) << std::setfill('0')
				  << static_cast<int>(b) << " ";
	}
	std::cout << std::endl;

    // 1. Prepare OpenSSL context for AES-256-ECB
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create EVP_CIPHER_CTX");

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key.data(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("AES init failed");
    }

    // 2. Decrypt client message
    std::vector<uint8_t> plaintext(encryptedMessage.size());
    int outLen1 = 0;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &outLen1,
                          reinterpret_cast<const uint8_t*>(encryptedMessage.data()),
                          static_cast<int>(encryptedMessage.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw HANDSHAKEFAILURE;
    }

    int outLen2 = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen1, &outLen2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw HANDSHAKEFAILURE;
	}

    EVP_CIPHER_CTX_free(ctx);

    // 3. Compare decrypted message with the fixed word
    String decrypted(reinterpret_cast<char*>(plaintext.data()), outLen1 + outLen2);
    if (decrypted != "encrypted") {
        throw HANDSHAKEFAILURE;
    }

    // 4. Handshake successful → initialize AESGCM nonces for the secure tunnel and sets flags on client object
    client->startTunnel(); 

	//==============================================
	//temporary bypass of login step for development
	client->setState(ClientState::AUTHENTICATED);
	//==============================================
	//remove when login and register task are completed


    // 5. Send confirmation message
    String content;
    content.push_back(static_cast<char>(MsgType::HANDSHAKESUCCESS));
	std::cout << content << std::endl;
    Message msg(std::move(content), client->getSocket());
    return msg;
}	