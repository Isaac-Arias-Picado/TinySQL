#include "QueryProcessor.h"
#include <cctype>

//CREATE TABLE

// El nombre de la tabla esta entre la palabra TABLE y el "(" (o el " AS ").
std::string QueryProcessor::extraerNombreTabla(const std::string& sql) {
    size_t inicioTabla = sql.find("TABLE");
    if (inicioTabla == std::string::npos) return "";
    size_t inicioNombre = inicioTabla + 5;
    size_t finNombre = sql.find("(", inicioNombre);
    size_t posAs = sql.find(" AS ", inicioNombre);
    if (posAs != std::string::npos && posAs < finNombre) {
        finNombre = posAs;
    }
    if (finNombre == std::string::npos) return "";
    std::string nombre = sql.substr(inicioNombre, finNombre - inicioNombre);
    return limpiarNombre(nombre);
}

// Devuelve lo que esta entre el primer "(" y el ultimo ")": la lista de columnas.
std::string QueryProcessor::extraerBloqueColumnas(const std::string& sql) {
    size_t inicio = sql.find("(");
    size_t fin = sql.rfind(")");
    if (inicio == std::string::npos || fin == std::string::npos || fin <= inicio) {
        return "";
    }
    return sql.substr(inicio + 1, fin - inicio - 1);
}

// Separa la lista de columnas por comas. Ignora las comas que esten dentro de
std::vector<std::string> QueryProcessor::partirPorComas(const std::string& bloque) {
    std::vector<std::string> partes;
    std::string actual;
    bool dentroParentesis = false;
    for (char c : bloque) {
        if (c == '(') dentroParentesis = true;
        else if (c == ')') dentroParentesis = false;
        if (c == ',' && !dentroParentesis) {
            partes.push_back(limpiarNombre(actual));
            actual = "";
        }
        else {
            actual += c;
        }
    }
    if (!actual.empty()) {
        partes.push_back(limpiarNombre(actual));
    }
    return partes;
}

// Convierte el texto del tipo a nuestro enum. Comparamos con rfind(...,0) porque
// el VARCHAR viene pegado a su tamano ("VARCHAR(30)").
TipoColumna QueryProcessor::parsearTipo(const std::string& tipoTexto) {
    if (tipoTexto.rfind("INTEGER", 0) == 0)  return TipoColumna::INTEGER;
    if (tipoTexto.rfind("DOUBLE", 0) == 0)   return TipoColumna::DOUBLE;
    if (tipoTexto.rfind("VARCHAR", 0) == 0)  return TipoColumna::VARCHAR;
    if (tipoTexto.rfind("DATETIME", 0) == 0) return TipoColumna::DATETIME;
    return TipoColumna::VARCHAR;
}

// Convierte un fragmento como "Nombre VARCHAR(30)" en una Columna con su nombre, tipo y tamano en bytes.
Columna QueryProcessor::parsearColumna(const std::string& texto) {
    Columna col;
    col.nullable = true;

    size_t espacio = texto.find(" ");
    if (espacio == std::string::npos) {
        col.name = texto;
        col.type = TipoColumna::VARCHAR;
        col.size = 20;
        return col;
    }
    col.name = texto.substr(0, espacio);

    std::string tipoTexto = limpiarNombre(texto.substr(espacio + 1));
    col.type = parsearTipo(tipoTexto);

    if (col.type == TipoColumna::VARCHAR) {
        size_t abre = tipoTexto.find("(");
        size_t cierra = tipoTexto.find(")");
        if (abre != std::string::npos && cierra != std::string::npos) {
            std::string numero = tipoTexto.substr(abre + 1, cierra - abre - 1);
            col.size = std::stoi(numero);
        }
        else {
            col.size = 20;
        }
    }
    else if (col.type == TipoColumna::INTEGER) {
        col.size = 4;
    }
    else if (col.type == TipoColumna::DOUBLE) {
        col.size = 8;
    }
    else if (col.type == TipoColumna::DATETIME) {
        col.size = 19;
    }
    return col;
}

//DROP TABLE

// Toma la palabra que sigue a TABLE como nombre.
std::string QueryProcessor::extraerNombreTablaDrop(const std::string& sql) {
    size_t inicio = sql.find("TABLE");
    if (inicio == std::string::npos) return "";
    size_t inicioNombre = inicio + 5;
    while (inicioNombre < sql.size() && (sql[inicioNombre] == ' ' || sql[inicioNombre] == '\t')) inicioNombre++;
    std::string resto = sql.substr(inicioNombre);
    size_t fin = resto.find_first_of(" \t;");
    if (fin == std::string::npos) fin = resto.size();
    std::string nombre = resto.substr(0, fin);
    return limpiarNombre(nombre);
}

//CREATE INDEX

// Nombre del indice: entre la palabra INDEX y " ON ".
std::string QueryProcessor::extraerNombreIndice(const std::string& sql) {
    size_t inicio = sql.find("INDEX");
    if (inicio == std::string::npos) return "";
    size_t inicioNombre = inicio + 5;
    while (inicioNombre < sql.size() && (sql[inicioNombre] == ' ' || sql[inicioNombre] == '\t')) inicioNombre++;
    size_t finNombre = sql.find(" ON ", inicioNombre);
    if (finNombre == std::string::npos) return "";
    std::string nombre = sql.substr(inicioNombre, finNombre - inicioNombre);
    return limpiarNombre(nombre);
}

// Tabla del indice: lo que va despues de ON y antes del "(".
std::string QueryProcessor::extraerNombreTablaIndex(const std::string& sql) {
    size_t onPos = sql.find("ON");
    if (onPos == std::string::npos) return "";
    size_t inicioNombre = onPos + 2;
    while (inicioNombre < sql.size() && (sql[inicioNombre] == ' ' || sql[inicioNombre] == '\t')) inicioNombre++;
    size_t finNombre = sql.find("(", inicioNombre);
    if (finNombre == std::string::npos) return "";
    std::string nombre = sql.substr(inicioNombre, finNombre - inicioNombre);
    return limpiarNombre(nombre);
}

// Columna del indice: lo que esta entre parentesis.
std::string QueryProcessor::extraerColumnaIndex(const std::string& sql) {
    size_t inicio = sql.find("(");
    if (inicio == std::string::npos) return "";
    size_t fin = sql.find(")", inicio);
    if (fin == std::string::npos) return "";
    std::string columna = sql.substr(inicio + 1, fin - inicio - 1);
    return limpiarNombre(columna);
}

// Tipo de arbol (BST o BTREE): lo que va despues de "OF TYPE".
std::string QueryProcessor::extraerTipoArbol(const std::string& sql) {
    std::string upper = sql;
    for (auto& c : upper) c = toupper((unsigned char)c);
    size_t ofPos = upper.find("OF TYPE");
    if (ofPos == std::string::npos) return "";
    size_t inicioTipo = ofPos + 7;
    while (inicioTipo < sql.size() && (sql[inicioTipo] == ' ' || sql[inicioTipo] == '\t')) inicioTipo++;
    size_t finTipo = sql.find_first_of(" \t;", inicioTipo);
    if (finTipo == std::string::npos) finTipo = sql.size();
    std::string tipo = sql.substr(inicioTipo, finTipo - inicioTipo);
    return limpiarNombre(tipo);
}