#include "BSTIndex.h"
#include <algorithm>

BSTIndex::BSTIndex(TipoColumna tipo) : tipoColumna(tipo), raiz(nullptr) {}

BSTIndex::~BSTIndex() {
    destruirArbol(raiz);
}

void BSTIndex::destruirArbol(Nodo* nodo) {
    if (!nodo) return;
    destruirArbol(nodo->izquierda);
    destruirArbol(nodo->derecha);
    delete nodo;
}

void BSTIndex::insertar(const Key& key, size_t offset) {
    insertarRec(raiz, key, offset);
}

void BSTIndex::insertarRec(Nodo*& nodo, const Key& key, size_t offset) {
    if (!nodo) {
        nodo = new Nodo(key, offset);
        return;
    }

    if (key < nodo->key) {
        insertarRec(nodo->izquierda, key, offset);
    }
    else if (nodo->key < key) {
        insertarRec(nodo->derecha, key, offset);
    }
}

size_t BSTIndex::buscar(const Key& key) const {
    Nodo* nodo = buscarRec(raiz, key);
    return nodo ? nodo->offset : static_cast<size_t>(-1);
}

BSTIndex::Nodo* BSTIndex::buscarRec(Nodo* nodo, const Key& key) const {
    if (!nodo) return nullptr;
    if (key < nodo->key) return buscarRec(nodo->izquierda, key);
    if (nodo->key < key) return buscarRec(nodo->derecha, key);
    return nodo;
}

bool BSTIndex::existe(const Key& key) const {
    return buscar(key) != static_cast<size_t>(-1);
}

std::vector<size_t> BSTIndex::buscarRango(const Key& inicio, const Key& fin) const {
    std::vector<size_t> resultados;
    std::vector<Key> todas = obtenerTodasLasLlaves();
    for (const auto& k : todas) {
        if (!(k < inicio) && !(fin < k)) {
            size_t off = buscar(k);
            if (off != static_cast<size_t>(-1))
                resultados.push_back(off);
        }
    }
    return resultados;
}

void BSTIndex::eliminar(const Key& key) {
    bool eliminado = false;
    raiz = eliminarRec(raiz, key, eliminado);
}

BSTIndex::Nodo* BSTIndex::eliminarRec(Nodo* nodo, const Key& key, bool& eliminado) {
    if (!nodo) return nullptr;

    if (key < nodo->key) {
        nodo->izquierda = eliminarRec(nodo->izquierda, key, eliminado);
    }
    else if (nodo->key < key) {
        nodo->derecha = eliminarRec(nodo->derecha, key, eliminado);
    }
    else {
        eliminado = true;
        if (!nodo->izquierda) {
            Nodo* temp = nodo->derecha;
            delete nodo;
            return temp;
        }
        else if (!nodo->derecha) {
            Nodo* temp = nodo->izquierda;
            delete nodo;
            return temp;
        }
        else {
            Nodo* minNodo = obtenerMinimo(nodo->derecha);
            nodo->key = minNodo->key;
            nodo->offset = minNodo->offset;
            nodo->derecha = eliminarRec(nodo->derecha, minNodo->key, eliminado);
        }
    }
    return nodo;
}

BSTIndex::Nodo* BSTIndex::obtenerMinimo(Nodo* nodo) const {
    while (nodo && nodo->izquierda) {
        nodo = nodo->izquierda;
    }
    return nodo;
}

std::vector<Key> BSTIndex::obtenerTodasLasLlaves() const {
    std::vector<Key> keys;
    recorridoInorder(raiz, keys);
    return keys;
}

void BSTIndex::recorridoInorder(Nodo* nodo, std::vector<Key>& keys) const {
    if (!nodo) return;
    recorridoInorder(nodo->izquierda, keys);
    keys.push_back(nodo->key);
    recorridoInorder(nodo->derecha, keys);
}