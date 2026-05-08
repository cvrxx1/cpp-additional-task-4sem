#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <algorithm>

static int collationStub(void*, int len1, const void* str1, int len2, const void* str2) {
    int minLen = len1 < len2 ? len1 : len2;
    int result = 0;
    for (int i = 0; i < minLen; i++) {
        char c1 = tolower(((const char*)str1)[i]);
        char c2 = tolower(((const char*)str2)[i]);
        if (c1 != c2) { result = c1 - c2; break; }
    }
    if (result == 0 && len1 != len2) result = len1 - len2;
    return result;
}

std::map<int, PathNode> DatabaseManager::loadPathMap(sqlite3* db) {
    std::map<int, PathNode> pathMap;
    const char* query = "SELECT Scope, Parent, Name FROM SystemIndex_GthrPth WHERE Name IS NOT NULL;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PathNode node;
            node.scopeId = sqlite3_column_int(stmt, 0);
            node.parentId = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? sqlite3_column_int(stmt, 1) : -1;
            const unsigned char* nameText = sqlite3_column_text(stmt, 2);
            node.name = nameText ? reinterpret_cast<const char*>(nameText) : "";
            pathMap[node.scopeId] = node;
        }
        sqlite3_finalize(stmt);
    }
    return pathMap;
}

std::string DatabaseManager::buildFullPath(int scopeId, const std::map<int, PathNode>& pathMap, int depth) {
    if (depth > 20) return "...";
    auto it = pathMap.find(scopeId);
    if (it == pathMap.end()) return "?";
    const PathNode& node = it->second;
    if (node.parentId == -1) return node.name;
    std::string parentPath = buildFullPath(node.parentId, pathMap, depth + 1);
    if (parentPath.empty() || parentPath == "?") return node.name;
    return parentPath + "\\" + node.name;
}

void DatabaseManager::loadMetadata(const std::string& windowsDbPath, std::vector<FileRecord>& records) {
    if (records.empty()) return;
    sqlite3* db;
    if (sqlite3_open_v2(windowsDbPath.c_str(), &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        std::cerr << "[!] Cannot open Windows.db: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    sqlite3_create_collation(db, "UNICODE_en-US_LINGUISTIC_IGNORECASE", SQLITE_UTF8, NULL, collationStub);

    std::map<int, std::pair<long long, std::string>> metaMap;
    const int BATCH_SIZE = 500;

    for (size_t start = 0; start < records.size(); start += BATCH_SIZE) {
        std::string idList;
        size_t end = std::min(start + BATCH_SIZE, records.size());
        for (size_t i = start; i < end; ++i) {
            if (!idList.empty()) idList += ",";
            idList += std::to_string(records[i].documentId);
        }

        std::string query = 
            "SELECT ps.WorkId, "
            "MAX(CASE WHEN ps.ColumnId=13 THEN ps.Value END), "
            "MAX(CASE WHEN ps.ColumnId=15 THEN ps.Value END) "
            "FROM SystemIndex_1_PropertyStore ps "
            "WHERE ps.WorkId IN (" + idList + ") "
            "GROUP BY ps.WorkId;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int workId = sqlite3_column_int(stmt, 0);
                long long size = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? sqlite3_column_int64(stmt, 1) : -1;
                std::string date = sqlite3_column_type(stmt, 2) != SQLITE_NULL ?
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "N/A";
                metaMap[workId] = {size, date};
            }
            sqlite3_finalize(stmt);
        }
    }
    sqlite3_close(db);

    for (auto& rec : records) {
        auto it = metaMap.find(rec.documentId);
        if (it != metaMap.end()) {
            rec.fileSize = it->second.first;
            rec.lastModified = it->second.second;
        } else {
            rec.fileSize = -1;
            rec.lastModified = "N/A";
        }
    }
}

bool DatabaseManager::extractData(const std::string& gatherDbPath,
                                 const std::string& windowsDbPath,
                                 std::vector<FileRecord>& records) {
    sqlite3* db;
    std::cout << "[*] Открываем " << gatherDbPath << "..." << std::endl;
    if (sqlite3_open_v2(gatherDbPath.c_str(), &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        std::cerr << "[!] Ошибка открытия: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_create_collation(db, "UNICODE_en-US_LINGUISTIC_IGNORECASE", SQLITE_UTF8, NULL, collationStub);

    std::cout << "[*] Загружаем иерархию папок..." << std::endl;
    auto pathMap = loadPathMap(db);
    std::cout << "[+] Узлов путей: " << pathMap.size() << std::endl;

    std::cout << "[*] Извлекаем файлы..." << std::endl;
    const char* query = "SELECT DocumentID, FileName, ScopeID FROM SystemIndex_Gthr;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "[!] SQL error: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileRecord rec;
        rec.documentId = sqlite3_column_int(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        rec.fileName = nameText ? reinterpret_cast<const char*>(nameText) : "?";

        // Правильное извлечение расширения: только если точка есть и это не первый/последний символ
        std::string fname = rec.fileName;
        size_t dot = fname.find_last_of('.');
        if (dot != std::string::npos && dot > 0 && dot < fname.length() - 1) {
            rec.fileType = fname.substr(dot + 1);
            // Отсекаем явно «битые» расширения длиннее 10 символов
            if (rec.fileType.length() > 10) rec.fileType = "no_ext";
        } else {
            rec.fileType = "no_ext";
        }

        int scopeId = sqlite3_column_type(stmt, 2) != SQLITE_NULL ? sqlite3_column_int(stmt, 2) : -1;
        if (scopeId == -1) rec.fullPath = rec.fileName;
        else {
            std::string parentPath = buildFullPath(scopeId, pathMap);
            rec.fullPath = (parentPath == "?" || parentPath == "...") ? rec.fileName : parentPath + "\\" + rec.fileName;
        }
        rec.lastModified = "N/A";
        rec.fileSize = -1;
        records.push_back(rec);
        count++;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "[+] Извлечено записей из gather: " << count << std::endl;

    // Загружаем метаданные из Windows.db
    std::cout << "[*] Загружаем метаданные из " << windowsDbPath << "..." << std::endl;
    loadMetadata(windowsDbPath, records);

    int withMeta = 0;
    for (const auto& r : records) if (r.lastModified != "N/A") withMeta++;
    std::cout << "[+] Записей с метаданными: " << withMeta << "/" << records.size() << std::endl;

    return count > 0;
}