#pragma once

#include "user_db_helper.hpp"

Gut::User_table::User_table()
{
	// get db connection
	//  get the db path
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::filesystem::path exePath(path);
	std::filesystem::path exeDir = exePath.parent_path();								// This is build/Debug/
	std::filesystem::path dbPath = exeDir.parent_path() / "database" / "stock_data.db"; // Move up one level from Debug to build, then into database
	std::string db_path = dbPath.string();

	// connect to db
	int rc = sqlite3_open(db_path.c_str(), &db);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
	}
	sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

	// make sure the table exists
	const char *query = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE, password TEXT NOT NULL, salt TEXT NOT NULL)";
	int rc = sqlite3_exec(db, query, nullptr, nullptr, nullptr);
	if (rc != SQLITE_OK)
	{
		std::cout << "cant create users table" << std::endl;
		throw Errors::FATALFAILURE;
	}
}

Gut::User_table::~User_table()
{
	// close db connection
	if (db)
	{
		sqlite3_close(db);
	}
}

int Gut::User_table::authenticateUser(String username, String password)
{
	// retrive the users salt
	const char *querySalt = "SELECT salt FROM users WHERE username = ?";
	sqlite3_stmt *stmtSalt;
	if (sqlite3_prepare_v2(db, querySalt, -1, &stmtSalt, nullptr) != SQLITE_OK)
		throw std::runtime_error("can't fetch salt from db");
	sqlite3_bind_text(stmtSalt, 1, username.c_str(), -1, SQLITE_STATIC);

	if (sqlite3_step(stmtSalt) == SQLITE_ROW)
	{
		const unsigned char *saltRaw = sqlite3_column_text(stmtSalt, 0);
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

		//retieve the user id of the username with the username and hashed password
		const char* queryAuthenticate = "SELECT id FROM users WHERE username = ? AND password = ?";

		sqlite3_stmt *stmtAuth;

		if(sqlite3_prepare_v2(db, queryAuthenticate, -1, &stmtAuth, nullptr) != SQLITE_OK) throw std::runtime_error("can't authenticate user");
		sqlite3_bind_text(stmtAuth, 1, username.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmtAuth, 2, hashStr.c_str(), -1, SQLITE_TRANSIENT);
		if(sqlite3_step(stmtAuth) == SQLITE_ROW){
			int id = sqlite3_column_int64(stmtAuth, 0);
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

	// check that the is no other user with the same username
	const char *query = "SELECT COUNT(*) FROM users WHERE username = ?;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK)
		throw std::runtime_error("failed to check user uniqueness");

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
	int count = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		count = sqlite3_column_int(stmt, 0);
	}
	else
		throw std::runtime_error("failed to check user uniqueness");

	sqlite3_finalize(stmt);
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
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, hashStr.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, salt.str().c_str(), -1, SQLITE_TRANSIENT);
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_DONE)
	{
		std::cout << "user: " << username << " registered" << std::endl;
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

	// 1. Length Check (2026 Standard: 12+ is good, 15+ is great)
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