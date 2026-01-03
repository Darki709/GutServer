#include "crypto.hpp"

Gut::AESGCM::AESGCM() : ctx(nullptr) {}

Gut::AESGCM::~AESGCM()
{
	if (ctx)
		EVP_CIPHER_CTX_free(ctx);
}

// initialize the cipher context with a key
void Gut::AESGCM::init(SessionKey &key)
{
	this->key = key;
	if (ctx)
		EVP_CIPHER_CTX_free(ctx);
	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
}

// Encrypt a plaintext buffer with nonce
bool Gut::AESGCM::encrypt(
	uint64_t nonce,
	const uint8_t *plaintext,
	size_t len,
	std::vector<uint8_t> &out)
{
	if (!ctx)
		return false;

	out.clear();

	if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
		return false;

	// Create 12-byte IV: 4 bytes zeros + 8-byte nonce
	std::array<uint8_t, 12> iv{};
	uint64_t be_nonce = htonll(nonce);
	memcpy(iv.data() + 4, &be_nonce, sizeof(be_nonce));

	if (!EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()))
		return false;

	out.resize(len);
	int outlen = 0;

	if (!EVP_EncryptUpdate(ctx, out.data(), &outlen, plaintext, static_cast<int>(len)))
		return false;

	int tmplen = 0;
	if (!EVP_EncryptFinal_ex(ctx, out.data() + outlen, &tmplen))
		return false;

	outlen += tmplen;

	// Get the 16-byte GCM tag
	std::array<uint8_t, 16> tag{};
	if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()))
		return false;
	//std::cout << "aes key " << key.data() << std::endl;
	//std::cout << tag.data() << std::endl;
	out.insert(out.end(), tag.begin(), tag.end());
	return true;
}

// Decrypt ciphertext with nonce (last 16 bytes are tag)
bool Gut::AESGCM::decrypt(
	uint64_t nonce,
	const uint8_t *ciphertext,
	size_t len,
	std::vector<uint8_t> &out)
{
	if (!ctx || len < 16)
		return false;

	out.clear();
	size_t cipherlen = len - 16;
	const uint8_t *tag = ciphertext + cipherlen;

	if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
		return false;

	std::array<uint8_t, 12> iv{};
	uint64_t be_nonce = htonll(nonce);
	memcpy(iv.data() + 4, &be_nonce, sizeof(be_nonce));

	if (!EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()))
		return false;

	out.resize(cipherlen);
	int outlen = 0;

	if (!EVP_DecryptUpdate(ctx, out.data(), &outlen, ciphertext, static_cast<int>(cipherlen)))
		return false;

	// Set expected tag
	if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t *>(tag)))
		return false;

	int tmplen = 0;
	if (!EVP_DecryptFinal_ex(ctx, out.data() + outlen, &tmplen))
		return false;

	outlen += tmplen;
	out.resize(outlen);
	return true;
}

Gut::SessionKey& Gut::AESGCM::getKey(){
	return key;
}