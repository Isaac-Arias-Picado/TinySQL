#pragma once
#include "Index.h"
#include <vector>
#include <memory>

class BTreeIndex : public Index {
private:
    struct Nodo {
        std::vector<Key> keys;
        std::vector<size_t> offsets;   // Mismo tamaño que keys
        std::vector<Nodo*> children;   // Tamaño keys.size() + 1
        bool is_leaf;

        Nodo(bool leaf) : is_leaf(leaf) {}
        ~Nodo() { for (auto* child : children) delete child; }
    };

    Nodo* raiz;
    int gradoMinimo; 
    TipoColumna tipoColumna;

    void dividirHijo(Nodo* padre, int indice);
    void insertarNoLleno(Nodo* nodo, const Key& key, size_t offset);
    Nodo* buscarNodo(Nodo* nodo, const Key& key) const;
    void buscarRangoRec(const Nodo* nodo, const Key& inicio, const Key& fin, std::vector<size_t>& resultados) const;
    void recorrerNodos(const Nodo* nodo, std::vector<Key>& keys) const;
    void eliminarDeNodo(Nodo* nodo, const Key& key);
    void llenarHijo(Nodo* nodo, int indice);
    void prestarDeAnterior(Nodo* nodo, int indice);
    void prestarDeSiguiente(Nodo* nodo, int indice);
    void fusionar(Nodo* nodo, int indice);

public:
    explicit BTreeIndex(TipoColumna tipo, int grado = 3);
    ~BTreeIndex() = default;

    void insertar(const Key& key, size_t offset) override;
    size_t buscar(const Key& key) const override;
    std::vector<size_t> buscarRango(const Key& inicio, const Key& fin) const override;
    void eliminar(const Key& key) override;
    bool existe(const Key& key) const override;
    std::vector<Key> obtenerTodasLasLlaves() const override;
};