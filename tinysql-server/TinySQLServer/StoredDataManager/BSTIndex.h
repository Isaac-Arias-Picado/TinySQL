#pragma once
#include "Index.h"
#include <vector>

class BSTIndex : public Index {
private:
    struct Nodo {
        Key key;
        size_t offset;
        Nodo* izquierda;
        Nodo* derecha;

        Nodo(const Key& k, size_t off) : key(k), offset(off), izquierda(nullptr), derecha(nullptr) {}
    };

    Nodo* raiz;
    TipoColumna tipoColumna;

    void insertarRec(Nodo*& nodo, const Key& key, size_t offset);
    Nodo* buscarRec(Nodo* nodo, const Key& key) const;
    void recorridoInorder(Nodo* nodo, std::vector<Key>& keys) const;
    Nodo* eliminarRec(Nodo* nodo, const Key& key, bool& eliminado);
    Nodo* obtenerMinimo(Nodo* nodo) const;
    void destruirArbol(Nodo* nodo);

public:
    explicit BSTIndex(TipoColumna tipo);
    ~BSTIndex();

    void insertar(const Key& key, size_t offset) override;
    size_t buscar(const Key& key) const override;
    std::vector<size_t> buscarRango(const Key& inicio, const Key& fin) const override;
    void eliminar(const Key& key) override;
    bool existe(const Key& key) const override;
    std::vector<Key> obtenerTodasLasLlaves() const override;
};