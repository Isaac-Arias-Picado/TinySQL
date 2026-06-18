#include <iostream>
#include <string>
#include <chrono>
#include "httplib.h"
#include "json.hpp"
#include "QueryProcessor.h"
#include "SystemCatalog.h"
using json = nlohmann::json;
static json toJson(const QueryResult& r) {
    json j;
    j["success"] = r.success;
    j["elapsed_ms"] = r.elapsed_ms;
    if (r.success) {
        if (!r.type.empty()) j["type"] = r.type;
        if (!r.message.empty()) j["message"] = r.message;
        if (r.type == "select") { j["columns"] = r.columns; j["rows"] = r.rows; }
    }
    else { j["error"] = r.error; }
    return j;
}
int main() {
    QueryProcessor processor;
    httplib::Server svr;
    svr.Options("/query", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
        });
    svr.Post("/query", [&processor](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            json body = json::parse(req.body);
            std::string sql = body.value("sql", "");
            std::string database = body.value("database", "");
            std::cout << "SQL: " << sql << " | Contexto BD: " << database << std::endl;
            auto inicio = std::chrono::high_resolution_clock::now();
            QueryResult r = processor.execute(sql, database);
            auto fin = std::chrono::high_resolution_clock::now();
            r.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(fin - inicio).count();
            res.set_content(toJson(r).dump(), "application/json");
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