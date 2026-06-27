#include "QueryProcessor.h"
#include <cctype>

// Recorta espacios y tabuladores de los extremos y elimina el ';' final si existe.
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

// Punto de entrada de la capa. Detecta cual es el comando segun como empieza
// la sentencia y arma los parametros para llamar a StoredDataManager.
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
    else if (cmd.rfind("CREATE INDEX", 0) == 0) {
        std::string indexName = extraerNombreIndice(sql);
        std::string tableName = extraerNombreTablaIndex(sql);
        std::string columnName = extraerColumnaIndex(sql);
        std::string treeType = extraerTipoArbol(sql);

        if (indexName.empty() || tableName.empty() || columnName.empty()) {
            r.error = "Invalid CREATE INDEX syntax";
            return r;
        }
        if (treeType != "BST" && treeType != "BTREE") {
            r.error = "Invalid index type. Use BST or BTREE";
            return r;
        }
        r = storage.crearIndice(dbContext, tableName, indexName, columnName, treeType);
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

        // El nombre de la tabla es la palabra que sigue a FROM
        size_t startTable = fromPos + 4;
        while (startTable < sql.size() && (sql[startTable] == ' ' || sql[startTable] == '\t')) startTable++;
        size_t endTable = sql.find_first_of(" \t", startTable);
        if (endTable == std::string::npos) endTable = sql.size();
        std::string tableName = limpiarNombre(sql.substr(startTable, endTable - startTable));

        // El WHERE es opcional; si no esta, se borran todas las filas
        std::string whereColumn, whereOperator, whereValue;
        size_t wherePos = cmd.find("WHERE");
        if (wherePos != std::string::npos) {
            std::string condicion = limpiarNombre(sql.substr(wherePos + 5));
            size_t firstSpace = condicion.find(' ');
            if (firstSpace == std::string::npos) {
                r.error = "Invalid WHERE clause";
                return r;
            }
            whereColumn = limpiarNombre(condicion.substr(0, firstSpace));
            std::string resto = condicion.substr(firstSpace + 1);

            // Buscamos cual de los operadores soportados aparece en la condicion
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
        std::string resto = limpiarNombre(sql.substr(6));

        // Lo que va entre SELECT y FROM son las columnas pedidas
        size_t fromPos = resto.find("FROM");
        if (fromPos == std::string::npos) {
            r.error = "SELECT syntax error: missing FROM";
            return r;
        }
        std::string columnasStr = limpiarNombre(resto.substr(0, fromPos));
        std::string restoAfterFrom = limpiarNombre(resto.substr(fromPos + 4));

        std::string tableName;
        std::string whereColumn, whereOperator, whereValue;
        std::string orderColumn, orderDirection;

        // El nombre de la tabla termina donde empieza WHERE u ORDER BY (lo que venga primero)
        size_t wherePos = restoAfterFrom.find("WHERE");
        size_t orderPos = restoAfterFrom.find("ORDER BY");
        size_t endTable = restoAfterFrom.size();
        if (wherePos != std::string::npos) endTable = wherePos;
        if (orderPos != std::string::npos && orderPos < endTable) endTable = orderPos;
        tableName = limpiarNombre(restoAfterFrom.substr(0, endTable));

        // WHERE opcional
        if (wherePos != std::string::npos) {
            std::string whereClause = limpiarNombre(restoAfterFrom.substr(wherePos + 5));
            size_t firstSpace = whereClause.find(' ');
            if (firstSpace != std::string::npos) {
                whereColumn = limpiarNombre(whereClause.substr(0, firstSpace));
                std::string restoWhere = limpiarNombre(whereClause.substr(firstSpace + 1));
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
                if (!whereValue.empty() && whereValue.front() == '"') whereValue.erase(0, 1);
                if (!whereValue.empty() && whereValue.back() == '"') whereValue.pop_back();
            }
            else {
                r.error = "Invalid WHERE clause";
                return r;
            }
        }

        // ORDER BY opcional: columna y direccion (ASC por defecto)
        if (orderPos != std::string::npos) {
            std::string orderClause = limpiarNombre(restoAfterFrom.substr(orderPos + 8));
            size_t space = orderClause.find(' ');
            if (space != std::string::npos) {
                orderColumn = limpiarNombre(orderClause.substr(0, space));
                orderDirection = limpiarNombre(orderClause.substr(space + 1));
            }
            else {
                orderColumn = limpiarNombre(orderClause);
                orderDirection = "ASC";
            }
            if (orderDirection != "ASC" && orderDirection != "DESC") {
                orderDirection = "ASC";
            }
        }

        // Las columnas pedidas pueden ser "*" o una lista separada por comas
        std::vector<std::string> columnas;
        if (columnasStr == "*") {
            columnas.push_back("*");
        }
        else {
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

        r = storage.seleccionarFilas(dbContext, tableName, columnas,
            whereColumn, whereOperator, whereValue,
            orderColumn, orderDirection);
    }
    else if (cmd.rfind("UPDATE", 0) == 0) {
        // Formato: UPDATE tabla SET columna = valor [WHERE ...]
        size_t setPos = cmd.find("SET");
        if (setPos == std::string::npos) {
            r.error = "UPDATE syntax error: missing SET";
            return r;
        }
        std::string tableName = limpiarNombre(sql.substr(6, setPos - 6));

        // La asignacion del SET va entre "SET" y "WHERE" (o hasta el final)
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

        // Mismo parseo de WHERE que en las otras sentencias
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