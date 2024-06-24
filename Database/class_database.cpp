#include "class_database.h"

using namespace std;

// Class: SQLHandler, 데이터베이스 관리
SQLHandler::SQLHandler(string _filename) {
	this->sql_handle = NULL;
	this->open(_filename);

	return ;
}

SQLHandler::~SQLHandler(void) {
	return ;
}

bool SQLHandler::open(string _filename) {
    int result = sqlite3_open(_filename.c_str(), &this->sql_handle);
    cout << "Sqlite3 Open Result : " << result << endl;
    if (result == SQLITE_OK) {
        return true;
    } else if(result == SQLITE_CANTOPEN) {
        ifstream ifile(_filename.c_str());
        if (!ifile) {
            cout << "File does not exist. Trying to create a new one." << endl;
        }
        result = sqlite3_open_v2(_filename.c_str(), &this->sql_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        if (result == SQLITE_OK) {
            cout << "Database not found. A new database has been created." << endl;
            return true;
        }
        cout << "Result: " << result << endl;
    }
    cout << "Can't open database: " << sqlite3_errmsg(this->sql_handle) << endl;
    return false;
}

vector<vector<string>> SQLHandler::query(string _query, int param_count, ...) {
	sqlite3_stmt* statement;
	vector<vector<string>> results;
	int rc;

	string stmp;
	char* param;
	va_list va;
	va_start(va, param_count);

	rc = sqlite3_prepare_v2(this->sql_handle, _query.c_str(), -1, &statement, 0);

	for (int idx = 1; idx <= param_count; idx++) {
		param = va_arg(va, char*);
		stmp = param;
		rc = sqlite3_bind_text(statement, idx, param, stmp.length(), NULL);
	}

	va_end(va);

	if (rc == SQLITE_OK) {
		int cols = sqlite3_column_count(statement);
		int result = 0;

		while (true) {
			result = sqlite3_step(statement);
			if (result == SQLITE_ROW) {
				vector<string> values;

				bool is_exist_col = true;
				for (int col = 0; col < cols; col++) {
					if ((char*)sqlite3_column_text(statement, col) == NULL) {
						is_exist_col = false;
						continue;
					}
					values.push_back((char*)sqlite3_column_text(statement, col));
				}

				if (is_exist_col) {
					results.push_back(values);
				}

			}
			else {
				break;
			}
		}
		sqlite3_finalize(statement);
	}

	string error = sqlite3_errmsg(this->sql_handle);
	if (error != "not an error") {
		cout << _query << " " << error << endl;
	}

	return results;
}

void SQLHandler::close(void) {
	sqlite3_close(this->sql_handle);   
	cout << "Sqlite3 Close" << endl;
	return ;
}
