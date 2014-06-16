#include <fstream>
#include <vector>
//#include <cstdint>
//#include <gctypes.h>

#include "Mesh.h"

using namespace std;

#define PRINT_DEBUG { \
	printf("n_vertices = %d\n", header.n_vertices); \
	printf("n_tris = %d\n", header.n_tris); \
	printf("n_texcoord = %d\n", header.n_texcoord); \
	printf("n_normals = %d\n", header.n_normals); \
	printf("n_submeshes = %d\n", header.n_subMeshes); \
}

#define PRINT_I(msg, i) {printf(msg); printf("%d\n", i);}

struct header_t {
	u16 n_vertices;
	u16 n_faces;
	u16 n_subMeshes;
};

void readVec3f(ifstream& inFile, int n_elements, Vec3* out, const char* dataName) {
	float* data_src = new float[n_elements];

	inFile.read((char*) data_src, n_elements*sizeof(f32));

	for (int i = 0, k = 0; i < n_elements; i+=3, k++) {
		float x = data_src[i];
		float y = data_src[i+1];
		float z = data_src[i+2];
		out[k].set(x, y, z);

		//printf(dataName);
		//printf("[%d] = (%.3f, %.3f, %.3f)\n", k, x, y, z);
	}

	delete[] data_src;
}

void readVec2f(ifstream& inFile, int n_elements, Vec2* out, const char* dataName) {
	float* data_src = new float[n_elements];

	inFile.read((char*) data_src, n_elements*sizeof(f32));

	for (int i = 0, k = 0; i < n_elements; i+=2, k++) {
		float x = data_src[i];
		float y = data_src[i+1];
		out[k].set(x, y);

		//printf(dataName);
		//printf("[%d] = (%.3f, %.3f)\n", k, x, y);
	}

	delete[] data_src;
}

void mesh_read(const char* filename, Mesh& out) {
	// read file contents
	ifstream inFile(filename, ios::in | ios::binary);
	
	// read header
	header_t header;
	inFile.read((char*) &header, sizeof(header));

	// fill sizes info
	out.n_vertices	= header.n_vertices;
	out.n_texcoord	= header.n_vertices;
	out.n_normals	= header.n_vertices;
	out.n_subMeshes = header.n_subMeshes;

	printf("n_vertex = %d\n",	 header.n_vertices);
	printf("n_tris = %d\n",		 header.n_faces);
	printf("n_subMeshes = %d\n", header.n_subMeshes);

	// read vertices
	int n_elements = 3*out.n_vertices;
	out.vertices = new Vec3[out.n_vertices];
	readVec3f(inFile, n_elements, out.vertices, "vertices");

	// read normals
	n_elements = 3*out.n_normals;
	out.normals = new Vec3[out.n_normals];
	readVec3f(inFile, n_elements, out.normals, "normals");

	// read texture coordinates
	n_elements = 2*out.n_texcoord;
	out.texcoord = new Vec2[out.n_texcoord];
	readVec2f(inFile, n_elements, out.texcoord, "texcoords");

	// read indices
	n_elements = 3*header.n_faces;
	out.indices = new u16[n_elements];
	inFile.read((char*) out.indices, n_elements*sizeof(u16));

	//for (int i = 0; i < n_elements; i++) {
	//	printf("indices[%d]= %d\n", i, out.indices[i]);
	//}

	// read sub meshes
	n_elements = 2*out.n_subMeshes;
	u16* subMeshes_src = new u16[n_elements];
	out.subMeshes = new SubMesh[out.n_subMeshes];

	inFile.read((char*) subMeshes_src, n_elements*sizeof(u16));

	for (int i = 0,k=0; i < n_elements; i+=2,k++) {
		int start	= subMeshes_src[i];
		int n		= subMeshes_src[i+1];
		out.subMeshes[k].set(start, n);
		//printf("subMesh[%d] = (%d, %d)\n", k, start, n);
	}

	delete[] subMeshes_src;
}
