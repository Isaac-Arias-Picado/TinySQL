#include "StoredDataManager.h"
#include "SystemCatalog.h"
#include "BSTIndex.h"
#include "BTreeIndex.h"
#include <iostream>
#include <set>
#include <functional>

// Al arrancar el servidor, vuelve a construir en memoria los indices que estan registrados en el System Catalog.
void StoredDataManager::cargarIndices() {
    auto indicesInfo = leerIndices();
    for (const auto& info : indicesInfo) {
        std::unique_ptr<Index> idx;
        if (info.tipoArbol == TipoArbol::BST) {
            idx.reset(new BSTIndex(info.tipoColumna));
        }
        else if (info.tipoArbol == TipoArbol::BTREE) {
            idx.reset(new BTreeIndex(info.tipoColumna));
        }
        else {
            continue;
        }

        // Ubicar la posicion de la columna indexada dentro de la tabla
        auto filasConOffset = leerFilasConOffset(info.dbName, info.tableName);
        std::vector<Columna> columnas = leerColumnas(info.dbName, info.tableName);
        int colIndex = -1;
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].name == info.columnName) {
                colIndex = static_cast<int>(i);
                break;
            }
        }
        if (colIndex == -1) {
            std::cerr << "Error: columna " << info.columnName
                << " no encontrada en tabla " << info.tableName << std::endl;
            continue;
        }

        // Llenar el arbol con los datos ya existentes en disco
        for (const auto& par : filasConOffset) {
            const auto& fila = par.first;
            size_t offset = par.second;
            Key key(fila[colIndex], info.tipoColumna);
            idx->insertar(key, offset);
        }

        // Registrar el indice en los dos mapas (por nombre y por columna)
        std::string key = info.dbName + "/" + info.tableName + "/" + info.indexName;
        indices[key] = std::move(idx);
        std::string colKey = info.dbName + "/" + info.tableName + "/" + info.columnName;
        indicePorColumna[colKey] = indices[key].get();

        std::cout << "Indice cargado: " << key << std::endl;
    }
}

// Devuelve el indice asociado a una columna, o nullptr si esa columna no tiene.
Index* StoredDataManager::obtenerIndice(const std::string& dbName,
    const std::string& tableName, const std::string& columnName) const {
    std::string key = dbName + "/" + tableName + "/" + columnName;
    auto it = indicePorColumna.find(key);
    if (it != indicePorColumna.end()) {
        return it->second;
    }
    return nullptr;
}

// CREATE INDEX: crea un arbol (BST o BTREE) sobre una columna.
QueryResult StoredDataManager::crearIndice(const std::string& dbName,
    const std::string& tableName,
    const std::string& indexName,
    const std::string& columnName,
    const std::string& tipoArbolStr) {
    QueryResult r;
    if (dbName.empty() || tableName.empty() || indexName.empty() || columnName.empty()) {
        r.error = "Invalid index parameters";
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

    // Ubicar la columna y su tipo
    int colIndex = -1;
    TipoColumna tipoCol = TipoColumna::VARCHAR;
    for (size_t i = 0; i < columnas.size(); ++i) {
        if (columnas[i].name == columnName) {
            colIndex = static_cast<int>(i);
            tipoCol = columnas[i].type;
            break;
        }
    }
    if (colIndex == -1) {
        r.error = "Column does not exist";
        return r;
    }

    // Revisar que no haya valores repetidos en la columna a indexar.
    auto filasConOffset = leerFilasConOffset(dbName, tableName);
    std::set<Key, std::function<bool(const Key&, const Key&)>> keysSet(
        [tipoCol](const Key& a, const Key& b) { return a < b; });
    for (const auto& par : filasConOffset) {
        Key key(par.first[colIndex], tipoCol);
        if (keysSet.find(key) != keysSet.end()) {
            r.error = "Index cannot be created: duplicate values in column";
            return r;
        }
        keysSet.insert(key);
    }

    // Crear el arbol del tipo pedido
    std::unique_ptr<Index> idx;
    if (tipoArbolStr == "BST") {
        idx.reset(new BSTIndex(tipoCol));
    }
    else if (tipoArbolStr == "BTREE") {
        idx.reset(new BTreeIndex(tipoCol));
    }
    else {
        r.error = "Unsupported index type. Use BST or BTREE";
        return r;
    }

    // Llenar el arbol con los datos existentes
    for (const auto& par : filasConOffset) {
        Key key(par.first[colIndex], tipoCol);
        idx->insertar(key, par.second);
    }

    // Registrar en los mapas en memoria
    std::string key = dbName + "/" + tableName + "/" + indexName;
    indices[key] = std::move(idx);
    std::string colKey = dbName + "/" + tableName + "/" + columnName;
    indicePorColumna[colKey] = indices[key].get();

    // Guardar en el catalogo para reconstruirlo al reiniciar el servidor
    escribirIndice(dbName, tableName, indexName, columnName,
        static_cast<int>(tipoCol), tipoArbolStr);

    r.success = true;
    r.type = "ddl";
    r.message = "Index created successfully";
    return r;
}