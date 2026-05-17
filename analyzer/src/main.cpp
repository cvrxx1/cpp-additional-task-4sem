#include <iostream>
#include <vector>
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <sqlite3.h>
#include <algorithm>
#include <sstream>
#include "database.h"
#include "statistics.h"

namespace fs = std::filesystem;

// удаляет временные файлы sqlite в указанной папке
void cleanupTempFiles(const std::string& folder) {
    const char* names[] = {"Windows-gather.db-shm", "Windows-gather.db-wal",
                           "Windows.db-shm", "Windows.db-wal"};
    for (const char* n : names) {
        std::string f = folder + "/" + n;
        if (std::remove(f.c_str()) == 0) {
            std::cout << "[*] Removed temp file: " << f << std::endl;
        }
    }
}

// определяет какой из двух файлов это gather база
std::string detectGatherDb(const std::string& path1, const std::string& path2) {
    sqlite3* db;
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
    return path1;
}

// ищет .db файлы в указанной папке
std::vector<std::string> findDbFiles(const std::string& folder) {
    std::vector<std::string> dbFiles;
    try {
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                if (path.find(".db") != std::string::npos &&
                    path.find("-shm") == std::string::npos &&
                    path.find("-wal") == std::string::npos) {
                    dbFiles.push_back(path);
                }
            }
        }
    } catch (...) {}
    return dbFiles;
}

// обрабатывает одну пару баз из указанной папки
bool processFolder(const std::string& inputFolder, const std::string& outputFile) {
    std::vector<std::string> dbFiles = findDbFiles(inputFolder);
    std::cout << "[*] Found " << dbFiles.size() << " .db files in " << inputFolder << std::endl;

    if (dbFiles.size() < 2) {
        std::cout << "[!] Need 2 database files in " << inputFolder << ", skipping." << std::endl;
        return false;
    }

    std::string gatherDbPath = detectGatherDb(dbFiles[0], dbFiles[1]);
    std::string windowsDbPath = (gatherDbPath == dbFiles[0]) ? dbFiles[1] : dbFiles[0];

    std::cout << "[*] Gather:  " << gatherDbPath << std::endl;
    std::cout << "[*] Windows: " << windowsDbPath << std::endl;

    DatabaseManager dbMgr;
    std::vector<FileRecord> records;

    if (!dbMgr.extractData(gatherDbPath, windowsDbPath, records)) {
        std::cerr << "[!] ERROR: Failed to extract data from " << inputFolder << std::endl;
        return false;
    }

    std::cout << "[+] Extracted " << records.size() << " records." << std::endl;

    // сохраняем таблицу файлов в базу SQLite
    std::string dbOutputFile = outputFile;
    size_t dot = dbOutputFile.find_last_of('.');
    if (dot != std::string::npos)
        dbOutputFile = dbOutputFile.substr(0, dot) + ".db";
    else
        dbOutputFile += ".db";
    dbMgr.saveToDatabase(records, dbOutputFile);

    // сохраняем текстовый отчёт
    Statistics stats;
    stats.generateReport(records, outputFile);
    return true;
}

int main(int argc, char* argv[]) {
    // режим с аргументом -d: несколько папок через ;
    if (argc >= 3 && std::string(argv[1]) == "-d") {
        std::string arg = argv[2];
        std::vector<std::string> folders;
        std::stringstream ss(arg);
        std::string item;
        while (std::getline(ss, item, ';')) {
            if (!item.empty()) folders.push_back(item);
        }

        std::cout << "===== WINDOWS 11 INDEX ANALYZER =====" << std::endl;
        std::cout << "Multiple folders mode (" << folders.size() << " paths)" << std::endl;
        std::cout << "=====================================" << std::endl;

        fs::create_directory("output");
        int processed = 0;
        int idx = 1;
        for (const auto& folder : folders) {
            std::string outputFile = "output/report_" + std::to_string(idx) + ".txt";
            std::cout << "\n--- Processing: " << folder << " ---" << std::endl;
            if (processFolder(folder, outputFile)) {
                cleanupTempFiles(folder);
                std::cout << "[OK] Report saved to " << outputFile << std::endl;
                processed++;
            }
            idx++;
        }

        std::cout << "\n[+] Processed " << processed << "/" << folders.size() << " folders." << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 0;
    }

    // режим с прямыми путями (старый способ)
    if (argc >= 2) {
        std::string gatherDbPath = argv[1];
        std::string windowsDbPath = (argc >= 3) ? argv[2] : "";
        std::string outputFile = (argc >= 4) ? argv[3] : "output/report.txt";

        std::cout << "===== WINDOWS 11 INDEX ANALYZER =====" << std::endl;
        std::cout << "Gather DB: " << gatherDbPath << std::endl;
        std::cout << "Windows DB: " << windowsDbPath << std::endl;
        std::cout << "Output:     " << outputFile << std::endl;
        std::cout << "=====================================" << std::endl;

        DatabaseManager dbMgr;
        std::vector<FileRecord> records;

        if (!dbMgr.extractData(gatherDbPath, windowsDbPath, records)) {
            std::cerr << "[!] ERROR: Failed to extract data." << std::endl;
            cleanupTempFiles(gatherDbPath.substr(0, gatherDbPath.find_last_of("/\\")));
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }

        std::cout << "[+] Extracted " << records.size() << " records." << std::endl;

        std::string dbOutputFile = outputFile;
        size_t dot = dbOutputFile.find_last_of('.');
        if (dot != std::string::npos)
            dbOutputFile = dbOutputFile.substr(0, dot) + ".db";
        else
            dbOutputFile += ".db";
        dbMgr.saveToDatabase(records, dbOutputFile);

        Statistics stats;
        stats.generateReport(records, outputFile);

        cleanupTempFiles(gatherDbPath.substr(0, gatherDbPath.find_last_of("/\\")));
        std::cout << "[OK] Report saved to " << outputFile << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 0;
    }

    // двойной клик — ищем базы в data/
    std::cout << "===== WINDOWS 11 INDEX ANALYZER =====" << std::endl;
    std::cout << "Scanning data/ folder..." << std::endl;
    std::cout << "=====================================" << std::endl;

    std::vector<std::string> dbFiles = findDbFiles("data");
    std::cout << "[*] Found " << dbFiles.size() << " .db files in data/" << std::endl;

    if (dbFiles.size() < 2) {
        std::cout << "[!] Need 2 database files in data/ folder." << std::endl;
        std::cout << "Place Windows-gather.db and Windows.db in data/" << std::endl;
        std::cout << "Or use: program.exe -d \"folder1;folder2;...\"" << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::string gatherDbPath = detectGatherDb(dbFiles[0], dbFiles[1]);
    std::string windowsDbPath = (gatherDbPath == dbFiles[0]) ? dbFiles[1] : dbFiles[0];

    std::cout << "[*] Gather:  " << gatherDbPath << std::endl;
    std::cout << "[*] Windows: " << windowsDbPath << std::endl;

    DatabaseManager dbMgr;
    std::vector<FileRecord> records;

    if (!dbMgr.extractData(gatherDbPath, windowsDbPath, records)) {
        std::cerr << "[!] ERROR: Failed to extract data." << std::endl;
        cleanupTempFiles("data");
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Extracted " << records.size() << " records." << std::endl;

    std::string outputFile = "output/report.txt";
    std::string dbOutputFile = "output/report.db";
    dbMgr.saveToDatabase(records, dbOutputFile);

    Statistics stats;
    stats.generateReport(records, outputFile);

    cleanupTempFiles("data");
    std::cout << "[OK] Report saved to " << outputFile << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}