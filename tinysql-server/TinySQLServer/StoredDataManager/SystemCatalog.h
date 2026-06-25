#pragma once
#include <array>
#include <string>
#include <vector>
#include "Types.h"

const int registrysize = 64;

std::array<char, registrysize> aRegistroFijo(const std::string& name);
void escribirBaseDatos(const std::string& dbName);
std::vector<std::string> leerBasesDatos();
void escribirTabla(const std::string& dbName, const std::string& tableName);
void escribirColumna(const std::string& dbName, const std::string& tableName, const Columna& col, int orden);
std::vector<Columna> leerColumnas(const std::string& dbName, const std::string& tableName);
void eliminarTablaDelCatalogo(const std::string& dbName, const std::string& tableName);
void eliminarColumnasDeTabla(const std::string& dbName, const std::string& tableName);


void escribirIndice(const std::string& dbName,
    const std::string& tableName,
    const std::string& indexName,
    const std::string& columnName,
    int tipoColumna,    
    const std::string& tipoArbol); 

std::vector<IndiceInfo> leerIndices();

void eliminarIndicesDeTabla(const std::string& dbName, const std::string& tableName);