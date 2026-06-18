#include "QueryProcessor.h"
#include <cctype>
std::string QueryProcessor::limpiarNombre(const std::string& raw) {
    std::string nombre = raw;
    size_t start = nombre.find_first_not_of(" \t");
    if (start != std::string::npos) nombre = nombre.substr(start);
    size_t end = nombre.find_last_not_of(" \t");
    if (end != std::string::npos) nombre = nombre.substr(0, end + 1);
    if (!nombre.empty() && nombre.back() == ';') nombre.pop_back();
    return nombre;
}
QueryResult QueryProcessor::execute(const std::string& sql, const std::string& dbContext) {
    std::string cmd = sql;
    for (auto& c : cmd) c = toupper((unsigned char)c);
    QueryResult r;
    if (cmd.rfind("CREATE DATABASE", 0) == 0) {
        r = storage.crearBaseDatos(limpiarNombre(sql.substr(15)));
    }
    else if (cmd.rfind("DROP DATABASE", 0) == 0) {
        r = storage.eliminarBaseDatos(limpiarNombre(sql.substr(13)));
    }
    else if (cmd.rfind("SET DATABASE", 0) == 0) {
        std::string nombre = limpiarNombre(sql.substr(12));
        if (storage.existeBaseDatos(nombre)) {
            r.success = true; r.type = "ddl"; r.message = "Context set to " + nombre;
        }
        else { r.error = "Database does not exist"; }
    }
    else { r.error = "Comando no implementado"; }
    return r;
}