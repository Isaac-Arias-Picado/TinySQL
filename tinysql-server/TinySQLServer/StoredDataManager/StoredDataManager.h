#pragma once
#include <string>
#include <set>
#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include "Types.h"
#include "Index.h"

class StoredDataManager {
public:
    StoredDataManager();

    // DDL
    QueryResult crearBaseDatos(const std::string& nombre);
    QueryResult eliminarBaseDatos(const std::string& nombre);
    bool existeBaseDatos(const std::string& nombre) const;
    QueryResult crearTabla(const std::string& dbName,
        const std::string& tableName,
        const std::vector<Columna>& columnas);
    QueryResult eliminarTabla(const std::string& dbName,
        const std::string& tableName);

    // Índices
    QueryResult crearIndice(const std::string& dbName,
        const std::string& tableName,
        const std::string& indexName,
        const std::string& columnName,
        const std::string& tipoArbol); // "BST" o "BTREE"

    // DML
    QueryResult insertarFila(const std::string& dbName,
        const std::string& tableName,
        const std::vector<std::string>& valores);
    QueryResult eliminarFilas(const std::string& dbName,
        const std::string& tableName,
        const std::string& whereColumn,
        const std::string& whereOperator,
        const std::string& whereValue);
    QueryResult seleccionarFilas(
        const std::string& dbName,
        const std::string& tableName,
        const std::vector<std::string>& columnas,
        const std::string& whereColumn,
        const std::string& whereOperator,
        const std::string& whereValue,
        const std::string& orderColumn,
        const std::string& orderDirection
    );
    QueryResult actualizarFilas(const std::string& dbName,
        const std::string& tableName,
        const std::string& setColumn,
        const std::string& setValue,
        const std::string& whereColumn,
        const std::string& whereOperator,
        const std::string& whereValue);

private:
    std::set<std::string> databases;
    // Índices en memoria: clave = "db/table/indexName"
    std::unordered_map<std::string, std::unique_ptr<Index>> indices;
    // Mapa auxiliar para buscar índice por columna: clave = "db/table/column"
    std::unordered_map<std::string, Index*> indicePorColumna;

    void cargarBasesDeDatos();
    void cargarIndices();
    bool esFechaValida(const std::string& fecha);

    Index* obtenerIndice(const std::string& dbName, const std::string& tableName, const std::string& columnName) const;

    std::vector<std::pair<std::vector<std::string>, size_t>>
        leerFilasConOffset(const std::string& dbName,
            const std::string& tableName);
};