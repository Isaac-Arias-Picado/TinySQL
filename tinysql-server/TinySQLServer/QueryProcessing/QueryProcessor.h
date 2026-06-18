#pragma once
#include <string>
#include "../StoredDataManager/Types.h"
#include "../StoredDataManager/StoredDataManager.h"

class QueryProcessor {
public:
    QueryResult execute(const std::string& sql, const std::string& dbContext);
private:
    StoredDataManager storage;
    std::string limpiarNombre(const std::string& raw);
};