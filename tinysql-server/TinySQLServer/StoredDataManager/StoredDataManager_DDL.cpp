#include "StoredDataManager.h"
#include "SystemCatalog.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Crea la carpeta de la BD y la registra en el catalogo.
QueryResult StoredDataManager::crearBaseDatos(const std::string& nombre) {
    QueryResult r;
    if (databases.count(nombre)) {
        r.error = "Database already exists";
        return r;
    }
    fs::create_directory(DATA_DIR + "/" + nombre);
    databases.insert(nombre);
    escribirBaseDatos(nombre);
    r.success = true;
    r.type = "ddl";
    r.message = "Database created";
    return r;
}

// Borra la carpeta de la BD con todo su contenido.
QueryResult StoredDataManager::eliminarBaseDatos(const std::string& nombre) {
    QueryResult r;
    if (!databases.count(nombre)) {
        r.error = "Database does not exist";
        return r;
    }
    fs::remove_all(DATA_DIR + "/" + nombre);
    databases.erase(nombre);
    r.success = true;
    r.type = "ddl";
    r.message = "Database dropped";
    return r;
}

// Crea el archivo binario (vacio) de la tabla y registra la tabla y sus
// columnas en el System Catalog.
QueryResult StoredDataManager::crearTabla(const std::string& dbName,
    const std::string& tableName,
    const std::vector<Columna>& columnas) {
    QueryResult r;
    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ofstream file(rutaTabla, std::ios::binary | std::ios::app);
    file.close();

    escribirTabla(dbName, tableName);
    for (int i = 0; i < (int)columnas.size(); ++i) {
        escribirColumna(dbName, tableName, columnas[i], i);
    }

    r.success = true;
    r.type = "ddl";
    r.message = "Table created";
    return r;
}

// DROP TABLE: solo permite borrar si la tabla esta vacia (sin filas). Ademas del
// archivo, limpia la tabla, sus columnas y sus indices del catalogo, y descarta
// los indices que tuviera en memoria.
QueryResult StoredDataManager::eliminarTabla(const std::string& dbName,
    const std::string& tableName) {
    QueryResult r;

    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) {
        r.error = "Table does not exist";
        return r;
    }

    // Revisar si la tabla tiene filas mirando el tamano del archivo
    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream file(rutaTabla, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        r.error = "Cannot open table file";
        return r;
    }
    size_t size = file.tellg();
    file.close();

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    if (size > 0 && size % registroSize == 0) {
        r.error = "Table is not empty. DROP TABLE only allowed on empty tables.";
        return r;
    }

    if (!fs::remove(rutaTabla)) {
        r.error = "Failed to delete table file";
        return r;
    }

    // Limpiar la metadata del catalogo
    eliminarTablaDelCatalogo(dbName, tableName);
    eliminarColumnasDeTabla(dbName, tableName);
    eliminarIndicesDeTabla(dbName, tableName);

    // Descartar de memoria los indices de esta tabla (mapa por nombre)
    for (auto it = indices.begin(); it != indices.end(); ) {
        const std::string& key = it->first;
        size_t pos1 = key.find('/');
        size_t pos2 = key.find('/', pos1 + 1);
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string db = key.substr(0, pos1);
            std::string table = key.substr(pos1 + 1, pos2 - pos1 - 1);
            if (db == dbName && table == tableName) {
                it = indices.erase(it);
                continue;
            }
        }
        ++it;
    }
    // Lo mismo en el mapa por columna
    for (auto it = indicePorColumna.begin(); it != indicePorColumna.end(); ) {
        const std::string& key = it->first;
        size_t pos1 = key.find('/');
        size_t pos2 = key.find('/', pos1 + 1);
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string db = key.substr(0, pos1);
            std::string table = key.substr(pos1 + 1, pos2 - pos1 - 1);
            if (db == dbName && table == tableName) {
                it = indicePorColumna.erase(it);
                continue;
            }
        }
        ++it;
    }

    r.success = true;
    r.type = "ddl";
    r.message = "Table dropped successfully";
    return r;
}