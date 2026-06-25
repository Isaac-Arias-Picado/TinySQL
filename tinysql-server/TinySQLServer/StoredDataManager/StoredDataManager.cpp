#include "StoredDataManager.h"
#include "SystemCatalog.h"
#include "Cifrado.h"
#include "BSTIndex.h"
#include "BTreeIndex.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <regex>
#include <functional>
#include <algorithm>
#include <memory>

namespace fs = std::filesystem;
static const std::string DATA_DIR = "./data";

// Constructor
StoredDataManager::StoredDataManager() {
    cargarBasesDeDatos();
    cargarIndices();
}

// ================== Carga de bases de datos ==================
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

// ================== Carga de índices ==================
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

        // Construir el índice recorriendo todas las filas de la tabla
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
            std::cerr << "Error: columna " << info.columnName << " no encontrada en tabla " << info.tableName << std::endl;
            continue;
        }

        for (const auto& par : filasConOffset) {
            const auto& fila = par.first;
            size_t offset = par.second;
            Key key(fila[colIndex], info.tipoColumna);
            idx->insertar(key, offset);
        }

        // Guardar en los mapas
        std::string key = info.dbName + "/" + info.tableName + "/" + info.indexName;
        indices[key] = std::move(idx);
        // También guardar por columna para búsqueda rápida
        std::string colKey = info.dbName + "/" + info.tableName + "/" + info.columnName;
        indicePorColumna[colKey] = indices[key].get();

        std::cout << "Índice cargado: " << key << std::endl;
    }
}

// ================== Obtener índice por columna ==================
Index* StoredDataManager::obtenerIndice(const std::string& dbName, const std::string& tableName, const std::string& columnName) const {
    std::string key = dbName + "/" + tableName + "/" + columnName;
    auto it = indicePorColumna.find(key);
    if (it != indicePorColumna.end()) {
        return it->second;
    }
    return nullptr;
}

// ================== Verificar existencia de BD ==================
bool StoredDataManager::existeBaseDatos(const std::string& nombre) const {
    return databases.count(nombre) > 0;
}

// ================== CREATE DATABASE ==================
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

// ================== DROP DATABASE ==================
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

// ================== CREATE TABLE ==================
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

// ================== DROP TABLE ==================
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

    eliminarTablaDelCatalogo(dbName, tableName);
    eliminarColumnasDeTabla(dbName, tableName);
    eliminarIndicesDeTabla(dbName, tableName); // eliminar del SystemCatalog

    // Eliminar índices en memoria
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
    // También limpiar indicePorColumna
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

// ================== CREATE INDEX ==================
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

    int colIndex = -1;
    TipoColumna tipoCol;
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

    // Verificar duplicados
    auto filasConOffset = leerFilasConOffset(dbName, tableName);
    std::set<Key, std::function<bool(const Key&, const Key&)>> keysSet(
        [tipoCol](const Key& a, const Key& b) { return a < b; });
    for (const auto& par : filasConOffset) {
        const auto& fila = par.first;
        Key key(fila[colIndex], tipoCol);
        if (keysSet.find(key) != keysSet.end()) {
            r.error = "Index cannot be created: duplicate values in column";
            return r;
        }
        keysSet.insert(key);
    }

    // Crear índice en memoria
    std::unique_ptr<Index> idx;
    TipoArbol tipoArbol;
    if (tipoArbolStr == "BST") {
        idx.reset(new BSTIndex(tipoCol));
        tipoArbol = TipoArbol::BST;
    }
    else if (tipoArbolStr == "BTREE") {
        idx.reset(new BTreeIndex(tipoCol));
        tipoArbol = TipoArbol::BTREE;
    }
    else {
        r.error = "Unsupported index type. Use BST or BTREE";
        return r;
    }

    for (const auto& par : filasConOffset) {
        const auto& fila = par.first;
        size_t offset = par.second;
        Key key(fila[colIndex], tipoCol);
        idx->insertar(key, offset);
    }

    std::string key = dbName + "/" + tableName + "/" + indexName;
    indices[key] = std::move(idx);
    std::string colKey = dbName + "/" + tableName + "/" + columnName;
    indicePorColumna[colKey] = indices[key].get();

    // Escribir en SystemCatalog
    escribirIndice(dbName, tableName, indexName, columnName, static_cast<int>(tipoCol), tipoArbolStr);

    r.success = true;
    r.type = "ddl";
    r.message = "Index created successfully";
    return r;
}

// ================== INSERT ==================
QueryResult StoredDataManager::insertarFila(const std::string& dbName,
    const std::string& tableName,
    const std::vector<std::string>& valores) {
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

    if (valores.size() != columnas.size()) {
        r.error = "Column count does not match value count";
        return r;
    }

    // Validar tipos y fechas
    for (size_t i = 0; i < columnas.size(); ++i) {
        const Columna& col = columnas[i];
        const std::string& valor = valores[i];
        if (col.type == TipoColumna::DATETIME && !esFechaValida(valor)) {
            r.error = "Invalid datetime format: " + valor;
            return r;
        }
    }

    // Verificar unicidad en columnas con índice
    for (size_t i = 0; i < columnas.size(); ++i) {
        Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
        if (idx) {
            Key key(valores[i], columnas[i].type);
            if (idx->existe(key)) {
                r.error = "Duplicate value in indexed column: " + columnas[i].name;
                return r;
            }
        }
    }

    // Calcular tamaño y escribir
    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    std::vector<char> registro(registroSize, 0);
    size_t pos = 0;
    for (size_t i = 0; i < columnas.size(); ++i) {
        const Columna& col = columnas[i];
        const std::string& valor = valores[i];
        if (col.type == TipoColumna::INTEGER) {
            int num = std::stoi(valor);
            std::memcpy(registro.data() + pos, &num, sizeof(int));
            pos += sizeof(int);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double num = std::stod(valor);
            std::memcpy(registro.data() + pos, &num, sizeof(double));
            pos += sizeof(double);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            for (int j = 0; j < (int)valor.size() && j < col.size; ++j)
                registro[pos + j] = valor[j];
            pos += col.size;
        }
        else if (col.type == TipoColumna::DATETIME) {
            for (int j = 0; j < (int)valor.size() && j < 19; ++j)
                registro[pos + j] = valor[j];
            pos += 19;
        }
    }

    encriptar(registro.data(), (int)registroSize);

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ofstream file(rutaTabla, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        r.error = "Cannot open table file";
        return r;
    }

    file.seekp(0, std::ios::end);
    size_t offset = file.tellp();
    file.write(registro.data(), registroSize);
    file.close();

    // Insertar en índices
    for (size_t i = 0; i < columnas.size(); ++i) {
        Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
        if (idx) {
            Key key(valores[i], columnas[i].type);
            idx->insertar(key, offset);
        }
    }

    r.success = true;
    r.type = "dml";
    r.message = "Row inserted";
    return r;
}

// ================== DELETE ==================
QueryResult StoredDataManager::eliminarFilas(const std::string& dbName,
    const std::string& tableName,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue) {
    QueryResult r;

    if (dbName.empty()) { r.error = "No database selected"; return r; }
    if (!databases.count(dbName)) { r.error = "Database does not exist"; return r; }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) { r.error = "Table does not exist"; return r; }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream inFile(rutaTabla, std::ios::binary);
    if (!inFile.is_open()) { r.error = "Cannot open table file"; return r; }

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    std::vector<std::vector<char>> todosLosRegistros;
    std::vector<char> buffer(registroSize);
    while (inFile.read(buffer.data(), registroSize)) {
        encriptar(buffer.data(), (int)registroSize);
        todosLosRegistros.push_back(buffer);
        buffer.assign(registroSize, 0);
    }
    inFile.close();

    if (todosLosRegistros.empty()) {
        r.success = true;
        r.type = "dml";
        r.message = "0 rows deleted (table was empty)";
        r.affected_rows = 0;
        return r;
    }

    int colIndex = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].name == whereColumn) { colIndex = (int)i; break; }
        }
        if (colIndex == -1) {
            r.error = "Column '" + whereColumn + "' does not exist";
            return r;
        }
    }

    // Usar índice si es posible
    std::vector<size_t> offsetsWhere;
    bool usoIndice = false;
    if (colIndex != -1) {
        Index* idx = obtenerIndice(dbName, tableName, whereColumn);
        if (idx) {
            Key key(whereValue, columnas[colIndex].type);
            if (whereOperator == "=") {
                size_t off = idx->buscar(key);
                if (off != static_cast<size_t>(-1)) {
                    offsetsWhere.push_back(off);
                    usoIndice = true;
                }
            }
            else if (whereOperator == ">" || whereOperator == "<") {
                Key keyInicio = (whereOperator == ">") ? key : Key("", columnas[colIndex].type);
                Key keyFin = (whereOperator == "<") ? key : Key("ZZZ", columnas[colIndex].type);
                offsetsWhere = idx->buscarRango(keyInicio, keyFin);
                usoIndice = !offsetsWhere.empty();
            }
            // LIKE, NOT -> secuencial
        }
    }

    auto offsetDe = [&](int idx) -> size_t {
        size_t off = 0;
        for (int i = 0; i < idx; ++i) off += columnas[i].size;
        return off;
        };

    auto extraerValor = [&](const std::vector<char>& reg, int idx) -> std::string {
        const Columna& col = columnas[idx];
        const char* data = reg.data() + offsetDe(idx);
        if (col.type == TipoColumna::INTEGER) {
            int val; std::memcpy(&val, data, sizeof(int)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double val; std::memcpy(&val, data, sizeof(double)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            std::string s;
            for (int j = 0; j < col.size; ++j) { if (data[j] == '\0') break; s.push_back(data[j]); }
            return s;
        }
        else if (col.type == TipoColumna::DATETIME) {
            return std::string(data, 19);
        }
        return "";
        };

    auto cumpleCondicion = [&](const std::vector<char>& reg) -> bool {
        if (whereColumn.empty()) return true;
        std::string valorColumna = extraerValor(reg, colIndex);
        const Columna& col = columnas[colIndex];
        if (whereOperator == "=") return valorColumna == whereValue;
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) > std::stod(whereValue);
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) < std::stod(whereValue);
            return valorColumna < whereValue;
        }
        else if (whereOperator == "LIKE") {
            std::string patron = whereValue;
            for (size_t i = 0; i < patron.size(); ++i) {
                if (patron[i] == '%') { patron.replace(i, 1, ".*"); i += 1; }
            }
            std::regex regex(patron);
            return std::regex_match(valorColumna, regex);
        }
        else if (whereOperator == "NOT") return valorColumna != whereValue;
        return false;
        };

    std::vector<std::vector<char>> nuevosRegistros;
    std::vector<size_t> offsetsEliminados;
    size_t eliminados = 0;

    if (usoIndice) {
        // Eliminar según offsets del índice
        for (size_t i = 0; i < todosLosRegistros.size(); ++i) {
            size_t offsetActual = i * registroSize;
            if (std::find(offsetsWhere.begin(), offsetsWhere.end(), offsetActual) != offsetsWhere.end()) {
                eliminados++;
                offsetsEliminados.push_back(offsetActual);
            }
            else {
                nuevosRegistros.push_back(todosLosRegistros[i]);
            }
        }
    }
    else {
        // Búsqueda secuencial
        for (const auto& reg : todosLosRegistros) {
            if (cumpleCondicion(reg)) {
                eliminados++;
            }
            else {
                nuevosRegistros.push_back(reg);
            }
        }
    }

    // Eliminar claves de índices para las filas borradas
    if (!offsetsEliminados.empty()) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
            if (idx) {
                // Recorrer todos los registros y eliminar las claves que coinciden con los offsets
                for (size_t off : offsetsEliminados) {
                    // Para cada offset, necesitamos la clave. Podemos buscarla en los datos originales.
                    // Como tenemos todosLosRegistros, podemos extraer el valor de la columna i.
                    // Pero es más eficiente buscar en el índice por clave, pero no sabemos la clave.
                    // Lo haremos recorriendo todas las llaves del índice y comparando offsets.
                    // Esto es O(n*m) pero para tablas pequeñas es aceptable.
                    // Mejor: al eliminar, guardamos las claves en un vector.
                    // Por simplicidad, usamos una búsqueda secuencial en el índice.
                    auto todas = idx->obtenerTodasLasLlaves();
                    for (const auto& k : todas) {
                        size_t off = idx->buscar(k);
                        if (off == off) { // siempre verdad, pero necesitamos comparar con el offset
                            // No podemos saber el offset directamente, usamos buscar y comparar.
                            // Mejor recorremos los registros originales y extraemos clave.
                        }
                    }
                }
            }
        }
        // Implementación más simple: recorremos todos los registros eliminados y extraemos clave
        for (size_t off : offsetsEliminados) {
            size_t idxReg = off / registroSize;
            if (idxReg < todosLosRegistros.size()) {
                const auto& reg = todosLosRegistros[idxReg];
                // Extraer todas las claves para actualizar índices
                for (size_t i = 0; i < columnas.size(); ++i) {
                    Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
                    if (idx) {
                        std::string valor = extraerValor(reg, (int)i);
                        Key key(valor, columnas[i].type);
                        idx->eliminar(key);
                    }
                }
            }
        }
    }

    // Reescribir archivo
    std::ofstream outFile(rutaTabla, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) { r.error = "Cannot write table file"; return r; }
    for (auto& reg : nuevosRegistros) {
        encriptar(reg.data(), (int)registroSize);
        outFile.write(reg.data(), registroSize);
    }
    outFile.close();

    r.success = true;
    r.type = "dml";
    r.message = std::to_string(eliminados) + " rows deleted";
    r.affected_rows = (int)eliminados;
    return r;
}

// ================== SELECT ==================
QueryResult StoredDataManager::seleccionarFilas(
    const std::string& dbName,
    const std::string& tableName,
    const std::vector<std::string>& columnas,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue,
    const std::string& orderColumn,
    const std::string& orderDirection) {

    QueryResult r;
    if (dbName.empty()) { r.error = "No database selected"; return r; }
    if (!databases.count(dbName)) { r.error = "Database does not exist"; return r; }

    std::vector<Columna> columnasTabla = leerColumnas(dbName, tableName);
    if (columnasTabla.empty()) { r.error = "Table does not exist"; return r; }

    auto filasConOffset = leerFilasConOffset(dbName, tableName);
    if (filasConOffset.empty()) {
        r.success = true;
        r.type = "select";
        r.message = "0 rows selected";
        return r;
    }

    // Determinar índices de columnas a seleccionar
    std::vector<int> indicesSeleccionados;
    bool seleccionarTodas = columnas.empty() || (columnas.size() == 1 && columnas[0] == "*");
    if (seleccionarTodas) {
        for (size_t i = 0; i < columnasTabla.size(); ++i) indicesSeleccionados.push_back((int)i);
    }
    else {
        for (const auto& col : columnas) {
            bool encontrada = false;
            for (size_t i = 0; i < columnasTabla.size(); ++i) {
                if (columnasTabla[i].name == col) {
                    indicesSeleccionados.push_back((int)i);
                    encontrada = true;
                    break;
                }
            }
            if (!encontrada) { r.error = "Column '" + col + "' does not exist"; return r; }
        }
    }

    // Usar índice para WHERE si es posible
    std::vector<size_t> offsetsFiltrados;
    bool usoIndice = false;
    int colIndexWhere = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            if (columnasTabla[i].name == whereColumn) { colIndexWhere = (int)i; break; }
        }
        if (colIndexWhere == -1) { r.error = "Column '" + whereColumn + "' does not exist"; return r; }

        Index* idx = obtenerIndice(dbName, tableName, whereColumn);
        if (idx) {
            Key key(whereValue, columnasTabla[colIndexWhere].type);
            if (whereOperator == "=") {
                size_t off = idx->buscar(key);
                if (off != static_cast<size_t>(-1)) offsetsFiltrados.push_back(off);
            }
            else if (whereOperator == ">" || whereOperator == "<") {
                Key keyInicio = (whereOperator == ">") ? key : Key("", columnasTabla[colIndexWhere].type);
                Key keyFin = (whereOperator == "<") ? key : Key("ZZZ", columnasTabla[colIndexWhere].type);
                offsetsFiltrados = idx->buscarRango(keyInicio, keyFin);
            }
            else {
                // LIKE, NOT -> secuencial
            }
            usoIndice = !offsetsFiltrados.empty();
        }
    }

    // Función auxiliar para extraer valor de una fila
    auto extraerValor = [&](const std::vector<std::string>& fila, int idx) -> std::string {
        return (idx >= 0 && idx < (int)fila.size()) ? fila[idx] : "";
        };

    // Evaluar condición WHERE (para búsqueda secuencial)
    auto cumpleCondicion = [&](const std::vector<std::string>& fila) -> bool {
        if (whereColumn.empty()) return true;
        std::string valorColumna = extraerValor(fila, colIndexWhere);
        const Columna& col = columnasTabla[colIndexWhere];
        if (whereOperator == "=") return valorColumna == whereValue;
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) > std::stod(whereValue);
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) < std::stod(whereValue);
            return valorColumna < whereValue;
        }
        else if (whereOperator == "LIKE") {
            std::string patron = whereValue;
            for (size_t i = 0; i < patron.size(); ++i) {
                if (patron[i] == '%') { patron.replace(i, 1, ".*"); i += 1; }
            }
            std::regex regex(patron);
            return std::regex_match(valorColumna, regex);
        }
        else if (whereOperator == "NOT") return valorColumna != whereValue;
        return false;
        };

    std::vector<std::vector<std::string>> filasResultado;

    if (usoIndice) {
        // Usar offsets del índice
        for (size_t off : offsetsFiltrados) {
            for (const auto& par : filasConOffset) {
                if (par.second == off) {
                    std::vector<std::string> filaSeleccionada;
                    for (int idx : indicesSeleccionados) {
                        filaSeleccionada.push_back(par.first[idx]);
                    }
                    filasResultado.push_back(filaSeleccionada);
                    break;
                }
            }
        }
    }
    else {
        // Búsqueda secuencial
        for (const auto& par : filasConOffset) {
            if (cumpleCondicion(par.first)) {
                std::vector<std::string> filaSeleccionada;
                for (int idx : indicesSeleccionados) {
                    filaSeleccionada.push_back(par.first[idx]);
                }
                filasResultado.push_back(filaSeleccionada);
            }
        }
    }

    // ORDER BY (Quicksort)
    if (!orderColumn.empty()) {
        int idxOrder = -1;
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            if (columnasTabla[i].name == orderColumn) { idxOrder = (int)i; break; }
        }
        if (idxOrder == -1) { r.error = "Column '" + orderColumn + "' does not exist for ORDER BY"; return r; }

        std::function<void(std::vector<std::vector<std::string>>&, int, int)> quicksort =
            [&](std::vector<std::vector<std::string>>& arr, int left, int right) {
            if (left >= right) return;
            int mid = (left + right) / 2;
            const Columna& colOrder = columnasTabla[idxOrder];
            std::string pivotStr = arr[mid][idxOrder];
            auto menor = [&](const std::string& a, const std::string& b) -> bool {
                if (colOrder.type == TipoColumna::INTEGER || colOrder.type == TipoColumna::DOUBLE)
                    return std::stod(a) < std::stod(b);
                return a < b;
                };
            int i = left, j = right;
            while (i <= j) {
                while (menor(arr[i][idxOrder], pivotStr)) i++;
                while (menor(pivotStr, arr[j][idxOrder])) j--;
                if (i <= j) {
                    std::swap(arr[i], arr[j]);
                    i++; j--;
                }
            }
            quicksort(arr, left, j);
            quicksort(arr, i, right);
            };
        quicksort(filasResultado, 0, (int)filasResultado.size() - 1);
        if (orderDirection == "DESC") {
            std::reverse(filasResultado.begin(), filasResultado.end());
        }
    }

    r.success = true;
    r.type = "select";
    r.message = std::to_string(filasResultado.size()) + " rows selected";
    for (int idx : indicesSeleccionados) r.columns.push_back(columnasTabla[idx].name);
    r.rows = std::move(filasResultado);
    return r;
}

// ================== UPDATE ==================
QueryResult StoredDataManager::actualizarFilas(const std::string& dbName,
    const std::string& tableName,
    const std::string& setColumn,
    const std::string& setValue,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue) {
    QueryResult r;
    if (dbName.empty()) { r.error = "No database selected"; return r; }
    if (!databases.count(dbName)) { r.error = "Database does not exist"; return r; }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) { r.error = "Table does not exist"; return r; }

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    int setIndex = -1;
    for (size_t i = 0; i < columnas.size(); ++i) {
        if (columnas[i].name == setColumn) { setIndex = (int)i; break; }
    }
    if (setIndex == -1) { r.error = "Column '" + setColumn + "' does not exist"; return r; }

    int whereIndex = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].name == whereColumn) { whereIndex = (int)i; break; }
        }
        if (whereIndex == -1) { r.error = "Column '" + whereColumn + "' does not exist"; return r; }
    }

    if (columnas[setIndex].type == TipoColumna::DATETIME && !esFechaValida(setValue)) {
        r.error = "Invalid datetime format: " + setValue;
        return r;
    }

    // Leer todos los registros
    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream inFile(rutaTabla, std::ios::binary);
    if (!inFile.is_open()) { r.error = "Cannot open table file"; return r; }

    std::vector<std::vector<char>> registros;
    std::vector<char> buffer(registroSize);
    while (inFile.read(buffer.data(), registroSize)) {
        encriptar(buffer.data(), (int)registroSize);
        registros.push_back(buffer);
        buffer.assign(registroSize, 0);
    }
    inFile.close();

    auto offsetDe = [&](int idx) -> size_t {
        size_t off = 0;
        for (int i = 0; i < idx; ++i) off += columnas[i].size;
        return off;
        };

    auto extraerValor = [&](const std::vector<char>& reg, int idx) -> std::string {
        const Columna& col = columnas[idx];
        const char* data = reg.data() + offsetDe(idx);
        if (col.type == TipoColumna::INTEGER) {
            int val; std::memcpy(&val, data, sizeof(int)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double val; std::memcpy(&val, data, sizeof(double)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            std::string s;
            for (int j = 0; j < col.size; ++j) { if (data[j] == '\0') break; s.push_back(data[j]); }
            return s;
        }
        else if (col.type == TipoColumna::DATETIME) {
            return std::string(data, 19);
        }
        return "";
        };

    auto cumpleCondicion = [&](const std::vector<char>& reg) -> bool {
        if (whereColumn.empty()) return true;
        std::string valorColumna = extraerValor(reg, whereIndex);
        const Columna& col = columnas[whereIndex];
        if (whereOperator == "=") return valorColumna == whereValue;
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) > std::stod(whereValue);
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) < std::stod(whereValue);
            return valorColumna < whereValue;
        }
        else if (whereOperator == "NOT") return valorColumna != whereValue;
        return false;
        };

    auto escribirValorEnRegistro = [&](std::vector<char>& reg) {
        const Columna& col = columnas[setIndex];
        char* data = reg.data() + offsetDe(setIndex);
        if (col.type == TipoColumna::INTEGER) {
            int num = std::stoi(setValue);
            std::memcpy(data, &num, sizeof(int));
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double num = std::stod(setValue);
            std::memcpy(data, &num, sizeof(double));
        }
        else if (col.type == TipoColumna::VARCHAR) {
            for (int j = 0; j < col.size; ++j) data[j] = 0;
            for (int j = 0; j < (int)setValue.size() && j < col.size; ++j) data[j] = setValue[j];
        }
        else if (col.type == TipoColumna::DATETIME) {
            for (int j = 0; j < 19; ++j) data[j] = 0;
            for (int j = 0; j < (int)setValue.size() && j < 19; ++j) data[j] = setValue[j];
        }
        };

    size_t actualizadas = 0;
    for (auto& reg : registros) {
        if (cumpleCondicion(reg)) {
            // Actualizar índices si la columna SET tiene índice
            Index* idxSet = obtenerIndice(dbName, tableName, setColumn);
            std::string valorViejo;
            if (idxSet) {
                valorViejo = extraerValor(reg, setIndex);
            }
            escribirValorEnRegistro(reg);
            actualizadas++;
            if (idxSet) {
                // Eliminar clave vieja, insertar nueva
                Key keyVieja(valorViejo, columnas[setIndex].type);
                Key keyNueva(setValue, columnas[setIndex].type);
                idxSet->eliminar(keyVieja);
                idxSet->insertar(keyNueva, (size_t)0); // offset no importa aquí, se reescribe
            }
        }
    }

    // Reescribir archivo
    std::ofstream outFile(rutaTabla, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) { r.error = "Cannot write table file"; return r; }
    size_t offsetActual = 0;
    for (auto& reg : registros) {
        encriptar(reg.data(), (int)registroSize);
        outFile.write(reg.data(), registroSize);
        // Actualizar offsets en índices (si no se actualizó la columna SET)
        // Para simplificar, asumimos que los offsets no cambian
        offsetActual += registroSize;
    }
    outFile.close();

    r.success = true;
    r.type = "dml";
    r.message = std::to_string(actualizadas) + " rows updated";
    r.affected_rows = (int)actualizadas;
    return r;
}

// ================== leerFilasConOffset ==================
std::vector<std::pair<std::vector<std::string>, size_t>>
StoredDataManager::leerFilasConOffset(const std::string& dbName,
    const std::string& tableName) {
    std::vector<std::pair<std::vector<std::string>, size_t>> resultado;
    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) return resultado;

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;
    if (registroSize == 0) return resultado;

    std::string ruta = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream file(ruta, std::ios::binary);
    if (!file.is_open()) return resultado;

    std::vector<char> buffer(registroSize);
    size_t offset = 0;
    while (file.read(buffer.data(), registroSize)) {
        encriptar(buffer.data(), (int)registroSize);
        std::vector<std::string> fila;
        size_t pos = 0;
        for (const auto& col : columnas) {
            std::string valor;
            if (col.type == TipoColumna::INTEGER) {
                int num; std::memcpy(&num, buffer.data() + pos, sizeof(int));
                valor = std::to_string(num);
                pos += sizeof(int);
            }
            else if (col.type == TipoColumna::DOUBLE) {
                double num; std::memcpy(&num, buffer.data() + pos, sizeof(double));
                valor = std::to_string(num);
                pos += sizeof(double);
            }
            else if (col.type == TipoColumna::VARCHAR) {
                valor = std::string(buffer.data() + pos, col.size);
                valor = std::string(valor.c_str());
                pos += col.size;
            }
            else if (col.type == TipoColumna::DATETIME) {
                valor = std::string(buffer.data() + pos, 19);
                pos += 19;
            }
            fila.push_back(valor);
        }
        resultado.push_back({ fila, offset });
        offset += registroSize;
    }
    file.close();
    return resultado;
}

// ================== Validar fecha ==================
bool StoredDataManager::esFechaValida(const std::string& fecha) {
    if (fecha.size() != 19) return false;
    if (fecha[4] != '-' || fecha[7] != '-' || fecha[10] != ' ') return false;
    if (fecha[13] != ':' || fecha[16] != ':') return false;
    return true;
}