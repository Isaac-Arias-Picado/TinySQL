#pragma once
#include <string>
#include <set>
#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include "Types.h"
#include "Index.h"

// Ruta base donde se guardan las bases de datos. Se define una sola vez en
// StoredDataManager.cpp y se comparte con los demas archivos de la clase.
extern const std::string DATA_DIR;

// Capa de acceso a disco. Lee y escribe los archivos binarios de las tablas
// y mantiene los indices en memoria.
class StoredDataManager {
public:
    StoredDataManager();

    // Operaciones sobre bases de datos y tablas (DDL)
    QueryResult crearBaseDatos(const std::string& nombre);
    QueryResult eliminarBaseDatos(const std::string& nombre);
    bool existeBaseDatos(const std::string& nombre) const;
    QueryResult crearTabla(const std::string& dbName,
        const std::string& tableName,
        const std::vector<Columna>& columnas);
    QueryResult eliminarTabla(const std::string& dbName,
        const std::string& tableName);

    // Creacion de indices (BST o BTREE) sobre una columna
    QueryResult crearIndice(const std::string& dbName,
        const std::string& tableName,
        const std::string& indexName,
        const std::string& columnName,
        const std::string& tipoArbol);

    // Operaciones sobre las filas (DML)
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
    // Nombres de las bases de datos existentes
    std::set<std::string> databases;
    // Indices en memoria. Clave: "db/tabla/nombreIndice"
    std::unordered_map<std::string, std::unique_ptr<Index>> indices;
    // Acceso rapido al indice por columna. Clave: "db/tabla/columna"
    std::unordered_map<std::string, Index*> indicePorColumna;

    void cargarBasesDeDatos();
    void cargarIndices();
    bool esFechaValida(const std::string& fecha);

    Index* obtenerIndice(const std::string& dbName,
        const std::string& tableName,
        const std::string& columnName) const;

    // Lee todas las filas de una tabla con el offset (posicion en bytes) de cada
    // una. El offset es lo que guardan los indices para llegar directo a la fila.
    std::vector<std::pair<std::vector<std::string>, size_t>>
        leerFilasConOffset(const std::string& dbName,
            const std::string& tableName);

    // Lee UNA sola fila directo desde su offset, sin recorrer toda la tabla.
    // Es lo que hace rapida la busqueda cuando hay indice.
    std::vector<std::string> leerFilaEnOffset(const std::string& dbName,
        const std::string& tableName, size_t offset);
};