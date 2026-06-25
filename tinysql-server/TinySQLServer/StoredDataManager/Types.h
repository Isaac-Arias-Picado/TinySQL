#pragma once
#include <string>
#include <vector>

struct QueryResult {
    bool success = false;
    std::string type;                              
    std::string message;                
    std::string error;                             
    long long elapsed_ms = 0;
    std::vector<std::string> columns;              
    std::vector<std::vector<std::string>> rows;   
    int affected_rows = 0;
};

enum class TipoColumna {
    INTEGER,   
    DOUBLE,    
    VARCHAR,   
    DATETIME   
};

struct Columna {
    std::string name;      
    TipoColumna type;     
    int size;               
    bool nullable;       
};

struct Key {
    std::string value;
    TipoColumna type;

    // Constructor
    Key(const std::string& val, TipoColumna t) : value(val), type(t) {}

    // Operador de comparación menor que
    bool operator<(const Key& other) const {
        if (type != other.type) return false; // No debería pasar
        switch (type) {
        case TipoColumna::INTEGER:
            return std::stoi(value) < std::stoi(other.value);
        case TipoColumna::DOUBLE:
            return std::stod(value) < std::stod(other.value);
        case TipoColumna::DATETIME:
        case TipoColumna::VARCHAR:
        default:
            return value < other.value; // Lexicográfico
        }
    }

    bool operator==(const Key& other) const {
        if (type != other.type) return false;
        switch (type) {
        case TipoColumna::INTEGER:
            return std::stoi(value) == std::stoi(other.value);
        case TipoColumna::DOUBLE:
            return std::stod(value) == std::stod(other.value);
        default:
            return value == other.value;
        }
    }

    bool operator>(const Key& other) const { return other < *this; }
    bool operator<=(const Key& other) const { return !(*this > other); }
    bool operator>=(const Key& other) const { return !(*this < other); }
};
