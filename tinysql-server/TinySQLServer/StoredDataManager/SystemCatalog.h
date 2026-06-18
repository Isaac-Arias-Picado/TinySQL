#pragma once
#include <array>
#include <string>
#include <vector>

const int registrysize = 64;

std::array<char, registrysize> aRegistroFijo(const std::string& name);
void escribirBaseDatos(const std::string& dbName);
std::vector<std::string> leerBasesDatos();