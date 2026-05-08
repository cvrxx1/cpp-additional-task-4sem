#ifndef STATISTICS_H
#define STATISTICS_H

#include <vector>
#include <string>
#include "database.h"

class Statistics {
public:
    // Выводит статистику в файл и консоль
    void generateReport(const std::vector<FileRecord>& records, const std::string& outputPath);

private:
    // Записывает строку одновременно в файл и в консоль
    void writeLine(std::ofstream& file, const std::string& line);
    
    void calculateTimeline(std::ofstream& out, const std::vector<FileRecord>& records);
    void findDuplicates(std::ofstream& out, const std::vector<FileRecord>& records);
    void calculateFileTypeStats(std::ofstream& out, const std::vector<FileRecord>& records);
};

#endif