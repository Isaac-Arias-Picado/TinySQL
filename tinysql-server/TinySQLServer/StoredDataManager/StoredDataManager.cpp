#include "StoredDataManager.h"
#include "SystemCatalog.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <regex>
#include <functional>

namespace fs = std::filesystem;
static const std::string DATA_DIR = "./data";

// Constructor
StoredDataManager::StoredDataManager() {
    cargarBasesDeDatos();
}

// Carga las bases de datos desde el sistema de archivos
void StoredDataManager::cargarBasesDeDatos() {
    if (!fs::exists(DATA_DIR)) fs::create_directory(DATA_DIR);
    for (const auto& entry : fs::directory_iterator(DATA_DIR)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if (name != "SystemCatalog") {
                databases.insert(name);
            }
        }
    }
    std::cout << "Bases de datos cargadas: ";
    for (const auto& db : databases) std::cout << db << " ";
    std::cout << std::endl;
}

// Verifica si una base de datos existe
bool StoredDataManager::existeBaseDatos(const std::string& nombre) const {
    return databases.count(nombre) > 0;
}

// CREATE DATABASE
QueryResult StoredDataManager::crearBaseDatos(const std::string& nombre) {
    QueryResult r;
    if (databases.count(nombre)) {
        r.error = "Database already exists";
        return r;
    }
    fs::create_directory(DATA_DIR + "/" + nombre);
    databases.insert(nombre);
    escribirBaseDatos(nombre);
    r.success = true;
    r.type = "ddl";
    r.message = "Database created";
    return r;
}

// DROP DATABASE
QueryResult StoredDataManager::eliminarBaseDatos(const std::string& nombre) {
    QueryResult r;
    if (!databases.count(nombre)) {
        r.error = "Database does not exist";
        return r;
    }
    fs::remove_all(DATA_DIR + "/" + nombre);
    databases.erase(nombre);
    r.success = true;
    r.type = "ddl";
    r.message = "Database dropped";
    return r;
}

// CREATE TABLE
QueryResult StoredDataManager::crearTabla(const std::string& dbName,
    const std::string& tableName,
    const std::vector<Columna>& columnas) {
    QueryResult r;
    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ofstream file(rutaTabla, std::ios::binary | std::ios::app);
    file.close();

    escribirTabla(dbName, tableName);
    for (int i = 0; i < (int)columnas.size(); ++i) {
        escribirColumna(dbName, tableName, columnas[i], i);
    }

    r.success = true;
    r.type = "ddl";
    r.message = "Table created";
    return r;
}

// DROP TABLE
QueryResult StoredDataManager::eliminarTabla(const std::string& dbName,
    const std::string& tableName) {
    QueryResult r;

    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) {
        r.error = "Table does not exist";
        return r;
    }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream file(rutaTabla, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        r.error = "Cannot open table file";
        return r;
    }
    size_t size = file.tellg();
    file.close();

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    if (size > 0 && size % registroSize == 0) {
        r.error = "Table is not empty. DROP TABLE only allowed on empty tables.";
        return r;
    }

    if (!fs::remove(rutaTabla)) {
        r.error = "Failed to delete table file";
        return r;
    }

    eliminarTablaDelCatalogo(dbName, tableName);
    eliminarColumnasDeTabla(dbName, tableName);

    r.success = true;
    r.type = "ddl";
    r.message = "Table dropped successfully";
    return r;
}

// INSERT INTO
QueryResult StoredDataManager::insertarFila(const std::string& dbName,const std::string& tableName,const std::vector<std::string>& valores) {
    QueryResult r;
    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) {
        r.error = "Table does not exist";
        return r;
    }

    if (valores.size() != columnas.size()) {
        r.error = "Column count does not match value count";
        return r;
    }
    std::cout << "DEBUG insertarFila: columnas.size=" << columnas.size() << ", valores.size=" << valores.size() << std::endl;
    // Validar tipos
    for (size_t i = 0; i < columnas.size(); ++i) {
        const Columna& col = columnas[i];
        const std::string& valor = valores[i];

        if (col.type == TipoColumna::DATETIME && !esFechaValida(valor)) {
            r.error = "Invalid datetime format: " + valor;
            return r;
        }
    }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ofstream file(rutaTabla, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        r.error = "Cannot open table file";
        return r;
    }

    for (size_t i = 0; i < columnas.size(); ++i) {
        const Columna& col = columnas[i];
        const std::string& valor = valores[i];

        if (col.type == TipoColumna::INTEGER) {
            int num = std::stoi(valor);
            file.write(reinterpret_cast<const char*>(&num), sizeof(int));
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double num = std::stod(valor);
            file.write(reinterpret_cast<const char*>(&num), sizeof(double));
        }
        else if (col.type == TipoColumna::VARCHAR) {
            std::vector<char> buffer(col.size, 0);
            for (int j = 0; j < (int)valor.size() && j < col.size; ++j)
                buffer[j] = valor[j];
            file.write(buffer.data(), col.size);
        }
        else if (col.type == TipoColumna::DATETIME) {
            std::vector<char> buffer(19, 0);
            for (int j = 0; j < (int)valor.size() && j < 19; ++j)
                buffer[j] = valor[j];
            file.write(buffer.data(), 19);
        }
    }

    file.close();
    r.success = true;
    r.type = "dml";
    r.message = "Row inserted";
    return r;
}

// Validador de fecha (YYYY-MM-DD HH:MM:SS)
bool StoredDataManager::esFechaValida(const std::string& fecha) {
    if (fecha.size() != 19) return false;
    if (fecha[4] != '-' || fecha[7] != '-' || fecha[10] != ' ') return false;
    if (fecha[13] != ':' || fecha[16] != ':') return false;
    return true;
}

// lee todas las filas con su offset en el archivo
std::vector<std::pair<std::vector<std::string>, size_t>>
StoredDataManager::leerFilasConOffset(const std::string& dbName,
    const std::string& tableName) {
    std::vector<std::pair<std::vector<std::string>, size_t>> resultado;

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) return resultado;

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;
    if (registroSize == 0) return resultado;

    std::string ruta = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream file(ruta, std::ios::binary);
    if (!file.is_open()) return resultado;

    std::vector<char> buffer(registroSize);
    size_t offset = 0;

    while (file.read(buffer.data(), registroSize)) {
        std::vector<std::string> fila;
        size_t pos = 0;

        for (const auto& col : columnas) {
            std::string valor;

            if (col.type == TipoColumna::INTEGER) {
                int num;
                std::memcpy(&num, buffer.data() + pos, sizeof(int));
                valor = std::to_string(num);
                pos += sizeof(int);
            }
            else if (col.type == TipoColumna::DOUBLE) {
                double num;
                std::memcpy(&num, buffer.data() + pos, sizeof(double));
                valor = std::to_string(num);
                pos += sizeof(double);
            }
            else if (col.type == TipoColumna::VARCHAR) {
                valor = std::string(buffer.data() + pos, col.size);
                valor = std::string(valor.c_str()); // elimina nulos finales
                pos += col.size;
            }
            else if (col.type == TipoColumna::DATETIME) {
                valor = std::string(buffer.data() + pos, 19);
                pos += 19;
            }

            fila.push_back(valor);
        }

        resultado.push_back({ fila, offset });
        offset += registroSize;
    }

    file.close();
    return resultado;
}

// DELETE FROM ... [WHERE ...]
QueryResult StoredDataManager::eliminarFilas(const std::string& dbName,
    const std::string& tableName,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue) {
    QueryResult r;

    // Validar base de datos
    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    // Validar tabla
    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) {
        r.error = "Table does not exist";
        return r;
    }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream inFile(rutaTabla, std::ios::binary);
    if (!inFile.is_open()) {
        r.error = "Cannot open table file";
        return r;
    }

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    // Leer todos los registros
    std::vector<std::vector<char>> todosLosRegistros;
    std::vector<char> buffer(registroSize);
    while (inFile.read(buffer.data(), registroSize)) {
        todosLosRegistros.push_back(buffer);
        buffer.assign(registroSize, 0);
    }
    inFile.close();

    if (todosLosRegistros.empty()) {
        r.success = true;
        r.type = "dml";
        r.message = "0 rows deleted (table was empty)";
        r.affected_rows = 0;
        return r;
    }

    // Indice de la columna WHERE
    int colIndex = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].name == whereColumn) {
                colIndex = (int)i;
                break;
            }
        }
        if (colIndex == -1) {
            r.error = "Column '" + whereColumn + "' does not exist";
            return r;
        }
    }

    // Funcion para extraer un valor de un registro
    auto extraerValor = [&](const std::vector<char>& reg, int idx) -> std::string {
        const Columna& col = columnas[idx];
        size_t offset = 0;
        for (int i = 0; i < idx; ++i) offset += columnas[i].size;
        const char* data = reg.data() + offset;

        if (col.type == TipoColumna::INTEGER) {
            int val;
            std::memcpy(&val, data, sizeof(int));
            return std::to_string(val);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double val;
            std::memcpy(&val, data, sizeof(double));
            return std::to_string(val);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            std::string result;
            for (int j = 0; j < col.size; ++j) {
                if (data[j] == '\0') break;
                result.push_back(data[j]);
            }
            return result;
        }
        else if (col.type == TipoColumna::DATETIME) {
            return std::string(data, 19);
        }
        return "";
        };

    // Convertir a numero si es necesario
    auto convertirValor = [&](const std::string& val, const Columna& col) -> double {
        if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
            return std::stod(val);
        return 0.0;
        };

    // Funcion que evalua la condicion
    auto cumpleCondicion = [&](const std::vector<char>& reg) -> bool {
        if (whereColumn.empty()) return true; // sin WHERE -> elimina todo

        std::string valorColumna = extraerValor(reg, colIndex);
        const Columna& col = columnas[colIndex];

        if (whereOperator == "=") {
            return valorColumna == whereValue;
        }
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE) {
                double a = convertirValor(valorColumna, col);
                double b = convertirValor(whereValue, col);
                return a > b;
            }
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE) {
                double a = convertirValor(valorColumna, col);
                double b = convertirValor(whereValue, col);
                return a < b;
            }
            return valorColumna < whereValue;
        }
        else if (whereOperator == "LIKE") {
            std::string patron = whereValue;
            for (size_t i = 0; i < patron.size(); ++i) {
                if (patron[i] == '%') {
                    patron.replace(i, 1, ".*");
                    i += 1;
                }
            }
            std::regex regex(patron);
            return std::regex_match(valorColumna, regex);
        }
        else if (whereOperator == "NOT") {
            return valorColumna != whereValue;
        }
        return false;
        };

    // Filtrar registros
    std::vector<std::vector<char>> nuevosRegistros;
    size_t eliminados = 0;
    for (const auto& reg : todosLosRegistros) {
        if (cumpleCondicion(reg)) {
            eliminados++;
        }
        else {
            nuevosRegistros.push_back(reg);
        }
    }

    // Reescribir archivo
    std::ofstream outFile(rutaTabla, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        r.error = "Cannot write table file";
        return r;
    }
    for (const auto& reg : nuevosRegistros) {
        outFile.write(reg.data(), reg.size());
    }
    outFile.close();


    r.success = true;
    r.type = "dml";
    r.message = std::to_string(eliminados) + " rows deleted";
    r.affected_rows = (int)eliminados;
    return r;
}

// SELECT ... FROM ... [WHERE ...] [ORDER BY ...]
QueryResult StoredDataManager::seleccionarFilas(
    const std::string& dbName,
    const std::string& tableName,
    const std::vector<std::string>& columnas,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue,
    const std::string& orderColumn,
    const std::string& orderDirection) {

    QueryResult r;

    // Validar base de datos
    if (dbName.empty()) {
        r.error = "No database selected";
        return r;
    }
    if (!databases.count(dbName)) {
        r.error = "Database does not exist";
        return r;
    }

    // Leer columnas de la tabla
    std::vector<Columna> columnasTabla = leerColumnas(dbName, tableName);
    if (columnasTabla.empty()) {
        r.error = "Table does not exist";
        return r;
    }

    // Obtener todas las filas con offset 
    auto filasConOffset = leerFilasConOffset(dbName, tableName);
    if (filasConOffset.empty()) {
        r.success = true;
        r.type = "select";
        r.message = "0 rows selected";
        return r; // tabla vacía
    }

    // Determinar que columnas seleccionar
    std::vector<int> indicesSeleccionados;
    bool seleccionarTodas = columnas.empty() || (columnas.size() == 1 && columnas[0] == "*");
    if (seleccionarTodas) {
        // Seleccionar todas las columnas
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            indicesSeleccionados.push_back((int)i);
        }
    }
    else {
        // Seleccionar solo las columnas especificadas
        for (const auto& col : columnas) {
            bool encontrada = false;
            for (size_t i = 0; i < columnasTabla.size(); ++i) {
                if (columnasTabla[i].name == col) {
                    indicesSeleccionados.push_back((int)i);
                    encontrada = true;
                    break;
                }
            }
            if (!encontrada) {
                r.error = "Column '" + col + "' does not exist";
                return r;
            }
        }
    }

    // Si hay WHERE, buscar indice de la columna
    int colIndexWhere = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            if (columnasTabla[i].name == whereColumn) {
                colIndexWhere = (int)i;
                break;
            }
        }
        if (colIndexWhere == -1) {
            r.error = "Column '" + whereColumn + "' does not exist in table";
            return r;
        }
    }

    // Funcion para extraer valor de una fila en una columna dada
    auto extraerValor = [&](const std::vector<std::string>& fila, int idx) -> std::string {
        if (idx < 0 || idx >= (int)fila.size()) return "";
        return fila[idx];
        };

    // Convertir a número para comparaciones
    auto convertirValor = [&](const std::string& val, const Columna& col) -> double {
        if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
            return std::stod(val);
        return 0.0;
        };

    // Evaluar condición WHERE 
    auto cumpleCondicion = [&](const std::vector<std::string>& fila) -> bool {
        if (whereColumn.empty()) return true; // sin WHERE -> todas

        std::string valorColumna = extraerValor(fila, colIndexWhere);
        const Columna& col = columnasTabla[colIndexWhere];

        if (whereOperator == "=") {
            return valorColumna == whereValue;
        }
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE) {
                double a = convertirValor(valorColumna, col);
                double b = convertirValor(whereValue, col);
                return a > b;
            }
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE) {
                double a = convertirValor(valorColumna, col);
                double b = convertirValor(whereValue, col);
                return a < b;
            }
            return valorColumna < whereValue;
        }
        else if (whereOperator == "LIKE") {
            std::string patron = whereValue;
            for (size_t i = 0; i < patron.size(); ++i) {
                if (patron[i] == '%') {
                    patron.replace(i, 1, ".*");
                    i += 1;
                }
            }
            std::regex regex(patron);
            return std::regex_match(valorColumna, regex);
        }
        else if (whereOperator == "NOT") {
            return valorColumna != whereValue;
        }
        return false;
        };

    // Filtrar filas
    std::vector<std::vector<std::string>> filasFiltradas;
    for (const auto& par : filasConOffset) {
        if (cumpleCondicion(par.first)) {
            filasFiltradas.push_back(par.first);
        }
    }

    // Seleccionar solo las columnas pedidas
    std::vector<std::vector<std::string>> filasResultado;
    for (const auto& fila : filasFiltradas) {
        std::vector<std::string> filaSeleccionada;
        for (int idx : indicesSeleccionados) {
            filaSeleccionada.push_back(fila[idx]);
        }
        filasResultado.push_back(filaSeleccionada);
    }

    // Si hay ORDER BY, ordenar
    if (!orderColumn.empty()) {
        // Buscar indice de la columna de orden 
        int idxOrder = -1;
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            if (columnasTabla[i].name == orderColumn) {
                idxOrder = (int)i;
                break;
            }
        }
        if (idxOrder == -1) {
            r.error = "Column '" + orderColumn + "' does not exist for ORDER BY";
            return r;
        }

        // Usar std::function para lambda recursivo
        std::function<void(std::vector<std::vector<std::string>>&, int, int)> quicksort =
            [&](std::vector<std::vector<std::string>>& arr, int left, int right) {
            if (left >= right) return;

            // Elegir pivote (medio)
            int mid = (left + right) / 2;
            const Columna& colOrder = columnasTabla[idxOrder];
            std::string pivotStr = arr[mid][idxOrder];

            // Funcion de comparacion segun tipo
            auto menor = [&](const std::string& a, const std::string& b) -> bool {
                if (colOrder.type == TipoColumna::INTEGER || colOrder.type == TipoColumna::DOUBLE) {
                    return std::stod(a) < std::stod(b);
                }
                return a < b;
                };

            int i = left, j = right;
            while (i <= j) {
                while (menor(arr[i][idxOrder], pivotStr)) i++;
                while (menor(pivotStr, arr[j][idxOrder])) j--;
                if (i <= j) {
                    std::swap(arr[i], arr[j]);
                    i++; j--;
                }
            }
            quicksort(arr, left, j);
            quicksort(arr, i, right);
            };

        quicksort(filasResultado, 0, (int)filasResultado.size() - 1);

        // Invertir si es DESC
        if (orderDirection == "DESC") {
            std::reverse(filasResultado.begin(), filasResultado.end());
        }
    }

    // Preparar el resultado
    r.success = true;
    r.type = "select";
    r.message = std::to_string(filasResultado.size()) + " rows selected";

    // Llenar columns con los nombres de las columnas seleccionadas
    for (int idx : indicesSeleccionados) {
        r.columns.push_back(columnasTabla[idx].name);
    }

    // Copiar las filas resultado
    r.rows = std::move(filasResultado);

    return r;
}