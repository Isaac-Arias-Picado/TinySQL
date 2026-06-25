#include "SystemCatalog.h"
#include "Cifrado.h"
#include <iostream>
#include <array>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>

std::array<char, registrysize> aRegistroFijo(const std::string& name) {
    std::array<char, registrysize> buffer{};
    if (name.size() >= registrysize) {
        std::cout << "Numero de caracteres maximo (63) del nombre alcanzados, se recorto el nombre";
        for (int i = 0; i < registrysize - 1; i++) {
            buffer[i] = name[i];
        }
    }
    else {
        for (int i = 0; i < name.size(); i++) {
            buffer[i] = name[i];
        }
    }
    return buffer;
}

void escribirBaseDatos(const std::string& dbName) {
    std::filesystem::create_directories("./data/SystemCatalog");
    std::array<char, registrysize> buffer = aRegistroFijo(dbName);
    const std::string ruta = "./data/SystemCatalog/SystemDatabases";
    std::ofstream file(ruta, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        std::cout << "Error: No existe aun el archivo SystemDataBase";
        return;
    }
    encriptar(buffer.data(), (int)buffer.size());
    file.write(buffer.data(), buffer.size());
    file.close();
}

std::vector<std::string> leerBasesDatos() {
    std::vector<std::string> nombres;
    const std::string ruta = "./data/SystemCatalog/SystemDatabases";
    std::ifstream file(ruta, std::ios::binary);
    if (!file.is_open()) {
        return nombres;
    }
    std::array<char, registrysize> buffer{};
    while (file.read(buffer.data(), buffer.size())) {
        encriptar(buffer.data(), (int)buffer.size());
        std::string nombre(buffer.data());
        nombres.push_back(nombre);
        buffer.fill(0);
    }
    file.close();
    return nombres;
}

void escribirTabla(const std::string& dbName, const std::string& tableName) {
    std::filesystem::create_directories("./data/SystemCatalog");
    const std::string ruta = "./data/SystemCatalog/SystemTables";
    std::ofstream file(ruta, std::ios::binary | std::ios::app);
    if (!file.is_open()) return;

    const size_t recordSize = registrysize * 2;
    std::vector<char> registro(recordSize, 0);

    std::array<char, registrysize> bufBD = aRegistroFijo(dbName);
    std::array<char, registrysize> bufTabla = aRegistroFijo(tableName);
    std::memcpy(registro.data(), bufBD.data(), registrysize);
    std::memcpy(registro.data() + registrysize, bufTabla.data(), registrysize);

    encriptar(registro.data(), (int)recordSize);
    file.write(registro.data(), recordSize);
    file.close();
}

void escribirColumna(const std::string& dbName, const std::string& tableName,
    const Columna& col, int orden) {
    std::filesystem::create_directories("./data/SystemCatalog");
    const std::string ruta = "./data/SystemCatalog/SystemColumns";
    std::ofstream file(ruta, std::ios::binary | std::ios::app);
    if (!file.is_open()) return;

    const size_t recordSize = registrysize * 3 + sizeof(int) * 4;
    std::vector<char> registro(recordSize, 0);
    size_t pos = 0;

    std::array<char, registrysize> bufBD = aRegistroFijo(dbName);
    std::array<char, registrysize> bufTabla = aRegistroFijo(tableName);
    std::array<char, registrysize> bufCol = aRegistroFijo(col.name);
    std::memcpy(registro.data() + pos, bufBD.data(), registrysize); pos += registrysize;
    std::memcpy(registro.data() + pos, bufTabla.data(), registrysize); pos += registrysize;
    std::memcpy(registro.data() + pos, bufCol.data(), registrysize); pos += registrysize;

    int tipo = static_cast<int>(col.type);
    int size = col.size;
    int nullable = col.nullable ? 1 : 0;
    std::memcpy(registro.data() + pos, &tipo, sizeof(int)); pos += sizeof(int);
    std::memcpy(registro.data() + pos, &size, sizeof(int)); pos += sizeof(int);
    std::memcpy(registro.data() + pos, &nullable, sizeof(int)); pos += sizeof(int);
    std::memcpy(registro.data() + pos, &orden, sizeof(int)); pos += sizeof(int);

    encriptar(registro.data(), (int)recordSize);
    file.write(registro.data(), recordSize);
    file.close();
}

std::vector<Columna> leerColumnas(const std::string& dbName, const std::string& tableName) {
    std::vector<Columna> columnas;
    const std::string ruta = "./data/SystemCatalog/SystemColumns";
    std::ifstream file(ruta, std::ios::binary);
    if (!file.is_open()) return columnas;

    const size_t recordSize = registrysize * 3 + sizeof(int) * 4;
    std::vector<char> registro(recordSize);

    while (file.read(registro.data(), recordSize)) {
        encriptar(registro.data(), (int)recordSize);
        size_t pos = 0;
        std::string dbLeido(registro.data() + pos); pos += registrysize;
        std::string tablaLeida(registro.data() + pos); pos += registrysize;
        std::string colNombre(registro.data() + pos); pos += registrysize;

        int tipo, size, nullable, orden;
        std::memcpy(&tipo, registro.data() + pos, sizeof(int)); pos += sizeof(int);
        std::memcpy(&size, registro.data() + pos, sizeof(int)); pos += sizeof(int);
        std::memcpy(&nullable, registro.data() + pos, sizeof(int)); pos += sizeof(int);
        std::memcpy(&orden, registro.data() + pos, sizeof(int)); pos += sizeof(int);

        if (dbLeido == dbName && tablaLeida == tableName) {
            Columna col;
            col.name = colNombre;
            col.type = static_cast<TipoColumna>(tipo);
            col.size = size;
            col.nullable = (nullable == 1);
            columnas.push_back(col);
        }
    }
    file.close();
    return columnas;
}

// ================== FUNCIONES PARA ÍNDICES ==================

// Estructura interna para escribir/leer índices (tamaño fijo)
static const size_t INDEX_RECORD_SIZE = registrysize * 4 + sizeof(int) + registrysize;
// dbName(64) + tableName(64) + indexName(64) + columnName(64) + tipoColumna(int) + tipoArbol(64)

void escribirIndice(const std::string& dbName,
    const std::string& tableName,
    const std::string& indexName,
    const std::string& columnName,
    int tipoColumna,
    const std::string& tipoArbol) {
    std::filesystem::create_directories("./data/SystemCatalog");
    const std::string ruta = "./data/SystemCatalog/SystemIndexes";
    std::ofstream file(ruta, std::ios::binary | std::ios::app);
    if (!file.is_open()) return;

    const size_t recordSize = registrysize * 4 + sizeof(int) + registrysize;
    std::vector<char> registro(recordSize, 0);
    size_t pos = 0;

    std::array<char, registrysize> bufBD = aRegistroFijo(dbName);
    std::array<char, registrysize> bufTabla = aRegistroFijo(tableName);
    std::array<char, registrysize> bufIndex = aRegistroFijo(indexName);
    std::array<char, registrysize> bufCol = aRegistroFijo(columnName);
    std::array<char, registrysize> bufArbol = aRegistroFijo(tipoArbol);

    std::memcpy(registro.data() + pos, bufBD.data(), registrysize); pos += registrysize;
    std::memcpy(registro.data() + pos, bufTabla.data(), registrysize); pos += registrysize;
    std::memcpy(registro.data() + pos, bufIndex.data(), registrysize); pos += registrysize;
    std::memcpy(registro.data() + pos, bufCol.data(), registrysize); pos += registrysize;

    std::memcpy(registro.data() + pos, &tipoColumna, sizeof(int)); pos += sizeof(int);

    std::memcpy(registro.data() + pos, bufArbol.data(), registrysize); pos += registrysize;

    encriptar(registro.data(), (int)recordSize);
    file.write(registro.data(), recordSize);
    file.close();
}

std::vector<IndiceInfo> leerIndices() {
    std::vector<IndiceInfo> indices;
    const std::string ruta = "./data/SystemCatalog/SystemIndexes";
    std::ifstream file(ruta, std::ios::binary);
    if (!file.is_open()) return indices;

    const size_t recordSize = registrysize * 4 + sizeof(int) + registrysize;
    std::vector<char> registro(recordSize);

    while (file.read(registro.data(), recordSize)) {
        encriptar(registro.data(), (int)recordSize);
        size_t pos = 0;
        std::string db(registro.data() + pos); pos += registrysize;
        std::string table(registro.data() + pos); pos += registrysize;
        std::string idxName(registro.data() + pos); pos += registrysize;
        std::string colName(registro.data() + pos); pos += registrysize;

        int tipoCol;
        std::memcpy(&tipoCol, registro.data() + pos, sizeof(int)); pos += sizeof(int);

        std::string tipoArbol(registro.data() + pos); pos += registrysize;

        IndiceInfo info;
        info.dbName = db;
        info.tableName = table;
        info.indexName = idxName;
        info.columnName = colName;
        info.tipoColumna = static_cast<TipoColumna>(tipoCol);
        info.tipoArbol = (tipoArbol == "BTREE") ? TipoArbol::BTREE : TipoArbol::BST;
        indices.push_back(info);
    }
    file.close();
    return indices;
}

void eliminarIndicesDeTabla(const std::string& dbName, const std::string& tableName) {
    const std::string ruta = "./data/SystemCatalog/SystemIndexes";
    const size_t recordSize = registrysize * 4 + sizeof(int) + registrysize;

    std::ifstream in(ruta, std::ios::binary);
    if (!in.is_open()) return;

    std::vector<std::vector<char>> registros;
    std::vector<char> buffer(recordSize);
    while (in.read(buffer.data(), recordSize)) {
        // Desencriptar para comparar
        std::vector<char> desencriptado = buffer;
        encriptar(desencriptado.data(), (int)recordSize);
        std::string db(desencriptado.data());
        std::string table(desencriptado.data() + registrysize);
        db = std::string(db.c_str());
        table = std::string(table.c_str());
        if (!(db == dbName && table == tableName)) {
            registros.push_back(buffer); // guardar encriptado
        }
        buffer.assign(recordSize, 0);
    }
    in.close();

    std::ofstream out(ruta, std::ios::binary | std::ios::trunc);
    for (const auto& reg : registros) {
        out.write(reg.data(), reg.size());
    }
    out.close();
}

template <typename Predicate>
void reescribirArchivoFiltrado(const std::string& ruta, size_t recordSize, Predicate pred) {
    std::ifstream in(ruta, std::ios::binary);
    if (!in.is_open()) return;

    std::vector<std::vector<char>> registros;
    std::vector<char> buffer(recordSize);
    while (in.read(buffer.data(), recordSize)) {
        std::vector<char> desencriptado = buffer;
        encriptar(desencriptado.data(), (int)recordSize);
        if (!pred(desencriptado)) {
            registros.push_back(buffer);
        }
        buffer.assign(recordSize, 0);
    }
    in.close();

    std::ofstream out(ruta, std::ios::binary | std::ios::trunc);
    for (const auto& reg : registros) {
        out.write(reg.data(), reg.size());
    }
    out.close();
}

void eliminarTablaDelCatalogo(const std::string& dbName, const std::string& tableName) {
    const std::string ruta = "./data/SystemCatalog/SystemTables";
    const size_t recordSize = registrysize * 2;
    reescribirArchivoFiltrado(ruta, recordSize, [&](const std::vector<char>& reg) -> bool {
        std::string db(reg.data(), registrysize);
        std::string tbl(reg.data() + registrysize, registrysize);
        db = std::string(db.c_str());
        tbl = std::string(tbl.c_str());
        return (db == dbName && tbl == tableName);
        });
}

void eliminarColumnasDeTabla(const std::string& dbName, const std::string& tableName) {
    const std::string ruta = "./data/SystemCatalog/SystemColumns";
    const size_t recordSize = registrysize * 3 + sizeof(int) * 4;
    reescribirArchivoFiltrado(ruta, recordSize, [&](const std::vector<char>& reg) -> bool {
        std::string db(reg.data(), registrysize);
        std::string tbl(reg.data() + registrysize, registrysize);
        db = std::string(db.c_str());
        tbl = std::string(tbl.c_str());
        return (db == dbName && tbl == tableName);
        });
}