#include "StoredDataManager.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Definicion de la ruta base compartida (declarada como extern en el header)
const std::string DATA_DIR = "./data";

// Al crear el manager se cargan las bases de datos existentes y reconstruimos los indices registrados en el catalogo.
StoredDataManager::StoredDataManager() {
    cargarBasesDeDatos();
    cargarIndices();
}

// Recorre la carpeta data y registra como bases de datos todas las carpetas
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