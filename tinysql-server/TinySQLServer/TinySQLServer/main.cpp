#include <iostream>
#include <string>
#include "../libs/httplib.h"   // Ajusta la ruta si es necesario
#include "../libs/nlohmann/json.hpp"

using json = nlohmann::json;

int main() {
    httplib::Server svr;

    // Endpoint POST /query
    svr.Post("/query", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string sql = body.value("sql", "");
            std::string database = body.value("database", "");

            std::cout << "SQL recibido: " << sql << std::endl;
            std::cout << "Base de datos contexto: " << database << std::endl;

            // Respuesta simulada de SELECT
            json response;
            response["success"] = true;
            response["type"] = "select";
            response["columns"] = { "ID", "NOMBRE", "EDAD" };
            response["rows"] = {
                {1, "Ana", 25},
                {2, "Luis", 30},
                {3, "Carlos", 22}
            };
            response["elapsed_ms"] = 5;

            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            error["elapsed_ms"] = 0;
            res.set_content(error.dump(), "application/json");
        }
        });

    std::cout << "Servidor TinySQLDb ejecutándose en http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);
    return 0;
}