#include "StoredDataManager.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include "SystemCatalog.h"

namespace fs = std::filesystem;
static const std::string DATA_DIR = "./data";
StoredDataManager::StoredDataManager() {
    cargarBasesDeDatos();
}
void StoredDataManager::cargarBasesDeDatos() {
    if (!fs::exists(DATA_DIR)) fs::create_directory(DATA_DIR);
    for (const auto& entry : fs::directory_iterator(DATA_DIR)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if (name != "SystemCatalog") {
                databases.insert(name);
            }
        }
    }
    std::cout << "Bases de datos cargadas: ";
    for (const auto& db : databases) std::cout << db << " ";
    std::cout << std::endl;
}
bool StoredDataManager::existeBaseDatos(const std::string& nombre) const {
    return databases.count(nombre) > 0;
}
QueryResult StoredDataManager::crearBaseDatos(const std::string& nombre) {
    QueryResult r;
    if (databases.count(nombre)) { r.error = "Database already exists"; return r; }
    fs::create_directory(DATA_DIR + "/" + nombre);
    databases.insert(nombre);
    escribirBaseDatos(nombre);
    r.success = true; r.type = "ddl"; r.message = "Database created"; return r;
}
QueryResult StoredDataManager::eliminarBaseDatos(const std::string& nombre) {
    QueryResult r;
    if (!databases.count(nombre)) { r.error = "Database does not exist"; return r; }
    fs::remove_all(DATA_DIR + "/" + nombre);
    databases.erase(nombre);
    r.success = true; r.type = "ddl"; r.message = "Database dropped"; return r;
}

QueryResult StoredDataManager::crearTabla(const std::string& dbName,
    const std::string& tableName,
    const std::vector<Columna>& columnas) {
    QueryResult r;

    // Se valida que se le hizo el SET a la base de datos
    if (dbName.empty()) {
        r.error = "No se selecciono ninguna base de datos";
        return r;
    }
    // Se confirma que la base de datos exista
    if (!databases.count(dbName)) {
        r.error = "Base de datos no existe";
        return r;
    }
    // Se crear el archivo binario 
    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ofstream file(rutaTabla, std::ios::binary | std::ios::app);
    file.close();

    // Se registra en el catalogo
    escribirTabla(dbName, tableName);
    for (int i = 0; i < (int)columnas.size(); i++) {
        escribirColumna(dbName, tableName, columnas[i], i);
    }

    r.success = true;
    r.type = "ddl";
    r.message = "Table created";
    return r;
}