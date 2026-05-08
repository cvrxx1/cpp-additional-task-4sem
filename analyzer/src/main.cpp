#include <iostream>
#include <vector>
#include <fstream>
#include <cstdio>   // для remove()
#include "database.h"
#include "statistics.h"

// Удаляет временные файлы SQLite
void cleanupTempFiles() {
    const char* tempFiles[] = {
        "data/Windows-gather.db-shm",
        "data/Windows-gather.db-wal",
        "data/Windows.db-shm",
        "data/Windows.db-wal"
    };
    for (const char* f : tempFiles) {
        if (std::remove(f) == 0) {
            std::cout << "[*] Удалён временный файл: " << f << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string gatherDbPath = "data/Windows-gather.db";
    std::string windowsDbPath = "data/Windows.db";
    std::string outputFile = "output/report.txt";

    if (argc >= 2) gatherDbPath = argv[1];
    if (argc >= 3) windowsDbPath = argv[2];
    if (argc >= 4) outputFile = argv[3];
    
    std::cout << "===== АНАЛИЗАТОР ИНДЕКСА WINDOWS 11 =====" << std::endl;
    std::cout << "Gather DB: " << gatherDbPath << std::endl;
    std::cout << "Windows DB: " << windowsDbPath << std::endl;
    std::cout << "Output: " << outputFile << std::endl;
    std::cout << "=========================================" << std::endl;

    DatabaseManager dbMgr;
    std::vector<FileRecord> records;
    
    if (!dbMgr.extractData(gatherDbPath, windowsDbPath, records)) {
        std::cerr << "[!] КРИТИЧЕСКАЯ ОШИБКА: Не удалось извлечь данные из баз." << std::endl;
        cleanupTempFiles();  // чистим даже при ошибке
        return 1;
    }

    Statistics stats;
    stats.generateReport(records, outputFile);

    // Удаляем временные файлы после успешного завершения
    cleanupTempFiles();

    return 0;
}