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
