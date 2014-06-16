#include <stdlib.h>

#include "Mesh.h"

void mesh_read(const char* filename, Mesh& out);

int main(int argc, char **argv) {
	Mesh mesh;
	mesh_read("box.m", mesh);

	system("pause");

	return 0;
}