#include "SystemCatalog.h"
#include <iostream>
#include <array>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>

std::array<char, registrysize> aRegistroFijo(const std::string& name) {
	std::array<char, registrysize> buffer{};
	if (name.size() >= registrysize) {
		std::cout << "Numero de caracteres maximo (63) del nombre alcanzados, se recorto el nombre";
		for (int i = 0; i < registrysize - 1; i++) {
			buffer[i] = name[i];
		}
	}
	else {
		for (int i = 0; i < name.size(); i++) {
			buffer[i] = name[i];
		}
	}
	return buffer;
}

void escribirBaseDatos(const std::string& dbName) {
	std::filesystem::create_directories("./data/SystemCatalog");
	std::array<char, registrysize> buffer = aRegistroFijo(dbName);
	const std::string ruta = "./data/SystemCatalog/SystemDatabases";
	std::ofstream file(ruta, std::ios::binary | std::ios::app);
	if (!file.is_open()) {
		std::cout << "Error: No existe aun el archivo SystemDataBase";
		return;
	}
	file.write(buffer.data(), buffer.size());
	file.close();
}

std::vector<std::string> leerBasesDatos() {
	std::vector<std::string> nombres;
	const std::string ruta = "./data/SystemCatalog/SystemDatabases";
	std::ifstream file(ruta, std::ios::binary);
	if (!file.is_open()) {
		return nombres;  // si no existe el archivo, devuelve vacío
	}
	std::array<char, registrysize> buffer{};
	while (file.read(buffer.data(), buffer.size())) {
		std::string nombre(buffer.data());  
		nombres.push_back(nombre);
		buffer.fill(0);  
	}
	file.close();
	return nombres;
}
