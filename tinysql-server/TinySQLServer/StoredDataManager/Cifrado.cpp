#include "Cifrado.h"
#include <string>

static const std::string KEY = "TinySQL";

void encriptar(char* data, int size) {
	int keylength = (int)KEY.size();
	for (int i = 0; i < size; i++) {
		data[i] = data[i] ^ KEY[i % keylength];
	}
}