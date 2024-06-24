#include "class_database.h"

#include <openssl/sha.h>
#include <openssl/evp.h>

static const string password_salt[] = {
	"Apple",
	"Banana",
	"ShineMuscat",
	"Mango",
	"Watermelon",
	"Dragonfuit",
	"Grape",
	"Peach",
	"Orange",
	"Lemon"
};

std::string GetSHA512(const string& str)
{
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	const EVP_MD* md = EVP_sha512();

	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int hash_len;

	EVP_DigestInit_ex(ctx, md, NULL);
	EVP_DigestUpdate(ctx, str.c_str(), str.size());
	EVP_DigestFinal_ex(ctx, hash, &hash_len);

	EVP_MD_CTX_free(ctx);

	std::string hashedString = "";
	for (unsigned int i = 0; i < hash_len; i++)
	{
		char hex[3];
		sprintf(hex, "%02x", hash[i]);
		hashedString += hex;
	}

	return hashedString;
}

string GetSaltHashPassword(const string username, const string password) {
	string hashedPassword = "";
	int salt_idx = 0;

	salt_idx = (username.front() + username.back()) % 10;
	hashedPassword = GetSHA512(password) + password_salt[salt_idx];
	hashedPassword = GetSHA512(hashedPassword);

	return hashedPassword;
}

void print_result(vector<vector<string>> v_result) {
	for (int idx = 0; idx < (int)v_result.size(); idx++) {
		for (int j = 0; j < (int)v_result[idx].size(); j++) {
			cout << "/" << v_result[idx][j];
		}
		cout << endl;
	}
}

int main(void) {

	SQLHandler* sql_handler = new SQLHandler("./user.db");
	vector<vector<string>> v_result;
	char input[10];
	string username;
	string password;
	string created_at;
	string hashedPassword = "";

	while (1) {
		printf("q - quit/ t - create table/ i - username,password/ c - +created_at\n");

		if (NULL == fgets(input, 10, stdin)) {
			fprintf(stderr, "input error");
			break;
		}

		if (input[0] == 'q') {
			break;
		}
		else if (input[0] == 'i') {
			cout << "username : ";
			getline(cin, username);

			cout << "password : ";
			getline(cin, password);

			hashedPassword = GetSaltHashPassword(username, password);
			v_result = sql_handler->query("INSERT INTO user(username, password) VALUES(?, ?);", 2, &username.at(0), &hashedPassword.at(0));

			print_result(v_result);
		}
		else if (input[0] == 'c') {
			cout << "username : ";
			getline(cin, username);

			cout << "password : ";
			getline(cin, password);

			cout << "created_at : ";
			getline(cin, created_at);

			hashedPassword = GetSaltHashPassword(username, password);
			v_result = sql_handler->query("INSERT INTO user(username, password, created_at) VALUES(?, ?, ?); ", 3, &username.at(0), &hashedPassword.at(0), &created_at.at(0));

			print_result(v_result);
		}
		else if (input[0] == 't') {
			v_result = sql_handler->query("CREATE TABLE user (username VARCHAR(255) NOT NULL, password VARCHAR(255) NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, is_admin BOOLEAN DEFAULT FALSE);", 0);

			print_result(v_result);
		}
		else if (input[0] == 'p') {
			cout << "username : ";
			getline(cin, username);

			cout << "password : ";
			getline(cin, password);

			hashedPassword = GetSaltHashPassword(username, password);

			v_result = sql_handler->query("UPDATE user SET password = ?, created_at = CURRENT_TIMESTAMP WHERE username = ? ;", 2, &hashedPassword.at(0), &username.at(0));

			print_result(v_result);
		}
		else if (input[0] == 's') {

			v_result = sql_handler->query("select * from user;", 0);

			print_result(v_result);
		}
		else if (input[0] == 'd') {
			cout << "DELETE username : ";
			getline(cin, username);

			v_result = sql_handler->query(" delete from user where username = ?;", 1, &username.at(0));

			print_result(v_result);
		}
	}
	sql_handler->close();

	return 0;
}

