#pragma once
#include <string>
#include <vector>
#include "../StoredDataManager/Types.h"
#include "../StoredDataManager/StoredDataManager.h"

// Recibe el SQL en texto, lo interpreta y delega
class QueryProcessor {
public:
    QueryResult execute(const std::string& sql, const std::string& dbContext);

private:
    StoredDataManager storage;

    // Quita espacios, tabs y el punto y coma final de un fragmento
    std::string limpiarNombre(const std::string& raw);

    // Parseo de sentencias que definen estructura (CREATE TABLE, DROP, INDEX)
    std::string extraerNombreTabla(const std::string& sql);
    std::string extraerBloqueColumnas(const std::string& sql);
    std::vector<std::string> partirPorComas(const std::string& bloque);
    TipoColumna parsearTipo(const std::string& tipoTexto);
    Columna parsearColumna(const std::string& texto);
    std::string extraerNombreTablaDrop(const std::string& sql);
    std::string extraerNombreIndice(const std::string& sql);
    std::string extraerNombreTablaIndex(const std::string& sql);
    std::string extraerColumnaIndex(const std::string& sql);
    std::string extraerTipoArbol(const std::string& sql);

    // Parseo de sentencias que manipulan datos (INSERT)
    std::string extraerNombreTablaInsert(const std::string& sql);
    std::vector<std::string> extraerValores(const std::string& sql);
};