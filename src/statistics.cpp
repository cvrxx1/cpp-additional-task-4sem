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

static std::string shortenPath(const std::string& path, size_t maxLen = 90) {
    if (path.length() <= maxLen) return path;
    return "..." + path.substr(path.length() - maxLen + 3);
}

static std::string separator(char c = '=', int len = 70) {
    return std::string(len, c);
}

void Statistics::generateReport(const std::vector<FileRecord>& records, const std::string& outputPath) {
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    // BOM removed — pure ASCII/UTF-8 without BOM for better compatibility
    // unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    // out.write((char*)bom, sizeof(bom));

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

void Statistics::calculateTimeline(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  1. ACTIVITY TIMELINE");
    writeLine(out, "     (recently modified files)");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    std::vector<FileRecord> sortedRecords = records;
    sortedRecords.erase(std::remove_if(sortedRecords.begin(), sortedRecords.end(), [](const FileRecord& r) {
        return r.lastModified == "N/A" || r.lastModified.empty();
    }), sortedRecords.end());

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

        // File size if known
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
    writeLine(out, "  2. DUPLICATE FILES");
    writeLine(out, "     (by file name)");
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
            if (dupFound >= 30) continue;

            dupFound++;
            writeLine(out, "  [" + std::to_string(dupFound) + "] \"" + name + "\" — " +
                      std::to_string(paths.size()) + " copies found:");
            for (size_t i = 0; i < paths.size() && i < 5; i++) {
                std::stringstream ss;
                ss << "      " << (i + 1) << ". " << shortenPath(paths[i], 62);
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

void Statistics::calculateFileTypeStats(std::ofstream& out, const std::vector<FileRecord>& records) {
    writeLine(out, separator('-', 70));
    writeLine(out, "  3. FILE TYPE STATISTICS");
    writeLine(out, separator('-', 70));
    writeLine(out, "");

    std::map<std::string, int> typeCount;
    int noExtCount = 0;

    for (const auto& rec : records) {
        if (rec.fileType == "no_ext" || rec.fileType.empty()) {
            noExtCount++;
        } else {
            std::string lower = rec.fileType;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            typeCount[lower]++;
        }
    }

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
        ss << "  Without extension: " << noExtCount << " (" << std::fixed << std::setprecision(1) << pct << "%)";
        writeLine(out, ss.str());
    }

    writeLine(out, "");
    writeLine(out, "  Total unique extensions: " + std::to_string(typeCount.size()));
    writeLine(out, "");
}