#include "StoredDataManager.h"
#include "SystemCatalog.h"
#include "Cifrado.h"
#include <fstream>
#include <cstring>
#include <regex>
#include <functional>
#include <algorithm>
#include <cctype>

//INSERT

// Inserta una fila: valida tipos y duplicados, arma el registro de tamano fijo,
// lo encripta y lo agrega al final del archivo. Luego actualiza los indices.
QueryResult StoredDataManager::insertarFila(const std::string& dbName,
    const std::string& tableName,
    const std::vector<std::string>& valores) {
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

    // Validar formato de las columnas DATETIME antes de escribir
    for (size_t i = 0; i < columnas.size(); ++i) {
        if (columnas[i].type == TipoColumna::DATETIME && !esFechaValida(valores[i])) {
            r.error = "Invalid datetime format: " + valores[i];
            return r;
        }
    }

    // Si alguna columna tiene indice, rechazar valores repetidos
    for (size_t i = 0; i < columnas.size(); ++i) {
        Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
        if (idx) {
            Key key(valores[i], columnas[i].type);
            if (idx->existe(key)) {
                r.error = "Duplicate value in indexed column: " + columnas[i].name;
                return r;
            }
        }
    }

    // Armar el registro completo en un buffer de tamano fijo
    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    std::vector<char> registro(registroSize, 0);
    size_t pos = 0;
    for (size_t i = 0; i < columnas.size(); ++i) {
        const Columna& col = columnas[i];
        const std::string& valor = valores[i];
        if (col.type == TipoColumna::INTEGER) {
            int num = std::stoi(valor);
            std::memcpy(registro.data() + pos, &num, sizeof(int));
            pos += sizeof(int);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double num = std::stod(valor);
            std::memcpy(registro.data() + pos, &num, sizeof(double));
            pos += sizeof(double);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            for (int j = 0; j < (int)valor.size() && j < col.size; ++j)
                registro[pos + j] = valor[j];
            pos += col.size;
        }
        else if (col.type == TipoColumna::DATETIME) {
            for (int j = 0; j < (int)valor.size() && j < 19; ++j)
                registro[pos + j] = valor[j];
            pos += 19;
        }
    }

    // Encriptar la fila completa antes de guardarla
    encriptar(registro.data(), (int)registroSize);

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ofstream file(rutaTabla, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        r.error = "Cannot open table file";
        return r;
    }

    // El offset de esta fila es el final actual del archivo, lo guardan los indices
    file.seekp(0, std::ios::end);
    size_t offset = file.tellp();
    file.write(registro.data(), registroSize);
    file.close();

    // Agregar el valor de cada columna indexada al arbol correspondiente
    for (size_t i = 0; i < columnas.size(); ++i) {
        Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
        if (idx) {
            Key key(valores[i], columnas[i].type);
            idx->insertar(key, offset);
        }
    }

    r.success = true;
    r.type = "dml";
    r.message = "Row inserted";
    return r;
}

//DELETE

// Borra las filas que cumplen el WHERE (o todas si no hay WHERE). Lee todo el
// archivo desencriptando, separa las filas que se quedan de las que se borran,
// quita del indice las claves de las borradas y reescribe el archivo.
QueryResult StoredDataManager::eliminarFilas(const std::string& dbName,
    const std::string& tableName,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue) {
    QueryResult r;

    if (dbName.empty()) { r.error = "No database selected"; return r; }
    if (!databases.count(dbName)) { r.error = "Database does not exist"; return r; }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) { r.error = "Table does not exist"; return r; }

    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream inFile(rutaTabla, std::ios::binary);
    if (!inFile.is_open()) { r.error = "Cannot open table file"; return r; }

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    // Leer y desencriptar todos los registros a memoria
    std::vector<std::vector<char>> todosLosRegistros;
    std::vector<char> buffer(registroSize);
    while (inFile.read(buffer.data(), registroSize)) {
        encriptar(buffer.data(), (int)registroSize);
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

    // Ubicar la columna del WHERE (si hay)
    int colIndex = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].name == whereColumn) { colIndex = (int)i; break; }
        }
        if (colIndex == -1) {
            r.error = "Column '" + whereColumn + "' does not exist";
            return r;
        }
    }

    // offset (en bytes) donde empieza una columna dentro del registro
    auto offsetDe = [&](int idx) -> size_t {
        size_t off = 0;
        for (int i = 0; i < idx; ++i) off += columnas[i].size;
        return off;
        };

    // Convierte el contenido binario de una columna a string
    auto extraerValor = [&](const std::vector<char>& reg, int idx) -> std::string {
        const Columna& col = columnas[idx];
        const char* data = reg.data() + offsetDe(idx);
        if (col.type == TipoColumna::INTEGER) {
            int val; std::memcpy(&val, data, sizeof(int)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double val; std::memcpy(&val, data, sizeof(double)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            std::string s;
            for (int j = 0; j < col.size; ++j) { if (data[j] == '\0') break; s.push_back(data[j]); }
            return s;
        }
        else if (col.type == TipoColumna::DATETIME) {
            return std::string(data, 19);
        }
        return "";
        };

    // Decide si una fila cumple la condicion del WHERE
    auto cumpleCondicion = [&](const std::vector<char>& reg) -> bool {
        if (whereColumn.empty()) return true;  // sin WHERE se borran todas
        std::string valorColumna = extraerValor(reg, colIndex);
        const Columna& col = columnas[colIndex];
        if (whereOperator == "=") return valorColumna == whereValue;
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) > std::stod(whereValue);
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) < std::stod(whereValue);
            return valorColumna < whereValue;
        }
        else if (whereOperator == "LIKE") {
            std::string valorLower = valorColumna;
            std::string patron = whereValue;
            for (auto& c : valorLower) c = (char)tolower((unsigned char)c);
            for (auto& c : patron) c = (char)tolower((unsigned char)c);
            for (size_t i = 0; i < patron.size(); ++i) {
                if (patron[i] == '%') { patron.replace(i, 1, ".*"); i += 1; }
            }
            std::regex regex(patron);
            return std::regex_match(valorLower, regex);
        }
        else if (whereOperator == "NOT") return valorColumna != whereValue;
        return false;
        };

    // Separar las filas que se quedan de las que se borran
    std::vector<std::vector<char>> nuevosRegistros;
    std::vector<std::vector<char>> registrosBorrados;
    for (const auto& reg : todosLosRegistros) {
        if (cumpleCondicion(reg)) {
            registrosBorrados.push_back(reg);
        }
        else {
            nuevosRegistros.push_back(reg);
        }
    }

    // Quitar de los indices las claves de las filas borradas
    for (const auto& reg : registrosBorrados) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            Index* idx = obtenerIndice(dbName, tableName, columnas[i].name);
            if (idx) {
                Key key(extraerValor(reg, (int)i), columnas[i].type);
                idx->eliminar(key);
            }
        }
    }

    // Reescribir el archivo solo con las filas que quedan, re-encriptando
    std::ofstream outFile(rutaTabla, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) { r.error = "Cannot write table file"; return r; }
    for (auto& reg : nuevosRegistros) {
        encriptar(reg.data(), (int)registroSize);
        outFile.write(reg.data(), registroSize);
    }
    outFile.close();

    r.success = true;
    r.type = "dml";
    r.message = std::to_string(registrosBorrados.size()) + " rows deleted";
    r.affected_rows = (int)registrosBorrados.size();
    return r;
}

//SELECT

// Selecciona filas de una tabla. Soporta lista de columnas o "*", filtro WHERE
// (con indice si la condicion es de igualdad sobre una columna indexada, o
// busqueda secuencial en otro caso) y ordenamiento con Quicksort propio.
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
    if (dbName.empty()) { r.error = "No database selected"; return r; }
    if (!databases.count(dbName)) { r.error = "Database does not exist"; return r; }

    std::vector<Columna> columnasTabla = leerColumnas(dbName, tableName);
    if (columnasTabla.empty()) { r.error = "Table does not exist"; return r; }

    // Determinar que columnas se van a mostrar (todas si es "*")
    std::vector<int> indicesSeleccionados;
    bool seleccionarTodas = columnas.empty() || (columnas.size() == 1 && columnas[0] == "*");
    if (seleccionarTodas) {
        for (size_t i = 0; i < columnasTabla.size(); ++i) indicesSeleccionados.push_back((int)i);
    }
    else {
        for (const auto& col : columnas) {
            bool encontrada = false;
            for (size_t i = 0; i < columnasTabla.size(); ++i) {
                if (columnasTabla[i].name == col) {
                    indicesSeleccionados.push_back((int)i);
                    encontrada = true;
                    break;
                }
            }
            if (!encontrada) { r.error = "Column '" + col + "' does not exist"; return r; }
        }
    }

    // Ver si podemos usar indice: WHERE de igualdad sobre una columna indexada
    int colIndexWhere = -1;
    Index* idx = nullptr;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            if (columnasTabla[i].name == whereColumn) { colIndexWhere = (int)i; break; }
        }
        if (colIndexWhere == -1) { r.error = "Column '" + whereColumn + "' does not exist"; return r; }

        if (whereOperator == "=") {
            idx = obtenerIndice(dbName, tableName, whereColumn);
        }
    }

    std::vector<std::vector<std::string>> filasResultado;

    if (idx) {
        //CAMINO CON INDICE
        // El indice nos da el offset de la fila buscada. Leemos solo esa fila
        // directo del disco, sin tocar el resto de la tabla.
        Key key(whereValue, columnasTabla[colIndexWhere].type);
        size_t off = idx->buscar(key);
        if (off != static_cast<size_t>(-1)) {
            std::vector<std::string> filaCompleta = leerFilaEnOffset(dbName, tableName, off);
            if (!filaCompleta.empty()) {
                std::vector<std::string> filaSeleccionada;
                for (int i : indicesSeleccionados) {
                    filaSeleccionada.push_back(filaCompleta[i]);
                }
                filasResultado.push_back(filaSeleccionada);
            }
        }
    }
    else {
        //CAMINO SECUENCIAL
        // Sin indice (o WHERE que no es "="), leemos todas las filas y filtramos.
        auto filasConOffset = leerFilasConOffset(dbName, tableName);
        if (filasConOffset.empty()) {
            r.success = true;
            r.type = "select";
            r.message = "0 rows selected";
            return r;
        }

        // Toma el valor de una columna de una fila ya leida
        auto extraerValor = [&](const std::vector<std::string>& fila, int idx) -> std::string {
            return (idx >= 0 && idx < (int)fila.size()) ? fila[idx] : "";
            };

        // Evalua el WHERE para la busqueda secuencial
        auto cumpleCondicion = [&](const std::vector<std::string>& fila) -> bool {
            if (whereColumn.empty()) return true;
            std::string valorColumna = extraerValor(fila, colIndexWhere);
            const Columna& col = columnasTabla[colIndexWhere];
            if (whereOperator == "=") return valorColumna == whereValue;
            else if (whereOperator == ">") {
                if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                    return std::stod(valorColumna) > std::stod(whereValue);
                return valorColumna > whereValue;
            }
            else if (whereOperator == "<") {
                if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                    return std::stod(valorColumna) < std::stod(whereValue);
                return valorColumna < whereValue;
            }
            else if (whereOperator == "LIKE") {
                // Comparacion sin distinguir mayusculas. El % se traduce a ".*".
                std::string valorLower = valorColumna;
                std::string patron = whereValue;
                for (auto& c : valorLower) c = (char)tolower((unsigned char)c);
                for (auto& c : patron) c = (char)tolower((unsigned char)c);
                for (size_t i = 0; i < patron.size(); ++i) {
                    if (patron[i] == '%') { patron.replace(i, 1, ".*"); i += 1; }
                }
                std::regex regex(patron);
                return std::regex_match(valorLower, regex);
            }
            else if (whereOperator == "NOT") return valorColumna != whereValue;
            return false;
            };

        for (const auto& par : filasConOffset) {
            if (cumpleCondicion(par.first)) {
                std::vector<std::string> filaSeleccionada;
                for (int i : indicesSeleccionados) {
                    filaSeleccionada.push_back(par.first[i]);
                }
                filasResultado.push_back(filaSeleccionada);
            }
        }
    }

    // ORDER BY con Quicksort propio (no usa std::sort). 
    if (!orderColumn.empty()) {
        int idxOrder = -1;
        for (size_t i = 0; i < columnasTabla.size(); ++i) {
            if (columnasTabla[i].name == orderColumn) { idxOrder = (int)i; break; }
        }
        if (idxOrder == -1) { r.error = "Column '" + orderColumn + "' does not exist for ORDER BY"; return r; }

        // El idxOrder es sobre la tabla completa, pero filasResultado solo tiene
        // las columnas seleccionadas. Buscamos la posicion del ORDER dentro de
        // las seleccionadas.
        int posEnResultado = -1;
        for (size_t i = 0; i < indicesSeleccionados.size(); ++i) {
            if (indicesSeleccionados[i] == idxOrder) { posEnResultado = (int)i; break; }
        }

        if (posEnResultado != -1) {
            const Columna& colOrder = columnasTabla[idxOrder];
            std::function<void(std::vector<std::vector<std::string>>&, int, int)> quicksort =
                [&](std::vector<std::vector<std::string>>& arr, int left, int right) {
                if (left >= right) return;
                int mid = (left + right) / 2;
                std::string pivotStr = arr[mid][posEnResultado];
                auto menor = [&](const std::string& a, const std::string& b) -> bool {
                    if (colOrder.type == TipoColumna::INTEGER || colOrder.type == TipoColumna::DOUBLE)
                        return std::stod(a) < std::stod(b);
                    return a < b;
                    };
                int i = left, j = right;
                while (i <= j) {
                    while (menor(arr[i][posEnResultado], pivotStr)) i++;
                    while (menor(pivotStr, arr[j][posEnResultado])) j--;
                    if (i <= j) {
                        std::swap(arr[i], arr[j]);
                        i++; j--;
                    }
                }
                quicksort(arr, left, j);
                quicksort(arr, i, right);
                };
            quicksort(filasResultado, 0, (int)filasResultado.size() - 1);
            if (orderDirection == "DESC") {
                std::reverse(filasResultado.begin(), filasResultado.end());
            }
        }
    }

    r.success = true;
    r.type = "select";
    r.message = std::to_string(filasResultado.size()) + " rows selected";
    for (int i : indicesSeleccionados) r.columns.push_back(columnasTabla[i].name);
    r.rows = std::move(filasResultado);
    return r;
}

//UPDATE

// Actualiza la columna del SET en las filas que cumplen el WHERE (o todas si no
// hay WHERE). Lee desencriptando, modifica en memoria, mantiene el indice de la
// columna actualizada y reescribe el archivo re-encriptando.
QueryResult StoredDataManager::actualizarFilas(const std::string& dbName,
    const std::string& tableName,
    const std::string& setColumn,
    const std::string& setValue,
    const std::string& whereColumn,
    const std::string& whereOperator,
    const std::string& whereValue) {
    QueryResult r;
    if (dbName.empty()) { r.error = "No database selected"; return r; }
    if (!databases.count(dbName)) { r.error = "Database does not exist"; return r; }

    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) { r.error = "Table does not exist"; return r; }

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;

    // Ubicar la columna que se va a modificar
    int setIndex = -1;
    for (size_t i = 0; i < columnas.size(); ++i) {
        if (columnas[i].name == setColumn) { setIndex = (int)i; break; }
    }
    if (setIndex == -1) { r.error = "Column '" + setColumn + "' does not exist"; return r; }

    // Ubicar la columna del WHERE (si hay)
    int whereIndex = -1;
    if (!whereColumn.empty()) {
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].name == whereColumn) { whereIndex = (int)i; break; }
        }
        if (whereIndex == -1) { r.error = "Column '" + whereColumn + "' does not exist"; return r; }
    }

    if (columnas[setIndex].type == TipoColumna::DATETIME && !esFechaValida(setValue)) {
        r.error = "Invalid datetime format: " + setValue;
        return r;
    }

    // Leer y desencriptar todos los registros
    std::string rutaTabla = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream inFile(rutaTabla, std::ios::binary);
    if (!inFile.is_open()) { r.error = "Cannot open table file"; return r; }

    std::vector<std::vector<char>> registros;
    std::vector<char> buffer(registroSize);
    while (inFile.read(buffer.data(), registroSize)) {
        encriptar(buffer.data(), (int)registroSize);
        registros.push_back(buffer);
        buffer.assign(registroSize, 0);
    }
    inFile.close();

    auto offsetDe = [&](int idx) -> size_t {
        size_t off = 0;
        for (int i = 0; i < idx; ++i) off += columnas[i].size;
        return off;
        };

    auto extraerValor = [&](const std::vector<char>& reg, int idx) -> std::string {
        const Columna& col = columnas[idx];
        const char* data = reg.data() + offsetDe(idx);
        if (col.type == TipoColumna::INTEGER) {
            int val; std::memcpy(&val, data, sizeof(int)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double val; std::memcpy(&val, data, sizeof(double)); return std::to_string(val);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            std::string s;
            for (int j = 0; j < col.size; ++j) { if (data[j] == '\0') break; s.push_back(data[j]); }
            return s;
        }
        else if (col.type == TipoColumna::DATETIME) {
            return std::string(data, 19);
        }
        return "";
        };

    auto cumpleCondicion = [&](const std::vector<char>& reg) -> bool {
        if (whereColumn.empty()) return true;
        std::string valorColumna = extraerValor(reg, whereIndex);
        const Columna& col = columnas[whereIndex];
        if (whereOperator == "=") return valorColumna == whereValue;
        else if (whereOperator == ">") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) > std::stod(whereValue);
            return valorColumna > whereValue;
        }
        else if (whereOperator == "<") {
            if (col.type == TipoColumna::INTEGER || col.type == TipoColumna::DOUBLE)
                return std::stod(valorColumna) < std::stod(whereValue);
            return valorColumna < whereValue;
        }
        else if (whereOperator == "NOT") return valorColumna != whereValue;
        return false;
        };

    // Escribe el nuevo valor de la columna SET dentro de un registro
    auto escribirValorEnRegistro = [&](std::vector<char>& reg) {
        const Columna& col = columnas[setIndex];
        char* data = reg.data() + offsetDe(setIndex);
        if (col.type == TipoColumna::INTEGER) {
            int num = std::stoi(setValue);
            std::memcpy(data, &num, sizeof(int));
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double num = std::stod(setValue);
            std::memcpy(data, &num, sizeof(double));
        }
        else if (col.type == TipoColumna::VARCHAR) {
            for (int j = 0; j < col.size; ++j) data[j] = 0;
            for (int j = 0; j < (int)setValue.size() && j < col.size; ++j) data[j] = setValue[j];
        }
        else if (col.type == TipoColumna::DATETIME) {
            for (int j = 0; j < 19; ++j) data[j] = 0;
            for (int j = 0; j < (int)setValue.size() && j < 19; ++j) data[j] = setValue[j];
        }
        };

    Index* idxSet = obtenerIndice(dbName, tableName, setColumn);

    size_t actualizadas = 0;
    for (auto& reg : registros) {
        if (cumpleCondicion(reg)) {
            // Si la columna modificada tiene indice, reemplazar la clave vieja
            // por la nueva en el arbol
            if (idxSet) {
                Key keyVieja(extraerValor(reg, setIndex), columnas[setIndex].type);
                idxSet->eliminar(keyVieja);
            }
            escribirValorEnRegistro(reg);
            if (idxSet) {
                Key keyNueva(setValue, columnas[setIndex].type);
                idxSet->insertar(keyNueva, (size_t)0);
            }
            actualizadas++;
        }
    }

    // Reescribir el archivo re-encriptando cada registro
    std::ofstream outFile(rutaTabla, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) { r.error = "Cannot write table file"; return r; }
    for (auto& reg : registros) {
        encriptar(reg.data(), (int)registroSize);
        outFile.write(reg.data(), registroSize);
    }
    outFile.close();

    r.success = true;
    r.type = "dml";
    r.message = std::to_string(actualizadas) + " rows updated";
    r.affected_rows = (int)actualizadas;
    return r;
}

//Lectura de filas

// Lee todas las filas de una tabla y devuelve, por cada una, sus valores ya
// convertidos a string junto con el offset (posicion en bytes) donde empieza en
// el archivo. Ese offset es lo que los indices guardan. Cada registro se
// desencripta al leerlo.
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
        encriptar(buffer.data(), (int)registroSize);
        std::vector<std::string> fila;
        size_t pos = 0;
        for (const auto& col : columnas) {
            std::string valor;
            if (col.type == TipoColumna::INTEGER) {
                int num; std::memcpy(&num, buffer.data() + pos, sizeof(int));
                valor = std::to_string(num);
                pos += sizeof(int);
            }
            else if (col.type == TipoColumna::DOUBLE) {
                double num; std::memcpy(&num, buffer.data() + pos, sizeof(double));
                valor = std::to_string(num);
                pos += sizeof(double);
            }
            else if (col.type == TipoColumna::VARCHAR) {
                valor = std::string(buffer.data() + pos, col.size);
                valor = std::string(valor.c_str());
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

// Lee una sola fila ubicada en 'offset' bytes dentro del archivo de la tabla.
std::vector<std::string> StoredDataManager::leerFilaEnOffset(
    const std::string& dbName, const std::string& tableName, size_t offset) {
    std::vector<std::string> fila;
    std::vector<Columna> columnas = leerColumnas(dbName, tableName);
    if (columnas.empty()) return fila;

    size_t registroSize = 0;
    for (const auto& col : columnas) registroSize += col.size;
    if (registroSize == 0) return fila;

    std::string ruta = DATA_DIR + "/" + dbName + "/" + tableName;
    std::ifstream file(ruta, std::ios::binary);
    if (!file.is_open()) return fila;

    // Saltar directo a la posicion de la fila (sin leer las anteriores)
    file.seekg(offset, std::ios::beg);

    std::vector<char> buffer(registroSize);
    if (!file.read(buffer.data(), registroSize)) {
        file.close();
        return fila;
    }
    file.close();

    encriptar(buffer.data(), (int)registroSize);
    size_t pos = 0;
    for (const auto& col : columnas) {
        std::string valor;
        if (col.type == TipoColumna::INTEGER) {
            int num; std::memcpy(&num, buffer.data() + pos, sizeof(int));
            valor = std::to_string(num);
            pos += sizeof(int);
        }
        else if (col.type == TipoColumna::DOUBLE) {
            double num; std::memcpy(&num, buffer.data() + pos, sizeof(double));
            valor = std::to_string(num);
            pos += sizeof(double);
        }
        else if (col.type == TipoColumna::VARCHAR) {
            valor = std::string(buffer.data() + pos, col.size);
            valor = std::string(valor.c_str());
            pos += col.size;
        }
        else if (col.type == TipoColumna::DATETIME) {
            valor = std::string(buffer.data() + pos, 19);
            pos += 19;
        }
        fila.push_back(valor);
    }
    return fila;
}

//Validacion

// Verifica que una fecha tenga el formato correcto
bool StoredDataManager::esFechaValida(const std::string& fecha) {
    if (fecha.size() != 19) return false;
    if (fecha[4] != '-' || fecha[7] != '-' || fecha[10] != ' ') return false;
    if (fecha[13] != ':' || fecha[16] != ':') return false;
    return true;
}