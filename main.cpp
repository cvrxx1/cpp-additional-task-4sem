#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <ctime>
#include <cstring>
#include "sqlite3.h"

struct FileRecord {
    int id;
    std::string name;
    std::string folder;
    long long lastModified;
    std::string extension;
};

int win_collation(void* p, int n1, const void* v1, int n2, const void* v2) {
    int n = n1 < n2 ? n1 : n2;
    return memcmp(v1, v2, n);
}

std::string formatFileTime(long long fileTime) {
    if (fileTime <= 0) return "N/A";
    // Convert Windows FileTime to Unix epoch
    time_t unixTime = (time_t)((fileTime / 10000000) - 11644473600ULL);
    char buf[20];
    struct tm ltm;
    localtime_s(&ltm, &unixTime);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &ltm);
    return std::string(buf);
}

int main() {
    sqlite3* db;
    char* zErrMsg = 0;

    if (sqlite3_open("Windows-gather.db", &db) != SQLITE_OK) {
        std::cerr << "Error opening database!" << std::endl;
        return 1;
    }

    sqlite3_create_collation(db, "UNICODE_en-US_LINGUISTIC_IGNORECASE", SQLITE_UTF8, nullptr, win_collation);
    sqlite3_exec(db, "ATTACH DATABASE 'Windows.db' AS win_db;", NULL, NULL, NULL);

    std::string query = 
        "SELECT g.DocumentID, g.FileName, p.Name, g.LastModified "
        "FROM SystemIndex_Gthr g "
        "JOIN SystemIndex_GthrPth p ON g.ScopeID = p.Scope "
        "LIMIT 100;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Query Error: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    std::map<std::string, int> extStats;
    std::map<std::string, int> duplicateCheck;

    std::cout << std::left << std::setw(8) << "ID" 
              << std::setw(35) << "File Name" 
              << "Date Modified" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* nameRaw = (const char*)sqlite3_column_text(stmt, 1);
        std::string name = nameRaw ? nameRaw : "Unknown";
        long long mTime = sqlite3_column_int64(stmt, 3);

        size_t dot = name.find_last_of(".");
        std::string ext = (dot != std::string::npos) ? name.substr(dot) : "No Extension";

        extStats[ext]++;
        duplicateCheck[name]++;

        std::cout << std::left << std::setw(8) << id 
                  << std::setw(35) << (name.length() > 32 ? name.substr(0, 29) + "..." : name)
                  << formatFileTime(mTime) << std::endl;
    }

    sqlite3_finalize(stmt);

    std::cout << "\n=== FILE TYPE STATISTICS ===\n";
    for (auto const& [ext, count] : extStats) std::cout << ext << ": " << count << std::endl;

    std::cout << "\n=== DUPLICATE NAMES FOUND ===\n";
    for (auto const& [name, count] : duplicateCheck) {
        if (count > 1) std::cout << name << " (copies: " << count << ")\n";
    }

    sqlite3_close(db);
    return 0;
}