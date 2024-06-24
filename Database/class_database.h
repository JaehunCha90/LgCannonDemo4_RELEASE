#ifndef __CLASS_DATABASE_H__
#define __CLASS_DATABASE_H__

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sqlite3.h>

using namespace std;

class SQLHandler {
	private:
		sqlite3 *sql_handle;

	public:
		SQLHandler(string _filename);
		~SQLHandler(void);
		
		bool open(string _filename);
		vector<vector<string>> query(string _query, int param_count, ...);
		void close(void);
};

#endif