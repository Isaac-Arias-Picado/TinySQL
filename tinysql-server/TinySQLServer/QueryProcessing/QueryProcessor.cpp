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
    else if (cmd.rfind("CREATE TABLE", 0) == 0) {
        std::string nombreTabla = extraerNombreTabla(sql);
        std::string bloque = extraerBloqueColumnas(sql);
        std::vector<std::string> textosColumnas = partirPorComas(bloque);

        std::vector<Columna> columnas;
        for (const auto& texto : textosColumnas) {
            columnas.push_back(parsearColumna(texto));
        }

        r = storage.crearTabla(dbContext, nombreTabla, columnas);
    }
    else if (cmd.rfind("INSERT", 0) == 0) {
        std::string tabla = extraerNombreTablaInsert(sql);
        std::vector<std::string> valores = extraerValores(sql);
        r = storage.insertarFila(dbContext, tabla, valores);
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

std::string QueryProcessor::extraerNombreTabla(const std::string& sql) {
    size_t inicioTabla = sql.find("TABLE"); //Se busca la palabra TABLE en la sentencia
    if (inicioTabla == std::string::npos) return ""; //Si no se encuentra, se devuelve un no encontrado
    size_t inicioNombre = inicioTabla + 5;  
    size_t finNombre = sql.find("(", inicioNombre);
    size_t posAs = sql.find(" AS ", inicioNombre);
    if (posAs != std::string::npos && posAs < finNombre) {
        finNombre = posAs;
    }
    if (finNombre == std::string::npos) return "";

    // Extrae y limpia espacios
    std::string nombre = sql.substr(inicioNombre, finNombre - inicioNombre);
    return limpiarNombre(nombre);
}

std::string QueryProcessor::extraerBloqueColumnas(const std::string& sql) {
    size_t inicio = sql.find("("); //Se busca especificamente el primer parentesis de la sentencia
    size_t fin = sql.rfind(")"); //Se busca el ultimo parentesis de cierre de la sentencia
    if (inicio == std::string::npos || fin == std::string::npos || fin <= inicio) {
        return "";
    }
    //Se extrae lo que este entre los parentesis
    return sql.substr(inicio + 1, fin - inicio - 1);
}

std::vector<std::string> QueryProcessor::partirPorComas(const std::string& bloque) {
    std::vector<std::string> partes;
    std::string actual;
    for (char c : bloque) {
        if (c == ',') {
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

// Se identifica el tipo de dato al parsear
TipoColumna QueryProcessor::parsearTipo(const std::string& tipoTexto) {
    if (tipoTexto.rfind("INTEGER", 0) == 0)  return TipoColumna::INTEGER;
    if (tipoTexto.rfind("DOUBLE", 0) == 0)   return TipoColumna::DOUBLE;
    if (tipoTexto.rfind("VARCHAR", 0) == 0)  return TipoColumna::VARCHAR;
    if (tipoTexto.rfind("DATETIME", 0) == 0) return TipoColumna::DATETIME;
}


Columna QueryProcessor::parsearColumna(const std::string& texto) {
    Columna col;
    col.nullable = true;  

    //Se busca del inicio de la palabra al primer espacio encontrado
    size_t espacio = texto.find(" ");
    col.name = texto.substr(0, espacio);

    //Se extrea el string que corresponde al tipo de texto
    std::string tipoTexto = limpiarNombre(texto.substr(espacio + 1));
    col.type = parsearTipo(tipoTexto);

    if (col.type == TipoColumna::VARCHAR) {
        // Se saca el numero que este entre los parentesis de VARCHAR(numero)
        size_t abre = tipoTexto.find("(");
        size_t cierra = tipoTexto.find(")");
        std::string numero = tipoTexto.substr(abre + 1, cierra - abre - 1);
        col.size = std::stoi(numero); //Se pasa el string a un numero para obtener el tamanho de VARCHAR
    } //El resto de tipos tienen un tamanho fijo
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

// Saca el nombre entre "INTO" y "VALUES"
std::string QueryProcessor::extraerNombreTablaInsert(const std::string& sql) {
    size_t inicioInto = sql.find("INTO");
    if (inicioInto == std::string::npos) return "";
    size_t inicioNombre = inicioInto + 4;
    size_t finNombre = sql.find("VALUES", inicioNombre);
    if (finNombre == std::string::npos) finNombre = sql.find("(", inicioNombre);
    if (finNombre == std::string::npos) return "";
    return limpiarNombre(sql.substr(inicioNombre, finNombre - inicioNombre));
}

// Saca los valores entre parentesis y los separa por comas
std::vector<std::string> QueryProcessor::extraerValores(const std::string& sql) {
    std::vector<std::string> valores;
    size_t inicio = sql.find("(");
    size_t fin = sql.rfind(")");
    if (inicio == std::string::npos || fin == std::string::npos) return valores;

    std::string bloque = sql.substr(inicio + 1, fin - inicio - 1);
    std::vector<std::string> crudos = partirPorComas(bloque);

    for (auto& v : crudos) {
        std::string limpio = limpiarNombre(v);
        // quitar comillas dobles al inicio y final
        if (!limpio.empty() && limpio.front() == '"') limpio.erase(0, 1);
        if (!limpio.empty() && limpio.back() == '"') limpio.pop_back();
        valores.push_back(limpio);
    }
    return valores;
}