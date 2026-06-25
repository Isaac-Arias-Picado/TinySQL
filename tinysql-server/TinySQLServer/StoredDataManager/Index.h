#pragma once
#include "types.h"
#include <vector>
#include <cstddef> 

class Index {
public:
    virtual ~Index() = default;

    virtual void insertar(const Key& key, size_t offset) = 0;

    virtual size_t buscar(const Key& key) const = 0;
    virtual std::vector<size_t> buscarRango(const Key& inicio, const Key& fin) const = 0;
    virtual void eliminar(const Key& key) = 0;
    virtual bool existe(const Key& key) const = 0;
    virtual std::vector<Key> obtenerTodasLasLlaves() const = 0;
};