#pragma once
#include <string>
#include <set>
#include <vector>
#include <utility>
#include "Types.h"

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
        const std::string& orderDirection   // "ASC" o "DESC"
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

    void cargarBasesDeDatos();
    bool esFechaValida(const std::string& fecha);

    std::vector<std::pair<std::vector<std::string>, size_t>>
        leerFilasConOffset(const std::string& dbName,
            const std::string& tableName);
};