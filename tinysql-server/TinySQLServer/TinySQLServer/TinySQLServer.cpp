#include <iostream>
#include <string>
#include <set>
#include <filesystem>
#include "httplib.h"
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

const std::string DATA_DIR = "./data";
std::set<std::string> databases;

//quita espacios y punto y coma final
std::string limpiarNombre(const std::string& raw) {
    std::string nombre = raw;
    // eliminar espacios al inicio
    size_t start = nombre.find_first_not_of(" \t");
    if (start != std::string::npos) nombre = nombre.substr(start);
    // eliminar espacios al final
    size_t end = nombre.find_last_not_of(" \t");
    if (end != std::string::npos) nombre = nombre.substr(0, end + 1);
    // quitar punto y coma final si existe
    if (!nombre.empty() && nombre.back() == ';') nombre.pop_back();
    return nombre;
}

// Carga las bases de datos desde el directorio
void cargarBasesDeDatos() {
    if (!fs::exists(DATA_DIR)) fs::create_directory(DATA_DIR);
    for (const auto& entry : fs::directory_iterator(DATA_DIR)) {
        if (entry.is_directory())
            databases.insert(entry.path().filename().string());
    }
    std::cout << "Bases de datos cargadas: ";
    for (const auto& db : databases) std::cout << db << " ";
    std::cout << std::endl;
}

// Manejar CREATE DATABASE
json crearBaseDatos(const std::string& nombre) {
    if (databases.count(nombre)) {
        return { {"success", false}, {"error", "Database already exists"}, {"elapsed_ms", 0} };
    }
    fs::create_directory(DATA_DIR + "/" + nombre);
    databases.insert(nombre);
    return { {"success", true}, {"type", "ddl"}, {"message", "Database created"}, {"elapsed_ms", 5} };
}

// Manejar DROP DATABASE
json eliminarBaseDatos(const std::string& nombre) {
    if (!databases.count(nombre)) {
        return { {"success", false}, {"error", "Database does not exist"}, {"elapsed_ms", 0} };
    }
    fs::remove_all(DATA_DIR + "/" + nombre);
    databases.erase(nombre);
    return { {"success", true}, {"type", "ddl"}, {"message", "Database dropped"}, {"elapsed_ms", 5} };
}

// Manejar SET DATABASE
json establecerBaseDatos(const std::string& nombre) {
    if (!databases.count(nombre)) {
        return { {"success", false}, {"error", "Database does not exist"}, {"elapsed_ms", 0} };
    }
    return { {"success", true}, {"type", "ddl"}, {"message", "Context set to " + nombre}, {"elapsed_ms", 2} };
}

int main() {
    cargarBasesDeDatos();

    httplib::Server svr;

    svr.Options("/query", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
        });

    svr.Post("/query", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            json body = json::parse(req.body);
            std::string sql = body.value("sql", "");
            std::string database = body.value("database", "");
            std::cout << "SQL: " << sql << " | Contexto BD: " << database << std::endl;

            // Convertir a mayusculas para comparar comando
            std::string cmd = sql;
            for (auto& c : cmd) c = toupper(c);

            json respuesta;

            if (cmd.rfind("CREATE DATABASE", 0) == 0) {
                std::string nombre = limpiarNombre(sql.substr(16));
                respuesta = crearBaseDatos(nombre);
            }
            else if (cmd.rfind("DROP DATABASE", 0) == 0) {
                std::string nombre = limpiarNombre(sql.substr(13));
                respuesta = eliminarBaseDatos(nombre);
            }
            else if (cmd.rfind("SET DATABASE", 0) == 0) {
                std::string nombre = limpiarNombre(sql.substr(12));
                respuesta = establecerBaseDatos(nombre);
            }
            else {
                respuesta = { {"success", false}, {"error", "Comando no implementado"}, {"elapsed_ms", 0} };
            }

            res.set_content(respuesta.dump(), "application/json");
        }
        catch (const std::exception& e) {
            json error = { {"success", false}, {"error", e.what()}, {"elapsed_ms", 0} };
            res.set_content(error.dump(), "application/json");
        }
        });

    std::cout << "Servidor TinySQLDb en http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);
    return 0;
}