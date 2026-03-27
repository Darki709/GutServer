#pragma once

#include "user_db_helper.hpp"
#include "db_utilities.hpp"

Gut::User_table::User_table() : Table_helper(){}

uint32_t Gut::User_table::authenticateUser(String username, String password)
{
	// retrive the users salt
	const char *querySalt = "SELECT salt FROM users WHERE username = ?";
	Gut::SecureStmt stmtSalt{nullptr};
	if (sqlite3_prepare_v2(db, querySalt, -1, &stmtSalt.stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error("can't fetch salt from db");
	sqlite3_bind_text(stmtSalt.stmt, 1, username.c_str(), -1, SQLITE_STATIC);

	if (sqlite3_step(stmtSalt.stmt) == SQLITE_ROW)
	{
		const unsigned char *saltRaw = sqlite3_column_text(stmtSalt.stmt, 0);
		String salt = "";
		if (saltRaw)
			salt = reinterpret_cast<const char *>(saltRaw);
		else
			return -2;
		// prepare to hash the password
		String secret = password + PEPPER;
		unsigned char hash[32];
		int iterations = 600000;

		// hash the password
		if (!PKCS5_PBKDF2_HMAC(secret.c_str(), secret.length(),
							   (unsigned char *)salt.c_str(), salt.length(),
							   iterations, EVP_sha256(), sizeof(hash), hash))
		{
			throw std::runtime_error("Password hashing failed.");
		}

		// convert hash to hex text
		std::stringstream shash;
		for (int i = 0; i < 32; i++)
		{
			// Formats each byte as 2-digit hex (e.g., 0a, ff)
			shash << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
		}
		String hashStr = shash.str();

		// retrieve the user id with username and hashed password
		const char* queryAuthenticate = "SELECT id FROM users WHERE username = ? AND password = ?";
		Gut::SecureStmt stmtAuth{nullptr};

		if (sqlite3_prepare_v2(db, queryAuthenticate, -1, &stmtAuth.stmt, nullptr) != SQLITE_OK)
			throw std::runtime_error("can't authenticate user");
		sqlite3_bind_text(stmtAuth.stmt, 1, username.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmtAuth.stmt, 2, hashStr.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmtAuth.stmt) == SQLITE_ROW){
			int id = (int)sqlite3_column_int64(stmtAuth.stmt, 0);
			return id;
		}
		else return -1;
	}
	else
		return -2;
}

int Gut::User_table::addUser(String username, String password)
{
	// first server checks that the password is secure enough, should happen in the client too
	String feedback;
	if (!isSecurePassword(password, feedback))
	{
		// delivers feedback to be passed to the user
		throw feedback;
	}

	// check that there is no other user with the same username
	const char *queryUsername = "SELECT COUNT(*) FROM users WHERE username = ?;";
	Gut::SecureStmt stmtUsername{nullptr};
	if (sqlite3_prepare_v2(db, queryUsername, -1, &stmtUsername.stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error("failed to check user uniqueness");

	sqlite3_bind_text(stmtUsername.stmt, 1, username.c_str(), -1, SQLITE_STATIC);
	int count = 0;
	if (sqlite3_step(stmtUsername.stmt) == SQLITE_ROW)
	{
		count = sqlite3_column_int(stmtUsername.stmt, 0);
	}
	else
		throw std::runtime_error("failed to check user uniqueness");

	if (count != 0)
		return -1;

	// finally we know that the new user's credentials are legal
	unsigned char buffer[SALT_SIZE];
	if (RAND_bytes(buffer, SALT_SIZE) != 1)
	{
		throw std::runtime_error("failed generate salt for password");
	}

	std::stringstream salt;
	for (int i = 0; i < SALT_SIZE; i++)
	{
		salt << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i];
	}

	String secret = password + PEPPER;
	unsigned char hash[32];
	int iterations = 600000;

	// salt as a hex string
	String saltStr = salt.str();

	// hash the password
	if (!PKCS5_PBKDF2_HMAC(secret.c_str(), secret.length(),
						   (unsigned char *)saltStr.c_str(), saltStr.length(),
						   iterations, EVP_sha256(), sizeof(hash), hash))
	{
		throw std::runtime_error("Password hashing failed.");
	}

	// convert hash to hex text
	std::stringstream shash;
	for (int i = 0; i < 32; i++)
	{
		// Formats each byte as 2-digit hex (e.g., 0a, ff)
		shash << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	}
	String hashStr = shash.str();

	const char *query = "INSERT INTO users  (username, password, salt) VALUES (?,?,?)";
	Gut::SecureStmt stmt{nullptr};
	if (sqlite3_prepare_v2(db, query, -1, &stmt.stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error("failed to prepare insert user");
	 sqlite3_bind_text(stmt.stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	 sqlite3_bind_text(stmt.stmt, 2, hashStr.c_str(), -1, SQLITE_TRANSIENT);
	 sqlite3_bind_text(stmt.stmt, 3, salt.str().c_str(), -1, SQLITE_TRANSIENT);
	int rc = sqlite3_step(stmt.stmt);
	if (rc == SQLITE_DONE)
	{
		std::cout << "user: " << username << " registered" << std::endl;
		return 0;
	}
	else if (rc == SQLITE_CONSTRAINT)
	{
		int err = sqlite3_extended_errcode(db);
		if (err == SQLITE_CONSTRAINT_UNIQUE)
		{
			return -1;
		}
		throw std::runtime_error("failed to insert new user to db" + String(sqlite3_errmsg(db)));
	}
	else
	{
		throw std::runtime_error("failed to insert new user to db" + String(sqlite3_errmsg(db)));
	}
}

bool Gut::User_table::isSecurePassword(const String &password, String &feedback)
{
	bool hasUpper = false;
	bool hasLower = false;
	bool hasDigit = false;
	bool hasSpecial = false;

	// 1. Length Check
	if (password.length() < 12)
	{
		feedback = "Password is too short. Use at least 12 characters.";
		return false;
	}

	// 2. Check for variety
	for (char c : password)
	{
		if (std::isupper(c))
			hasUpper = true;
		else if (std::islower(c))
			hasLower = true;
		else if (std::isdigit(c))
			hasDigit = true;
		else if (std::ispunct(c))
			hasSpecial = true;
	}

	// 3. Blocklist (Basic for now)
	std::vector<std::string> forbidden = {"password", "12345678", "qwertyuiop", "admin123"};
	std::string lowerPw = password;
	std::transform(lowerPw.begin(), lowerPw.end(), lowerPw.begin(), ::tolower);

	for (const auto &bad : forbidden)
	{
		if (lowerPw.find(bad) != std::string::npos)
		{
			feedback = "Password contains a forbidden common pattern.";
			return false;
		}
	}

	// 4. Final Validation Logic
	// We want all 4 categories to consider it "Secure"
	int score = hasUpper + hasLower + hasDigit + hasSpecial;

	if (score < 4)
	{
		feedback = "Add more variety (mix of letters and numbers).";
		return false;
	}

	return true;
}

double Gut::User_table::getBalance(uint32_t usrId){
	const char *query = "SELECT money FROM users WHERE id = ?";
	Gut::SecureStmt stmt{nullptr};
	if (sqlite3_prepare(db, query, -1, &stmt.stmt, nullptr) != SQLITE_OK){
		throw std::runtime_error("can't fetch balance from db");
	}
	sqlite3_bind_int(stmt.stmt, 1, usrId);
	if (sqlite3_step(stmt.stmt) == SQLITE_ROW){
		return sqlite3_column_double(stmt.stmt, 0);
	}
	else throw std::invalid_argument("no user exists with id: " + std::to_string(usrId));
}