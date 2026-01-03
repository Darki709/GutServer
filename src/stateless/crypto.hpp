#ifndef CRYPTO_HPP
#define CRYPTO_HPP
#include "../libraries.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>


namespace Gut
{
	class AESGCM
	{
	private:
		EVP_CIPHER_CTX* ctx;
		SessionKey key{};

	public:
		AESGCM();
		~AESGCM();
		
		void init(SessionKey &key);

		bool encrypt(
			uint64_t nonce,
			const uint8_t *plaintext,
			size_t len,
			std::vector<uint8_t> &out);

		bool decrypt(
			uint64_t nonce,
			const uint8_t *ciphertext,
			size_t len,
			std::vector<uint8_t> &out);

		SessionKey& getKey();
	};
}
#endif