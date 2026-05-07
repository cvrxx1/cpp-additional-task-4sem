#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <map>

// Forward declaration
struct sqlite3;

// Структура для хранения полной информации о файле
struct FileRecord {
    int documentId;
    std::string fileName;
    std::string fullPath;       // Полный восстановленный путь
    std::string lastModified;   // Из Windows.db -> System.DateModified
    std::string fileType;       // Расширение файла
    long long fileSize;         // Размер из Windows.db -> System.Size
};

// Вспомогательная структура для построения дерева папок
struct PathNode {
    int scopeId;
    int parentId;
    std::string name;
};

class DatabaseManager {
public:
    bool extractData(const std::string& gatherDbPath, 
                     const std::string& windowsDbPath, 
                     std::vector<FileRecord>& records);

private:
    std::map<int, PathNode> loadPathMap(sqlite3* db);
    std::string buildFullPath(int scopeId, const std::map<int, PathNode>& pathMap, int depth = 0);
    void loadMetadata(const std::string& windowsDbPath, std::vector<FileRecord>& records);
};

#endif