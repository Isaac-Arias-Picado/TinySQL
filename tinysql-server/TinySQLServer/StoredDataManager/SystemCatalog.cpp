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

void escribirTabla(const std::string& dbName, const std::string& tableName) {
	std::filesystem::create_directories("./data/SystemCatalog");
	std::array<char, registrysize> bufBD = aRegistroFijo(dbName);
	std::array<char, registrysize> bufTabla = aRegistroFijo(tableName);
	const std::string ruta = "./data/SystemCatalog/SystemTables";
	std::ofstream file(ruta, std::ios::binary | std::ios::app);
	if (!file.is_open()) return;
	file.write(bufBD.data(), bufBD.size());
	file.write(bufTabla.data(), bufTabla.size());
	file.close();
}

void escribirColumna(const std::string& dbName, const std::string& tableName,
	const Columna& col, int orden) {
	std::filesystem::create_directories("./data/SystemCatalog");
	const std::string ruta = "./data/SystemCatalog/SystemColumns";
	std::ofstream file(ruta, std::ios::binary | std::ios::app);
	if (!file.is_open()) return;

	std::array<char, registrysize> bufBD = aRegistroFijo(dbName);
	std::array<char, registrysize> bufTabla = aRegistroFijo(tableName);
	std::array<char, registrysize> bufCol = aRegistroFijo(col.name);
	file.write(bufBD.data(), bufBD.size());
	file.write(bufTabla.data(), bufTabla.size());
	file.write(bufCol.data(), bufCol.size());

	int tipo = static_cast<int>(col.type);   
	int size = col.size;
	int nullable = col.nullable ? 1 : 0;  
	file.write(reinterpret_cast<char*>(&tipo), sizeof(int));
	file.write(reinterpret_cast<char*>(&size), sizeof(int));
	file.write(reinterpret_cast<char*>(&nullable), sizeof(int));
	file.write(reinterpret_cast<char*>(&orden), sizeof(int));
	file.close();
}

std::vector<Columna> leerColumnas(const std::string& dbName, const std::string& tableName) {
	std::vector<Columna> columnas;
	const std::string ruta = "./data/SystemCatalog/SystemColumns";
	std::ifstream file(ruta, std::ios::binary);
	if (!file.is_open()) return columnas;

	std::array<char, registrysize> bufBD{};
	std::array<char, registrysize> bufTabla{};
	std::array<char, registrysize> bufCol{};

	while (file.read(bufBD.data(), bufBD.size())) {
		file.read(bufTabla.data(), bufTabla.size());
		file.read(bufCol.data(), bufCol.size());

		int tipo, size, nullable, orden;
		file.read(reinterpret_cast<char*>(&tipo), sizeof(int));
		file.read(reinterpret_cast<char*>(&size), sizeof(int));
		file.read(reinterpret_cast<char*>(&nullable), sizeof(int));
		file.read(reinterpret_cast<char*>(&orden), sizeof(int));

		std::string dbLeido(bufBD.data());
		std::string tablaLeida(bufTabla.data());

		if (dbLeido == dbName && tablaLeida == tableName) {
			Columna col;
			col.name = std::string(bufCol.data());
			col.type = static_cast<TipoColumna>(tipo);
			col.size = size;
			col.nullable = (nullable == 1);
			columnas.push_back(col);
		}
		bufBD.fill(0); bufTabla.fill(0); bufCol.fill(0);
	}
	file.close();
	return columnas;
}

template <typename Predicate>
void reescribirArchivoFiltrado(const std::string& ruta, size_t recordSize, Predicate pred) {
	std::ifstream in(ruta, std::ios::binary);
	if (!in.is_open()) return;

	std::vector<std::vector<char>> registros;
	std::vector<char> buffer(recordSize);
	while (in.read(buffer.data(), recordSize)) {
		if (!pred(buffer)) {
			registros.push_back(buffer);  
		}
		buffer.assign(recordSize, 0);
	}
	in.close();

	std::ofstream out(ruta, std::ios::binary | std::ios::trunc);
	for (const auto& reg : registros) {
		out.write(reg.data(), reg.size());
	}
	out.close();
}

void eliminarTablaDelCatalogo(const std::string& dbName, const std::string& tableName) {
	const std::string ruta = "./data/SystemCatalog/SystemTables";
	const size_t recordSize = registrysize * 2;  

	reescribirArchivoFiltrado(ruta, recordSize, [&](const std::vector<char>& reg) -> bool {
		// El registro es: dbName (64) + tableName (64)
		std::string db(reg.data(), registrysize);
		std::string tbl(reg.data() + registrysize, registrysize);
		db = std::string(db.c_str());
		tbl = std::string(tbl.c_str());
		return (db == dbName && tbl == tableName);  
		});
}

void eliminarColumnasDeTabla(const std::string& dbName, const std::string& tableName) {
	const std::string ruta = "./data/SystemCatalog/SystemColumns";
	const size_t recordSize = registrysize * 3 + sizeof(int) * 4;

	reescribirArchivoFiltrado(ruta, recordSize, [&](const std::vector<char>& reg) -> bool {
		std::string db(reg.data(), registrysize);
		std::string tbl(reg.data() + registrysize, registrysize);
		db = std::string(db.c_str());
		tbl = std::string(tbl.c_str());
		return (db == dbName && tbl == tableName); 
		});
}
