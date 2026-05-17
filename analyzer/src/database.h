#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <map>

// предварительное объявление вместо включения sqlite3.h
struct sqlite3;

// хранит всю информацию об одном проиндексированном файле
struct FileRecord {
    int documentId;
    std::string fileName;
    std::string fullPath;       // полный путь, восстановленный из SystemIndex_GthrPth
    std::string lastModified;   // дата изменения из Windows.db
    std::string fileType;       // расширение файла в нижнем регистре
    long long fileSize;         // размер файла из Windows.db
};

// узел в дереве папок для восстановления путей
struct PathNode {
    int scopeId;
    int parentId;
    std::string name;
};

class DatabaseManager {
public:
    // основная функция извлечения данных из двух баз
    bool extractData(const std::string& gatherDbPath, 
                     const std::string& windowsDbPath, 
                     std::vector<FileRecord>& records);

    // сохраняет список файлов в новую базу SQLite (таблица Data)
    bool saveToDatabase(const std::vector<FileRecord>& records, const std::string& outputPath);

private:
    // загружает иерархию папок из SystemIndex_GthrPth
    std::map<int, PathNode> loadPathMap(sqlite3* db);
    // рекурсивно строит полный путь по scopeId
    std::string buildFullPath(int scopeId, const std::map<int, PathNode>& pathMap, int depth = 0);
    // загружает метаданные (размер, дату) из Windows.db
    void loadMetadata(const std::string& windowsDbPath, std::vector<FileRecord>& records);
};

#endif