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
private:
    std::set<std::string> databases;
    void cargarBasesDeDatos();
};