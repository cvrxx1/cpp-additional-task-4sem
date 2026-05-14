#include "statistics.h"
#include <fstream>
#include <map>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>

// выводит строку и в консоль и в файл одновременно
void Statistics::writeLine(std::ofstream& file, const std::string& line) {
    std::cout << line << std::endl;
    if (file.is_open()) {
        file << line << std::endl;
    }
}

// разделитель для красивого форматирования
static std::string separator(char c = '=', int len = 70) {
    return std::string(len, c);
}

// главная функция генерации всего отчета
void Statistics::generateReport(const std::vector<FileRecord>& records, const std::string& outputPath) {
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);

    writeLine(out, "");
    writeLine(out, separator('=', 70));
    writeLine(out, "     WINDOWS 11 SEARCH INDEX REPORT");
    writeLine(out, separator('=', 70));
    writeLine(out, "");
    writeLine(out, "  Total entries in index: " + std::to_string(records.size()));
    writeLine(out, "");

    calculateTimeline(out, records);
    findDuplicates(out, records);
    calculateFileTypeStats(out, records);

    writeLine(out, separator('=', 70));
    writeLine(out, "  Report generated successfully.");
    writeLine(out, separator('=', 70));

    out.close();
    std::cout << "\n[OK] Report saved to " << outputPath << std::endl;
}

// раздел с временной шкалой последних изменений
void Statistics::calculateTimeline(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  1. ACTIVITY TIMELINE");
    writeLine(out, "     (recently modified files)");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    // копируем только записи у которых есть дата изменения
    std::vector<FileRecord> sortedRecords = records;
    sortedRecords.erase(std::remove_if(sortedRecords.begin(), sortedRecords.end(), [](const FileRecord& r) {
        return r.lastModified == "N/A" || r.lastModified.empty();
    }), sortedRecords.end());

    // сортируем по дате сначала новые
    std::sort(sortedRecords.begin(), sortedRecords.end(), [](const FileRecord& a, const FileRecord& b) {
        return a.lastModified > b.lastModified;
    });

    writeLine(out, "  Last 20 modified files:");
    writeLine(out, "");

    int count = 0;
    for (const auto& rec : sortedRecords) {
        if (count++ >= 20) break;
        std::stringstream ss;
        ss << "  " << std::setw(2) << count << ". ";
        ss << "[" << rec.lastModified << "] ";

        // показываем размер файла если он известен
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

        ss << "\n     " << rec.fullPath;
        writeLine(out, ss.str());
        writeLine(out, "");
    }
    writeLine(out, "");
}

// раздел поиска дубликатов по имени файла
void Statistics::findDuplicates(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  2. DUPLICATE FILES");
    writeLine(out, "     (by file name)");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    // группируем файлы по имени
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
            if (dupFound >= 30) continue; // показываем только первые 30 групп

            dupFound++;
            writeLine(out, "  [" + std::to_string(dupFound) + "] \"" + name + "\" — " +
                      std::to_string(paths.size()) + " copies found:");
            // выводим первые 5 копий
            for (size_t i = 0; i < paths.size() && i < 5; i++) {
                std::stringstream ss;
                ss << "      " << (i + 1) << ". " << paths[i];
                writeLine(out, ss.str());
            }
            if (paths.size() > 5) {
                writeLine(out, "      ... and " + std::to_string(paths.size() - 5) + " more");
            }
            writeLine(out, "");
        }
    }

    if (totalDups == 0) {
        writeLine(out, "  No duplicates found.");
    } else {
        writeLine(out, "  Total duplicate groups: " + std::to_string(totalDups));
        if (totalDups > 30) {
            writeLine(out, "  (first 30 shown)");
        }
    }
    writeLine(out, "");
}

// раздел статистики по типам файлов
void Statistics::calculateFileTypeStats(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  3. FILE TYPE STATISTICS");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    std::map<std::string, int> typeCount;
    int noExtCount = 0;

    // подсчитываем количество каждого расширения
    for (const auto& rec : records) {
        if (rec.fileType == "no_ext" || rec.fileType.empty()) {
            noExtCount++;
        } else {
            // приводим к нижнему регистру чтобы объединить jpg и JPG
            std::string lower = rec.fileType;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            typeCount[lower]++;
        }
    }

    // сортируем по убыванию количества
    std::vector<std::pair<std::string, int>> sortedTypes(typeCount.begin(), typeCount.end());
    std::sort(sortedTypes.begin(), sortedTypes.end(), [](auto& a, auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    writeLine(out, "  Top file extensions by count:");
    writeLine(out, "");
    writeLine(out, "  +----------------------+------------+----------+");
    writeLine(out, "  | Extension            |    Count   |   Share  |");
    writeLine(out, "  +----------------------+------------+----------+");

    int shown = 0;
    int totalWithExt = records.size() - noExtCount;

    for (const auto& [ext, count] : sortedTypes) {
        if (shown >= 25) break; // только топ 25
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
        ss << "  Without extension: " << noExtCount << " (" << std::fixed << std::setprecision(1) << pct << "%)";
        writeLine(out, ss.str());
    }

    writeLine(out, "");
    writeLine(out, "  Total unique extensions: " + std::to_string(typeCount.size()));
    writeLine(out, "");
}