#pragma once
#include <string>
#include <set>
#include "Types.h"
class StoredDataManager {
public:
    StoredDataManager();
    QueryResult crearBaseDatos(const std::string& nombre);
    QueryResult eliminarBaseDatos(const std::string& nombre);
    bool existeBaseDatos(const std::string& nombre) const;
    QueryResult crearTabla(const std::string& dbName, const std::string& tableName, const std::vector<Columna>& columnas);
private:
    std::set<std::string> databases;
    void cargarBasesDeDatos();
};