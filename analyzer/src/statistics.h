#ifndef STATISTICS_H
#define STATISTICS_H

#include <vector>
#include <string>
#include "database.h"

class Statistics {
public:
    // создает отчет со всеми разделами и сохраняет в файл
    void generateReport(const std::vector<FileRecord>& records, const std::string& outputPath);

private:
    // выводит строку одновременно в консоль и в файл
    void writeLine(std::ofstream& file, const std::string& line);
    
    // раздел 1: последние измененные файлы
    void calculateTimeline(std::ofstream& out, const std::vector<FileRecord>& records);
    // раздел 2: поиск дубликатов по имени файла
    void findDuplicates(std::ofstream& out, const std::vector<FileRecord>& records);
    // раздел 3: статистика по расширениям файлов
    void calculateFileTypeStats(std::ofstream& out, const std::vector<FileRecord>& records);
};

#endif