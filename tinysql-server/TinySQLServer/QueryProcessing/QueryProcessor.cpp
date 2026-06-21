#include "QueryProcessor.h"
#include <cctype>
#include <iostream>

// Limpia espacios, tabuladores y punto y coma final
std::string QueryProcessor::limpiarNombre(const std::string& raw) {
    std::string nombre = raw;
    size_t start = nombre.find_first_not_of(" \t");
    if (start != std::string::npos) nombre = nombre.substr(start);
    else nombre.clear();

    size_t end = nombre.find_last_not_of(" \t");
    if (end != std::string::npos) nombre = nombre.substr(0, end + 1);
    else nombre.clear();

    if (!nombre.empty() && nombre.back() == ';') nombre.pop_back();
    return nombre;
}

// Punto de entrada principal
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
    else if (cmd.rfind("DROP TABLE", 0) == 0) {
        std::string nombreTabla = extraerNombreTablaDrop(sql);
        if (nombreTabla.empty()) {
            r.error = "Invalid table name";
            return r;
        }
        r = storage.eliminarTabla(dbContext, nombreTabla);
    }
    else if (cmd.rfind("INSERT", 0) == 0) {
        std::string tabla = extraerNombreTablaInsert(sql);
        std::vector<std::string> valores = extraerValores(sql);

        std::cout << "DEBUG execute: tabla='" << tabla << "', valores.size=" << valores.size() << std::endl;
        for (size_t i = 0; i < valores.size(); ++i) {
            std::cout << "  valor[" << i << "] = '" << valores[i] << "'" << std::endl;
        }

        if (tabla.empty()) {
            r.error = "Invalid INSERT syntax: table name missing";
            return r;
        }
        if (valores.empty()) {
            r.error = "Invalid INSERT syntax: values missing";
            return r;
        }
        r = storage.insertarFila(dbContext, tabla, valores);
    }
    else if (cmd.rfind("SET DATABASE", 0) == 0) {
        std::string nombre = limpiarNombre(sql.substr(12));
        if (storage.existeBaseDatos(nombre)) {
            r.success = true;
            r.type = "ddl";
            r.message = "Context set to " + nombre;
        }
        else {
            r.error = "Database does not exist";
        }
    }
    else if (cmd.rfind("DELETE", 0) == 0) {
        size_t fromPos = cmd.find("FROM");
        if (fromPos == std::string::npos) {
            r.error = "DELETE syntax error: missing FROM";
            return r;
        }

        size_t startTable = fromPos + 4;
        while (startTable < sql.size() && (sql[startTable] == ' ' || sql[startTable] == '\t')) startTable++;
        size_t endTable = sql.find_first_of(" \t", startTable);
        if (endTable == std::string::npos) endTable = sql.size();
        std::string tableName = limpiarNombre(sql.substr(startTable, endTable - startTable));

        std::string whereColumn, whereOperator, whereValue;

        size_t wherePos = cmd.find("WHERE");
        if (wherePos != std::string::npos) {
            std::string condicion = sql.substr(wherePos + 5);
            condicion = limpiarNombre(condicion);
            size_t firstSpace = condicion.find(' ');
            if (firstSpace == std::string::npos) {
                r.error = "Invalid WHERE clause";
                return r;
            }
            whereColumn = limpiarNombre(condicion.substr(0, firstSpace));
            std::string resto = condicion.substr(firstSpace + 1);

            std::vector<std::string> ops = { "LIKE", "NOT", "=", ">", "<" };
            int foundOp = -1;
            for (size_t i = 0; i < ops.size(); ++i) {
                size_t posOp = resto.find(ops[i]);
                if (posOp != std::string::npos && (posOp == 0 || resto[posOp - 1] == ' ')) {
                    foundOp = (int)i;
                    break;
                }
            }
            if (foundOp == -1) {
                r.error = "Unsupported operator in WHERE";
                return r;
            }
            whereOperator = ops[foundOp];
            size_t afterOp = resto.find(whereOperator) + whereOperator.size();
            whereValue = limpiarNombre(resto.substr(afterOp));
            if (!whereValue.empty() && whereValue.front() == '"') whereValue.erase(0, 1);
            if (!whereValue.empty() && whereValue.back() == '"') whereValue.pop_back();
        }

        r = storage.eliminarFilas(dbContext, tableName, whereColumn, whereOperator, whereValue);
    }
    else if (cmd.rfind("SELECT", 0) == 0) {
        // Parsear SELECT
        std::string resto = sql.substr(6); 
        resto = limpiarNombre(resto);

        // Extraer columnas 
        size_t fromPos = resto.find("FROM");
        if (fromPos == std::string::npos) {
            r.error = "SELECT syntax error: missing FROM";
            return r;
        }
        std::string columnasStr = limpiarNombre(resto.substr(0, fromPos));
        std::string restoAfterFrom = limpiarNombre(resto.substr(fromPos + 4));

        // Extraer nombre de tabla 
        std::string tableName;
        std::string whereColumn, whereOperator, whereValue;
        std::string orderColumn, orderDirection;

        // Buscar WHERE
        size_t wherePos = restoAfterFrom.find("WHERE");
        size_t orderPos = restoAfterFrom.find("ORDER BY");

        size_t endTable = restoAfterFrom.size();
        if (wherePos != std::string::npos) endTable = wherePos;
        if (orderPos != std::string::npos && orderPos < endTable) endTable = orderPos;

        tableName = limpiarNombre(restoAfterFrom.substr(0, endTable));

        // Procesar WHERE si existe
        if (wherePos != std::string::npos) {
            std::string whereClause = restoAfterFrom.substr(wherePos + 5);
            size_t firstSpace = whereClause.find(' ');
            if (firstSpace != std::string::npos) {
                whereColumn = limpiarNombre(whereClause.substr(0, firstSpace));
                std::string restoWhere = limpiarNombre(whereClause.substr(firstSpace + 1));
                // Buscar operador
                std::vector<std::string> ops = { "LIKE", "NOT", "=", ">", "<" };
                int foundOp = -1;
                for (size_t i = 0; i < ops.size(); ++i) {
                    size_t posOp = restoWhere.find(ops[i]);
                    if (posOp != std::string::npos && (posOp == 0 || restoWhere[posOp - 1] == ' ')) {
                        foundOp = (int)i;
                        break;
                    }
                }
                if (foundOp == -1) {
                    r.error = "Unsupported operator in WHERE";
                    return r;
                }
                whereOperator = ops[foundOp];
                size_t afterOp = restoWhere.find(whereOperator) + whereOperator.size();
                whereValue = limpiarNombre(restoWhere.substr(afterOp));
                // Quitar comillas
                if (!whereValue.empty() && whereValue.front() == '"') whereValue.erase(0, 1);
                if (!whereValue.empty() && whereValue.back() == '"') whereValue.pop_back();
            }
            else {
                r.error = "Invalid WHERE clause";
                return r;
            }
        }

        // Procesar ORDER BY si existe
        if (orderPos != std::string::npos) {
            std::string orderClause = restoAfterFrom.substr(orderPos + 8);
            orderClause = limpiarNombre(orderClause);
            // Dividir en columna y direccion
            size_t space = orderClause.find(' ');
            if (space != std::string::npos) {
                orderColumn = limpiarNombre(orderClause.substr(0, space));
                orderDirection = limpiarNombre(orderClause.substr(space + 1));
            }
            else {
                orderColumn = limpiarNombre(orderClause);
                orderDirection = "ASC"; // por defecto
            }
            // Normalizar direccion
            if (orderDirection != "ASC" && orderDirection != "DESC") {
                orderDirection = "ASC";
            }
        }

        std::vector<std::string> columnas;
        if (columnasStr == "*") {
            columnas.push_back("*");
        }
        else {
            // Dividir por comas
            std::string actual;
            for (char c : columnasStr) {
                if (c == ',') {
                    columnas.push_back(limpiarNombre(actual));
                    actual = "";
                }
                else {
                    actual += c;
                }
            }
            if (!actual.empty()) {
                columnas.push_back(limpiarNombre(actual));
            }
        }

        // Llamar a StoredDataManager
        r = storage.seleccionarFilas(dbContext, tableName, columnas,
            whereColumn, whereOperator, whereValue,
            orderColumn, orderDirection);
    }
    else if (cmd.rfind("UPDATE", 0) == 0) {
        size_t setPos = cmd.find("SET");
        if (setPos == std::string::npos) {
            r.error = "UPDATE syntax error: missing SET";
            return r;
        }
        std::string tableName = limpiarNombre(sql.substr(6, setPos - 6));

        size_t wherePos = cmd.find("WHERE");
        std::string asignacion;
        if (wherePos != std::string::npos) {
            asignacion = sql.substr(setPos + 3, wherePos - (setPos + 3));
        }
        else {
            asignacion = sql.substr(setPos + 3);
        }
        asignacion = limpiarNombre(asignacion);

        size_t igualPos = asignacion.find('=');
        if (igualPos == std::string::npos) {
            r.error = "UPDATE syntax error: missing = in SET";
            return r;
        }
        std::string setColumn = limpiarNombre(asignacion.substr(0, igualPos));
        std::string setValue = limpiarNombre(asignacion.substr(igualPos + 1));
        if (!setValue.empty() && setValue.front() == '"') setValue.erase(0, 1);
        if (!setValue.empty() && setValue.back() == '"') setValue.pop_back();

        std::string whereColumn, whereOperator, whereValue;
        if (wherePos != std::string::npos) {
            std::string condicion = limpiarNombre(sql.substr(wherePos + 5));
            size_t firstSpace = condicion.find(' ');
            if (firstSpace == std::string::npos) {
                r.error = "Invalid WHERE clause";
                return r;
            }
            whereColumn = limpiarNombre(condicion.substr(0, firstSpace));
            std::string resto = condicion.substr(firstSpace + 1);

            std::vector<std::string> ops = { "LIKE", "NOT", "=", ">", "<" };
            int foundOp = -1;
            for (size_t i = 0; i < ops.size(); ++i) {
                size_t posOp = resto.find(ops[i]);
                if (posOp != std::string::npos && (posOp == 0 || resto[posOp - 1] == ' ')) {
                    foundOp = (int)i;
                    break;
                }
            }
            if (foundOp == -1) {
                r.error = "Unsupported operator in WHERE";
                return r;
            }
            whereOperator = ops[foundOp];
            size_t afterOp = resto.find(whereOperator) + whereOperator.size();
            whereValue = limpiarNombre(resto.substr(afterOp));
            if (!whereValue.empty() && whereValue.front() == '"') whereValue.erase(0, 1);
            if (!whereValue.empty() && whereValue.back() == '"') whereValue.pop_back();
        }

        r = storage.actualizarFilas(dbContext, tableName, setColumn, setValue,
            whereColumn, whereOperator, whereValue);
    }
    else {
        r.error = "Comando no implementado";
    }
    return r;
}


// Para CREATE TABLE: extrae el nombre de la tabla
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

// Para CREATE TABLE: extrae el bloque entre parentesis
std::string QueryProcessor::extraerBloqueColumnas(const std::string& sql) {
    size_t inicio = sql.find("(");
    size_t fin = sql.rfind(")");
    if (inicio == std::string::npos || fin == std::string::npos || fin <= inicio) {
        return "";
    }
    return sql.substr(inicio + 1, fin - inicio - 1);
}

// Para CREATE TABLE: divide el bloque de columnas por comas
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


// Para CREATE TABLE: parsea el tipo de dato
TipoColumna QueryProcessor::parsearTipo(const std::string& tipoTexto) {
    if (tipoTexto.rfind("INTEGER", 0) == 0)  return TipoColumna::INTEGER;
    if (tipoTexto.rfind("DOUBLE", 0) == 0)   return TipoColumna::DOUBLE;
    if (tipoTexto.rfind("VARCHAR", 0) == 0)  return TipoColumna::VARCHAR;
    if (tipoTexto.rfind("DATETIME", 0) == 0) return TipoColumna::DATETIME;
    return TipoColumna::VARCHAR;
}

// Para CREATE TABLE: parsea una definicion de columna completa
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


// Para INSERT: extrae el nombre de la tabla entre "INTO" y "VALUES"
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

// Para INSERT: extrae los valores entre parentesis
std::vector<std::string> QueryProcessor::extraerValores(const std::string& sql) {
    std::vector<std::string> valores;

    // Buscar "VALUES" en la cadena original (sin convertir a mayusculas)
    size_t valuesPos = sql.find("VALUES");
    if (valuesPos == std::string::npos) {
        valuesPos = sql.find("values");
        if (valuesPos == std::string::npos) {
            std::cout << "DEBUG extraerValores: no se encontró 'VALUES'" << std::endl;
            return valores;
        }
    }


    size_t inicio = sql.find("(", valuesPos);
    if (inicio == std::string::npos) {
        std::cout << "DEBUG extraerValores: no se encontró '(' después de VALUES" << std::endl;
        return valores;
    }

    size_t fin = sql.rfind(")");
    if (fin == std::string::npos || fin <= inicio) {
        std::cout << "DEBUG extraerValores: no se encontró ')' o está antes de '('" << std::endl;
        return valores;
    }

    std::string bloque = sql.substr(inicio + 1, fin - inicio - 1);
    std::cout << "DEBUG extraerValores: bloque extraído = '" << bloque << "'" << std::endl;

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

    for (size_t i = 0; i < valores.size(); ++i) {
        std::cout << "  valor[" << i << "] = '" << valores[i] << "'" << std::endl;
    }

    return valores;
}

// Para DROP TABLE: extrae el nombre de la tabla despues de "TABLE"
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