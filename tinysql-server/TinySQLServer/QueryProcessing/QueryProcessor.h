#pragma once
#include <string>
#include <vector>
#include "../StoredDataManager/Types.h"
#include "../StoredDataManager/StoredDataManager.h"

class QueryProcessor {
public:
    QueryResult execute(const std::string& sql, const std::string& dbContext);

private:
    StoredDataManager storage;

    std::string limpiarNombre(const std::string& raw);

    // CREATE TABLE
    std::string extraerNombreTabla(const std::string& sql);
    std::string extraerBloqueColumnas(const std::string& sql);
    std::vector<std::string> partirPorComas(const std::string& bloque);
    TipoColumna parsearTipo(const std::string& tipoTexto);
    Columna parsearColumna(const std::string& texto);

    // INSERT
    std::string extraerNombreTablaInsert(const std::string& sql);
    std::vector<std::string> extraerValores(const std::string& sql);

    // DROP TABLE
    std::string extraerNombreTablaDrop(const std::string& sql);
};