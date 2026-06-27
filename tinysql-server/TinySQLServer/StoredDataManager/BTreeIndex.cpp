#include "BTreeIndex.h"
#include <algorithm>
#include <stdexcept>

BTreeIndex::BTreeIndex(TipoColumna tipo, int grado)
    : tipoColumna(tipo), gradoMinimo(grado), raiz(nullptr) {
}

// Insercion. Si la raiz esta llena, se crea una raiz nueva y se divide la
// anterior antes de insertar (es la unica forma de que el arbol gane altura).
void BTreeIndex::insertar(const Key& key, size_t offset) {
    if (!raiz) {
        raiz = new Nodo(true);
        raiz->keys.push_back(key);
        raiz->offsets.push_back(offset);
        return;
    }

    if (static_cast<int>(raiz->keys.size()) == 2 * gradoMinimo - 1) {
        Nodo* nuevaRaiz = new Nodo(false);
        nuevaRaiz->children.push_back(raiz);
        raiz = nuevaRaiz;
        dividirHijo(raiz, 0);
    }

    insertarNoLleno(raiz, key, offset);
}

// Divide el hijo lleno que esta en padre->children[indice]. El hijo tiene
// 2t-1 claves: las primeras t-1 se quedan, la mediana (posicion t-1) sube al
// padre, y las ultimas t-1 pasan a un hijo nuevo.
void BTreeIndex::dividirHijo(Nodo* padre, int indice) {
    Nodo* hijo = padre->children[indice];
    Nodo* nuevoHijo = new Nodo(hijo->is_leaf);

    int t = gradoMinimo;

    // Copiar la mitad derecha (claves t..2t-2) al hijo nuevo
    for (int i = 0; i < t - 1; ++i) {
        nuevoHijo->keys.push_back(hijo->keys[i + t]);
        nuevoHijo->offsets.push_back(hijo->offsets[i + t]);
    }

    // Si no es hoja, tambien se reparten los hijos correspondientes
    if (!hijo->is_leaf) {
        for (int i = 0; i < t; ++i) {
            nuevoHijo->children.push_back(hijo->children[i + t]);
        }
    }

    // Guardamos la mediana ANTES de recortar el hijo. Si recortaramos primero,
    // la posicion [t-1] quedaria fuera del vector y el acceso seria invalido.
    Key claveMediana = hijo->keys[t - 1];
    size_t offsetMediana = hijo->offsets[t - 1];

    // El hijo original se queda solo con sus primeras t-1 claves
    hijo->keys.resize(t - 1);
    hijo->offsets.resize(t - 1);
    if (!hijo->is_leaf) {
        hijo->children.resize(t);
    }

    // La mediana sube al padre y el hijo nuevo se engancha a su derecha
    padre->keys.insert(padre->keys.begin() + indice, claveMediana);
    padre->offsets.insert(padre->offsets.begin() + indice, offsetMediana);
    padre->children.insert(padre->children.begin() + indice + 1, nuevoHijo);
}

// Inserta en un nodo que se garantiza no lleno. En una hoja, ubica la posicion
// ordenada e inserta. En un nodo interno, baja al hijo que corresponde; si ese
// hijo esta lleno lo divide primero (para no quedar atrapado en un nodo lleno).
void BTreeIndex::insertarNoLleno(Nodo* nodo, const Key& key, size_t offset) {
    int i = static_cast<int>(nodo->keys.size()) - 1;

    if (nodo->is_leaf) {
        while (i >= 0 && key < nodo->keys[i]) {
            --i;
        }
        ++i;
        nodo->keys.insert(nodo->keys.begin() + i, key);
        nodo->offsets.insert(nodo->offsets.begin() + i, offset);
    }
    else {
        while (i >= 0 && key < nodo->keys[i]) {
            --i;
        }
        ++i;
        Nodo* hijo = nodo->children[i];

        if (static_cast<int>(hijo->keys.size()) == 2 * gradoMinimo - 1) {
            dividirHijo(nodo, i);
            // Tras dividir, la mediana subio a nodo->keys[i]; si la clave nueva
            // es mayor, va al hijo derecho que acaba de crearse
            if (nodo->keys[i] < key) {
                ++i;
            }
        }
        insertarNoLleno(nodo->children[i], key, offset);
    }
}

// Busca una clave y devuelve su offset (posicion en disco), o -1 si no existe.
size_t BTreeIndex::buscar(const Key& key) const {
    Nodo* nodo = buscarNodo(raiz, key);
    if (!nodo) return static_cast<size_t>(-1);

    for (size_t i = 0; i < nodo->keys.size(); ++i) {
        if (!(nodo->keys[i] < key) && !(key < nodo->keys[i])) {
            return nodo->offsets[i];
        }
    }
    return static_cast<size_t>(-1);
}

// Baja por el arbol buscando el nodo que contiene la clave. En cada nodo avanza
// mientras las claves sean menores; si encuentra la clave la devuelve, si llega
// a una hoja sin encontrarla devuelve nullptr, si no baja al hijo que toca.
BTreeIndex::Nodo* BTreeIndex::buscarNodo(Nodo* nodo, const Key& key) const {
    if (!nodo) return nullptr;

    size_t i = 0;
    while (i < nodo->keys.size() && nodo->keys[i] < key) {
        ++i;
    }

    if (i < nodo->keys.size() && !(key < nodo->keys[i])) {
        return nodo;
    }

    if (nodo->is_leaf) {
        return nullptr;
    }

    return buscarNodo(nodo->children[i], key);
}

bool BTreeIndex::existe(const Key& key) const {
    return buscar(key) != static_cast<size_t>(-1);
}

std::vector<size_t> BTreeIndex::buscarRango(const Key& inicio, const Key& fin) const {
    std::vector<size_t> resultados;
    if (!raiz) return resultados;
    buscarRangoRec(raiz, inicio, fin, resultados);
    return resultados;
}

// Recorre el arbol juntando los offsets de las claves que caen en [inicio, fin].
// Combina el recorrido en orden con el descenso por los hijos correspondientes.
void BTreeIndex::buscarRangoRec(const Nodo* nodo, const Key& inicio, const Key& fin,
    std::vector<size_t>& resultados) const {
    if (!nodo) return;

    size_t i = 0;
    while (i < nodo->keys.size() && nodo->keys[i] < inicio) {
        ++i;
    }

    if (!nodo->is_leaf) {
        if (i > 0) {
            buscarRangoRec(nodo->children[i - 1], inicio, fin, resultados);
        }
    }

    while (i < nodo->keys.size() && !(fin < nodo->keys[i])) {
        if (!(nodo->keys[i] < inicio)) {
            resultados.push_back(nodo->offsets[i]);
        }
        if (!nodo->is_leaf) {
            buscarRangoRec(nodo->children[i + 1], inicio, fin, resultados);
        }
        ++i;
    }

    if (!nodo->is_leaf && i < nodo->children.size()) {
        buscarRangoRec(nodo->children[i], inicio, fin, resultados);
    }
}

// Eliminacion. Borra la clave del subarbol y, si la raiz queda vacia y no es
// hoja, baja su unico hijo como nueva raiz (el arbol pierde altura).
void BTreeIndex::eliminar(const Key& key) {
    if (!raiz) return;
    eliminarDeNodo(raiz, key);

    if (raiz->keys.empty() && !raiz->is_leaf) {
        Nodo* antiguaRaiz = raiz;
        raiz = raiz->children[0];
        antiguaRaiz->children.clear();
        delete antiguaRaiz;
    }
}

// Elimina una clave de un nodo. Si esta en una hoja, se borra directo. Si esta
// en un nodo interno, se reemplaza por su predecesor o sucesor (segun cual hijo
// tenga suficientes claves) y se borra ese de mas abajo. Si ninguno alcanza, se
// fusionan. Si la clave no esta en este nodo, baja al hijo que corresponde,
// asegurandose antes de que ese hijo tenga al menos t claves.
void BTreeIndex::eliminarDeNodo(Nodo* nodo, const Key& key) {
    int idx = 0;
    while (idx < static_cast<int>(nodo->keys.size()) && nodo->keys[idx] < key) {
        ++idx;
    }

    if (idx < static_cast<int>(nodo->keys.size()) && !(key < nodo->keys[idx])) {
        if (nodo->is_leaf) {
            nodo->keys.erase(nodo->keys.begin() + idx);
            nodo->offsets.erase(nodo->offsets.begin() + idx);
        }
        else {
            Nodo* hijoIzq = nodo->children[idx];
            Nodo* hijoDer = nodo->children[idx + 1];

            // Reemplazar por el predecesor (mayor de la izquierda) si alcanza
            if (static_cast<int>(hijoIzq->keys.size()) >= gradoMinimo) {
                Nodo* predecesor = hijoIzq;
                while (!predecesor->is_leaf) {
                    predecesor = predecesor->children.back();
                }
                Key keyPred = predecesor->keys.back();
                size_t offsetPred = predecesor->offsets.back();
                nodo->keys[idx] = keyPred;
                nodo->offsets[idx] = offsetPred;
                eliminarDeNodo(hijoIzq, keyPred);
            }
            // Si no, por el sucesor (menor de la derecha) si alcanza
            else if (static_cast<int>(hijoDer->keys.size()) >= gradoMinimo) {
                Nodo* sucesor = hijoDer;
                while (!sucesor->is_leaf) {
                    sucesor = sucesor->children.front();
                }
                Key keySuc = sucesor->keys.front();
                size_t offsetSuc = sucesor->offsets.front();
                nodo->keys[idx] = keySuc;
                nodo->offsets[idx] = offsetSuc;
                eliminarDeNodo(hijoDer, keySuc);
            }
            // Si ninguno alcanza, fusionar los dos hijos y borrar de ahi
            else {
                fusionar(nodo, idx);
                eliminarDeNodo(hijoIzq, key);
            }
        }
    }
    else {
        if (nodo->is_leaf) return;  // la clave no esta en el arbol

        Nodo* hijo = nodo->children[idx];

        // Asegurar que el hijo al que vamos a bajar tenga suficientes claves
        if (static_cast<int>(hijo->keys.size()) < gradoMinimo - 1) {
            llenarHijo(nodo, idx);
        }

        // Tras un posible merge, el indice del hijo puede haber cambiado
        if (idx > static_cast<int>(nodo->keys.size())) {
            hijo = nodo->children[idx - 1];
        }
        else {
            hijo = nodo->children[idx];
        }
        eliminarDeNodo(hijo, key);
    }
}

// Rellena un hijo que quedo con muy pocas claves: le presta una de un hermano
// que tenga de sobra, o si ninguno tiene, lo fusiona con un hermano.
void BTreeIndex::llenarHijo(Nodo* nodo, int idx) {
    if (idx != 0 && static_cast<int>(nodo->children[idx - 1]->keys.size()) >= gradoMinimo) {
        prestarDeAnterior(nodo, idx);
    }
    else if (idx != static_cast<int>(nodo->children.size()) - 1 &&
        static_cast<int>(nodo->children[idx + 1]->keys.size()) >= gradoMinimo) {
        prestarDeSiguiente(nodo, idx);
    }
    else {
        if (idx != static_cast<int>(nodo->children.size()) - 1) {
            fusionar(nodo, idx);
        }
        else {
            fusionar(nodo, idx - 1);
        }
    }
}

// Toma prestada una clave del hermano izquierdo, rotando a traves del padre.
void BTreeIndex::prestarDeAnterior(Nodo* nodo, int idx) {
    Nodo* hijo = nodo->children[idx];
    Nodo* hermano = nodo->children[idx - 1];

    hijo->keys.insert(hijo->keys.begin(), nodo->keys[idx - 1]);
    hijo->offsets.insert(hijo->offsets.begin(), nodo->offsets[idx - 1]);

    if (!hijo->is_leaf) {
        hijo->children.insert(hijo->children.begin(), hermano->children.back());
        hermano->children.pop_back();
    }

    nodo->keys[idx - 1] = hermano->keys.back();
    nodo->offsets[idx - 1] = hermano->offsets.back();

    hermano->keys.pop_back();
    hermano->offsets.pop_back();
}

// Toma prestada una clave del hermano derecho, rotando a traves del padre.
void BTreeIndex::prestarDeSiguiente(Nodo* nodo, int idx) {
    Nodo* hijo = nodo->children[idx];
    Nodo* hermano = nodo->children[idx + 1];

    hijo->keys.push_back(nodo->keys[idx]);
    hijo->offsets.push_back(nodo->offsets[idx]);

    if (!hijo->is_leaf) {
        hijo->children.push_back(hermano->children.front());
        hermano->children.erase(hermano->children.begin());
    }

    nodo->keys[idx] = hermano->keys.front();
    nodo->offsets[idx] = hermano->offsets.front();

    hermano->keys.erase(hermano->keys.begin());
    hermano->offsets.erase(hermano->offsets.begin());
}

// Fusiona el hijo idx con su hermano derecho, bajando la clave del padre que
// los separaba. El nodo resultante queda con 2t-1 claves.
void BTreeIndex::fusionar(Nodo* nodo, int idx) {
    Nodo* hijo = nodo->children[idx];
    Nodo* hermano = nodo->children[idx + 1];

    hijo->keys.push_back(nodo->keys[idx]);
    hijo->offsets.push_back(nodo->offsets[idx]);

    for (size_t i = 0; i < hermano->keys.size(); ++i) {
        hijo->keys.push_back(hermano->keys[i]);
        hijo->offsets.push_back(hermano->offsets[i]);
    }

    if (!hijo->is_leaf) {
        for (size_t i = 0; i < hermano->children.size(); ++i) {
            hijo->children.push_back(hermano->children[i]);
        }
        hermano->children.clear();
    }

    nodo->keys.erase(nodo->keys.begin() + idx);
    nodo->offsets.erase(nodo->offsets.begin() + idx);
    nodo->children.erase(nodo->children.begin() + idx + 1);

    delete hermano;
}

std::vector<Key> BTreeIndex::obtenerTodasLasLlaves() const {
    std::vector<Key> keys;
    recorrerNodos(raiz, keys);
    return keys;
}

// Recorrido en orden: para cada clave, primero visita el hijo a su izquierda,
// luego la clave; al final visita el ultimo hijo. Devuelve las claves ordenadas.
void BTreeIndex::recorrerNodos(const Nodo* nodo, std::vector<Key>& keys) const {
    if (!nodo) return;
    for (size_t i = 0; i < nodo->keys.size(); ++i) {
        if (!nodo->is_leaf) {
            recorrerNodos(nodo->children[i], keys);
        }
        keys.push_back(nodo->keys[i]);
    }
    if (!nodo->is_leaf && !nodo->children.empty()) {
        recorrerNodos(nodo->children.back(), keys);
    }
}