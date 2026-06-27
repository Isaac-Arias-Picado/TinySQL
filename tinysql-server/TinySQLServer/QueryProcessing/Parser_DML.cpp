#include "QueryProcessor.h"

//INSERT

// El nombre de la tabla esta entre INTO y VALUES (o el "(" si no hay VALUES).
std::string QueryProcessor::extraerNombreTablaInsert(const std::string& sql) {
    size_t inicioInto = sql.find("INTO");
    if (inicioInto == std::string::npos) {
        inicioInto = sql.find("into");
        if (inicioInto == std::string::npos) return "";
    }
    size_t inicioNombre = inicioInto + 4;
    while (inicioNombre < sql.size() && (sql[inicioNombre] == ' ' || sql[inicioNombre] == '\t')) inicioNombre++;
    size_t finNombre = sql.find("VALUES", inicioNombre);
    if (finNombre == std::string::npos) finNombre = sql.find("values", inicioNombre);
    if (finNombre == std::string::npos) finNombre = sql.find("(", inicioNombre);
    if (finNombre == std::string::npos) return "";
    std::string nombre = sql.substr(inicioNombre, finNombre - inicioNombre);
    return limpiarNombre(nombre);
}

// Extrae los valores que van entre parentesis despues de VALUES.
std::vector<std::string> QueryProcessor::extraerValores(const std::string& sql) {
    std::vector<std::string> valores;

    size_t valuesPos = sql.find("VALUES");
    if (valuesPos == std::string::npos) {
        valuesPos = sql.find("values");
        if (valuesPos == std::string::npos) return valores;
    }

    size_t inicio = sql.find("(", valuesPos);
    if (inicio == std::string::npos) return valores;

    size_t fin = sql.rfind(")");
    if (fin == std::string::npos || fin <= inicio) return valores;

    std::string bloque = sql.substr(inicio + 1, fin - inicio - 1);

    std::string actual;
    bool dentroComillas = false;
    for (char c : bloque) {
        if (c == '"' || c == '\'') {
            dentroComillas = !dentroComillas;
            actual += c;
        }
        else if (c == ',' && !dentroComillas) {
            std::string val = limpiarNombre(actual);
            if (!val.empty() && (val.front() == '"' || val.front() == '\'')) val.erase(0, 1);
            if (!val.empty() && (val.back() == '"' || val.back() == '\'')) val.pop_back();
            valores.push_back(val);
            actual = "";
        }
        else {
            actual += c;
        }
    }
    if (!actual.empty()) {
        std::string val = limpiarNombre(actual);
        if (!val.empty() && (val.front() == '"' || val.front() == '\'')) val.erase(0, 1);
        if (!val.empty() && (val.back() == '"' || val.back() == '\'')) val.pop_back();
        valores.push_back(val);
    }
    return valores;
}