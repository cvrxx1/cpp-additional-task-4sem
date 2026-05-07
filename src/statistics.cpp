#include "statistics.h"
#include <fstream>
#include <map>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>

void Statistics::writeLine(std::ofstream& file, const std::string& line) {
    std::cout << line << std::endl;
    if (file.is_open()) {
        file << line << std::endl;
    }
}

// Обрезка длинных путей
static std::string shortenPath(const std::string& path, size_t maxLen = 90) {
    if (path.length() <= maxLen) return path;
    return "..." + path.substr(path.length() - maxLen + 3);
}

// Разделитель
static std::string separator(char c = '=', int len = 70) {
    return std::string(len, c);
}

void Statistics::generateReport(const std::vector<FileRecord>& records, const std::string& outputPath) {
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    out.write((char*)bom, sizeof(bom));

    writeLine(out, "");
    writeLine(out, separator('=', 70));
    writeLine(out, "     ОТЧЁТ ПО ИНДЕКСУ WINDOWS 11");
    writeLine(out, separator('=', 70));
    writeLine(out, "");
    writeLine(out, "  Всего записей в индексе: " + std::to_string(records.size()));
    writeLine(out, "");

    calculateTimeline(out, records);
    findDuplicates(out, records);
    calculateFileTypeStats(out, records);

    writeLine(out, separator('=', 70));
    writeLine(out, "  Отчёт сгенерирован успешно.");
    writeLine(out, separator('=', 70));

    out.close();
    std::cout << "\n[ГОТОВО] Отчет сохранен в " << outputPath << std::endl;
}

void Statistics::calculateTimeline(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  1. ВРЕМЕННАЯ ШКАЛА АКТИВНОСТИ");
    writeLine(out, "     (последние изменённые файлы)");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    std::vector<FileRecord> sortedRecords = records;
    sortedRecords.erase(std::remove_if(sortedRecords.begin(), sortedRecords.end(), [](const FileRecord& r) {
        return r.lastModified == "N/A" || r.lastModified.empty();
    }), sortedRecords.end());

    std::sort(sortedRecords.begin(), sortedRecords.end(), [](const FileRecord& a, const FileRecord& b) {
        return a.lastModified > b.lastModified;
    });

    writeLine(out, "  Последние 20 изменённых файлов:");
    writeLine(out, "");

    int count = 0;
    for (const auto& rec : sortedRecords) {
        if (count++ >= 20) break;
        std::stringstream ss;
        ss << "  " << std::setw(2) << count << ". ";
        ss << "[" << rec.lastModified << "] ";
        
        // Размер, если известен
        if (rec.fileSize >= 0) {
            double size = rec.fileSize;
            std::string unit = " B";
            if (size > 1024) { size /= 1024; unit = " KB"; }
            if (size > 1024) { size /= 1024; unit = " MB"; }
            if (size > 1024) { size /= 1024; unit = " GB"; }
            std::stringstream sz;
            sz << std::fixed << std::setprecision(1) << size << unit;
            ss << "(" << std::setw(10) << sz.str() << ") ";
        }
        
        ss << "\n     " << shortenPath(rec.fullPath, 65);
        writeLine(out, ss.str());
        writeLine(out, "");
    }
    writeLine(out, "");
}

void Statistics::findDuplicates(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  2. ПОИСК ДУБЛИКАТОВ ФАЙЛОВ");
    writeLine(out, "     (по совпадению имени)");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    std::map<std::string, std::vector<std::string>> nameMap;
    for (const auto& rec : records) {
        if (rec.fileName.length() > 3) {
            nameMap[rec.fileName].push_back(rec.fullPath);
        }
    }

    int dupFound = 0;
    int totalDups = 0;
    for (const auto& [name, paths] : nameMap) {
        if (paths.size() > 1) {
            totalDups++;
            if (dupFound >= 30) continue; // Ограничим для читаемости
            
            dupFound++;
            writeLine(out, "  [" + std::to_string(dupFound) + "] \"" + name + "\" — найдено " + 
                      std::to_string(paths.size()) + " копий:");
            for (size_t i = 0; i < paths.size() && i < 5; i++) {
                std::stringstream ss;
                ss << "      " << (i + 1) << ". " << shortenPath(paths[i], 62);
                writeLine(out, ss.str());
            }
            if (paths.size() > 5) {
                writeLine(out, "      ... и ещё " + std::to_string(paths.size() - 5) + " копий");
            }
            writeLine(out, "");
        }
    }

    if (totalDups == 0) {
        writeLine(out, "  Дубликаты не найдены.");
    } else {
        writeLine(out, "  Всего найдено групп дубликатов: " + std::to_string(totalDups));
        if (totalDups > 30) {
            writeLine(out, "  (показаны первые 30)");
        }
    }
    writeLine(out, "");
}

void Statistics::calculateFileTypeStats(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  3. СТАТИСТИКА ПО ТИПАМ ФАЙЛОВ");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    std::map<std::string, int> typeCount;
    int noExtCount = 0;
    int folderCount = 0;

    for (const auto& rec : records) {
        if (rec.fileType == "no_ext" || rec.fileType.empty()) {
            noExtCount++;
        } else {
            // Приводим к нижнему регистру для объединения .JPG и .jpg
            std::string lower = rec.fileType;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            typeCount[lower]++;
        }
    }

    // Топ-20 расширений
    std::vector<std::pair<std::string, int>> sortedTypes(typeCount.begin(), typeCount.end());
    std::sort(sortedTypes.begin(), sortedTypes.end(), [](auto& a, auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    writeLine(out, "  Топ расширений по количеству файлов:");
    writeLine(out, "");
    writeLine(out, "  +----------------------+------------+----------+");
    writeLine(out, "  | Расширение           | Количество |    Доля  |");
    writeLine(out, "  +----------------------+------------+----------+");

    int shown = 0;
    int totalWithExt = records.size() - noExtCount;
    
    for (const auto& [ext, count] : sortedTypes) {
        if (shown >= 25) break;
        double pct = totalWithExt > 0 ? (100.0 * count / totalWithExt) : 0;
        std::stringstream ss;
        ss << "  | ." << std::setw(20) << std::left << ext 
           << " | " << std::setw(10) << count 
           << " | " << std::setw(7) << std::fixed << std::setprecision(1) << pct << "% |";
        writeLine(out, ss.str());
        shown++;
    }

    writeLine(out, "  +----------------------+------------+----------+");
    writeLine(out, "");
    
    if (noExtCount > 0) {
        double pct = 100.0 * noExtCount / records.size();
        std::stringstream ss;
        ss << "  Без расширения: " << noExtCount << " (" << std::fixed << std::setprecision(1) << pct << "%)";
        writeLine(out, ss.str());
    }

    writeLine(out, "");
    writeLine(out, "  Всего уникальных расширений: " + std::to_string(typeCount.size()));
    writeLine(out, "");
}