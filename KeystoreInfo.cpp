/*
		Copyright 2013 to 2022 TeamWin
		TWRP is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version.

		TWRP is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with TWRP.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <string>
#include <sqlite3.h>
#include "KeystoreInfo.hpp"

std::string KeystoreInfo::uint2hex(int64_t num) {
	uint64_t temp = num;
	std::string s = "";
	while (temp) {
		int a = temp % 16;
		if (a <= 9)
			s += (48 + a);
		else
			s += (87 + a);
		temp = temp / 16;
	}
	std::reverse(s.begin(), s.end());
	return s;
}

std::string KeystoreInfo::getHandle(const userid_t user_id) {
	int rc = 0;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	const char *path = "/data/system/locksettings.db";

	// Android creates this database during its first boot. Opening it with
	// the default flags (read-write, create) would leave an empty root-owned
	// file behind when that boot never got that far, and the ROM could not
	// open its own database any more. Only read it, and only if it exists.
	if (access(path, F_OK) != 0) {
		fprintf(stderr, "%s does not exist\n", path);
		return "";
	}
	rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return "";
	}
	std::string sql = "SELECT * FROM locksettings WHERE name = 'sp-handle' AND user = " + std::to_string(user_id);
	rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
	if (rc != SQLITE_OK && sqlite3_extended_errcode(db) == SQLITE_READONLY_ROLLBACK) {
		// An interrupted write left a hot rollback journal, which a read-only
		// connection cannot roll back. Reopen read-write but without create
		// (the file exists, see above) and let SQLite roll it back in place,
		// as Android does on its next start. No file is created and the
		// owner and label of the database do not change.
		fprintf(stderr, "%s has a hot journal, rolling it back\n", path);
		sqlite3_close(db);
		db = NULL;
		rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return "";
		}
		rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
	}
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Can't read %s: %s\n", path, sqlite3_errmsg(db));
		sqlite3_close(db);
		return "";
	}
	int64_t value = 0;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		value = sqlite3_column_int64(stmt, 3);
	}
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return "";
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return uint2hex(value);
}

std::string KeystoreInfo::getAlias(std::string handle) {
	std::string alias(SYNTHETIC_PASSWORD_KEY_PREFIX);
	alias = alias + handle;
	return alias;
}
