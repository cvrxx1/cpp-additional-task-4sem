#include <iostream>
#include <vector>
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <sqlite3.h>
#include "database.h"
#include "statistics.h"

namespace fs = std::filesystem;

// удаляет временные файлы sqlite которые остаются после открытия баз
void cleanupTempFiles() {
    const char* tempFiles[] = {
        "data/Windows-gather.db-shm",
        "data/Windows-gather.db-wal",
        "data/Windows.db-shm",
        "data/Windows.db-wal"
    };
    for (const char* f : tempFiles) {
        if (std::remove(f) == 0) {
            std::cout << "[*] Removed temp file: " << f << std::endl;
        }
    }
}

// определяет какой из двух файлов это gather база по наличию таблицы SystemIndex_Gthr
// проверяет оба файла а не только первый
std::string detectGatherDb(const std::string& path1, const std::string& path2) {
    sqlite3* db;
    // проверяем первый файл
    if (sqlite3_open_v2(path1.c_str(), &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, 
            "SELECT name FROM sqlite_master WHERE type='table' AND name='SystemIndex_Gthr'", 
            -1, &stmt, 0) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return path1;
            }
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
    }
    // проверяем второй файл
    if (sqlite3_open_v2(path2.c_str(), &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, 
            "SELECT name FROM sqlite_master WHERE type='table' AND name='SystemIndex_Gthr'", 
            -1, &stmt, 0) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return path2;
            }
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
    }
    return path1; // если ни один не подошел возвращаем первый
}

// ищет все .db файлы в папке data кроме временных shm и wal
std::vector<std::string> findDbFiles() {
    std::vector<std::string> dbFiles;
    try {
        for (const auto& entry : fs::directory_iterator("data")) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                if (path.find(".db") != std::string::npos && 
                    path.find("-shm") == std::string::npos && 
                    path.find("-wal") == std::string::npos) {
                    dbFiles.push_back(path);
                }
            }
        }
    } catch (...) {
        std::cerr << "[!] Error reading data/ folder" << std::endl;
    }
    return dbFiles;
}

int main(int argc, char* argv[]) {
    // пути по умолчанию для двойного клика
    std::string gatherDbPath = "data/Windows-gather.db";
    std::string windowsDbPath = "data/Windows.db";
    std::string outputFile = "output/report.txt";

    // можно передать свои пути через командную строку
    if (argc >= 2) gatherDbPath = argv[1];
    if (argc >= 3) windowsDbPath = argv[2];
    if (argc >= 4) outputFile = argv[3];

    // проверяем существуют ли стандартные файлы
    bool standardFound = false;
    if (argc < 2) {
        std::ifstream gTest(gatherDbPath);
        std::ifstream wTest(windowsDbPath);
        standardFound = gTest.good() && wTest.good();
    }

    // если стандартные не найдены ищем любые db в data
    if (argc < 2 && !standardFound) {
        std::vector<std::string> dbFiles = findDbFiles();
        std::cout << "[*] Found " << dbFiles.size() << " .db files in data/" << std::endl;

        if (dbFiles.size() >= 2) {
            // определяем где gather а где windows по содержимому
            gatherDbPath = detectGatherDb(dbFiles[0], dbFiles[1]);
            windowsDbPath = (gatherDbPath == dbFiles[0]) ? dbFiles[1] : dbFiles[0];
            std::cout << "[*] Auto-detected:" << std::endl;
            std::cout << "    Gather:  " << gatherDbPath << std::endl;
            std::cout << "    Windows: " << windowsDbPath << std::endl;
            std::cout << std::endl;
        } else {
            std::cout << "[!] Need 2 database files in data/ folder." << std::endl;
            std::cout << "Found: " << dbFiles.size() << std::endl;
            std::cout << "Place Windows-gather.db and Windows.db in data/" << std::endl;
            std::cout << "Or run: program.exe <gather_db> <windows_db>" << std::endl;
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
    }

    std::cout << "===== WINDOWS 11 INDEX ANALYZER =====" << std::endl;
    std::cout << "Gather DB: " << gatherDbPath << std::endl;
    std::cout << "Windows DB: " << windowsDbPath << std::endl;
    std::cout << "Output:     " << outputFile << std::endl;
    std::cout << "=====================================" << std::endl;

    DatabaseManager dbMgr;
    std::vector<FileRecord> records;
    
    if (!dbMgr.extractData(gatherDbPath, windowsDbPath, records)) {
        std::cerr << "[!] ERROR: Failed to extract data." << std::endl;
        cleanupTempFiles();
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Extracted " << records.size() << " records." << std::endl;

    Statistics stats;
    stats.generateReport(records, outputFile);

    cleanupTempFiles();
    std::cout << "[OK] Report saved to " << outputFile << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}