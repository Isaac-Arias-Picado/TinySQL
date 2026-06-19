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
    std::string extraerNombreTabla(const std::string& sql);
    std::string extraerBloqueColumnas(const std::string& sql);
    std::vector<std::string> partirPorComas(const std::string& bloque);
    TipoColumna parsearTipo(const std::string& tipoTexto);
    Columna parsearColumna(const std::string& texto);
    std::string extraerNombreTablaInsert(const std::string& sql);
    std::vector<std::string> extraerValores(const std::string& sql);
};