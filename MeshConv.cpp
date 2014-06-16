#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <assert.h>

#include <assimp\mesh.h>
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

using namespace std;

// swap the bytes in a float (to change big/little-endian
inline float swap_f32(float f) {
	union {
		float f;
		uint8_t b[4];
	} u1, u2;

	u1.f = f;
	u2.b[0] = u1.b[3];
	u2.b[1] = u1.b[2];
	u2.b[2] = u1.b[1];
	u2.b[3] = u1.b[0];
	return u2.f;
}

inline uint16_t swap_u16(uint16_t i) {
	union {
		uint16_t i;
		uint8_t b[2];
	} u1, u2;

	u1.i = i;
	u2.b[0] = u1.b[1];
	u2.b[1] = u1.b[0];
	return u2.i;
}

typedef void (*CopyDataFunc)(float*, const aiVector3D&, int&);

uint32_t g_process_flags = 
	aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes |
	aiProcess_SortByPType | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_SplitLargeMeshes |
	aiProcess_RemoveRedundantMaterials;

//uint32_t g_process_flags = 
//		aiProcess_GenSmoothNormals		|
//		aiProcess_SplitLargeMeshes      |
//		aiProcess_Triangulate			|
//		aiProcess_JoinIdenticalVertices	|
//		aiProcess_SortByPType;

void CopyData_2f(float* dest, const aiVector3D& src, int& idx) {
	dest[idx]	= swap_f32(src.x);
	dest[idx+1] = swap_f32(src.y);

	idx += 2;
}

void CopyData_3f(float* dest, const aiVector3D& src, int& idx) {
	dest[idx]	= swap_f32(src.x);
	dest[idx+1] = swap_f32(src.y);
	dest[idx+2] = swap_f32(src.z);

	idx += 3;
}

void WriteData(ofstream& outFile, aiVector3D* data_src, int n, int element_size, CopyDataFunc copyFunc) {
	int n_elements = n * element_size;
	float* data_out = new float[n_elements];
	
	for (int i = 0, v = 0; i < n; i++)
		copyFunc(data_out, data_src[i], v);

	outFile.write((char*)data_out, n_elements * sizeof(float));
	delete data_out;
}

void WriteHeader(ofstream& output, const aiScene* pScene, uint16_t len_material) {
	//n_vertex, n_normals, n_texcoord, n_faces, n_submeshes
	
	// compute total of vertices
	uint16_t n_vertices = 0;
	for (uint32_t i = 0 ; i < pScene->mNumMeshes ; i++)
		n_vertices += pScene->mMeshes[i]->mNumVertices;

	// compute total of faces
	uint16_t n_faces = 0;
	for (uint32_t i = 0 ; i < pScene->mNumMeshes ; i++)
		n_faces += pScene->mMeshes[i]->mNumFaces;

	uint16_t n_subMeshes = pScene->mNumMeshes;

	uint16_t header[] = {n_vertices, n_faces, n_subMeshes, len_material};
	header[0] = swap_u16(header[0]);
	header[1] = swap_u16(header[1]);
	header[2] = swap_u16(header[2]);
	header[3] = swap_u16(header[3]);

	output.write((char*)header, 4 * sizeof(uint16_t));

	printf("Total_Vertices: %d\n", n_vertices);
	printf("Total_Faces: %d\n", n_faces);
	printf("Total_Submeshes: %d\n", n_subMeshes);
}

void WriteMaterialName(ofstream& output, const std::string materialName) {
	const char* name = materialName.c_str();
	int size = materialName.length()+1;
	printf("Material Name=%s, size=%d\n", name, size);
	output.write(name, size);
}

void WritePositions(ofstream& output, const aiScene* pScene) {
	for (uint32_t i = 0 ; i < pScene->mNumMeshes ; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];
		WriteData(output, mesh->mVertices, mesh->mNumVertices, 3, CopyData_3f);
	}
}

void WriteNormals(ofstream& output, const aiScene* pScene) {
	for (uint32_t i = 0 ; i < pScene->mNumMeshes ; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];
		WriteData(output, mesh->mNormals, mesh->mNumVertices, 3, CopyData_3f);
	}
}

void WriteTexCoord(ofstream& output, const aiScene* pScene) {
	for (uint32_t i = 0 ; i < pScene->mNumMeshes ; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];
		if (mesh->HasTextureCoords(0))
			WriteData(output, mesh->mTextureCoords[0], mesh->mNumVertices, 2, CopyData_2f);
	}
}

void WriteIndices(ofstream& output, const aiScene* pScene) {
	int acc = 0;
	for (uint32_t i = 0 ; i < pScene->mNumMeshes ; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];

		int n_indices = 3 * mesh->mNumFaces;
		uint16_t* indices = new uint16_t[n_indices];

		for (uint32_t f = 0, idx=0; f < mesh->mNumFaces ; f++) {
			const aiFace& face = mesh->mFaces[f];
			assert(face.mNumIndices == 3);
			indices[idx]	= swap_u16(acc + face.mIndices[0]);
			indices[idx+1]  = swap_u16(acc + face.mIndices[1]);
			indices[idx+2]  = swap_u16(acc + face.mIndices[2]);

			idx += 3;
		}

		output.write((char*)indices, n_indices * sizeof(uint16_t));
		delete[] indices;

		acc += mesh->mNumVertices;
	}
}

void WriteSubMeshes(ofstream& output, const aiScene* pScene) {
	int n_elements = 2*pScene->mNumMeshes;

	uint16_t* subMeshes = new uint16_t[n_elements];
	uint16_t start = 0;
	for (uint32_t i = 0, m = 0; i < pScene->mNumMeshes ; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];
	
		uint16_t n_tris = mesh->mNumFaces;

		printf("\tSubMesh %d, start=%d, size=%d\n", i, start, n_tris);
		
		subMeshes[m]	= swap_u16(start);
		subMeshes[m+1]	= swap_u16(n_tris);
		
		m += 2;
		start += n_tris;
	}
	
	output.write((char*)subMeshes, n_elements * sizeof(uint16_t));
	delete[] subMeshes;
}

void WriteSubMeshMaterials(ofstream& output, const aiScene* pScene) {
	int n = pScene->mNumMeshes;
	uint8_t* materials = new uint8_t[n];
	for (uint32_t i = 0; i < pScene->mNumMeshes ; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];
	
		uint8_t materialIdx = mesh->mMaterialIndex - 1;
		
		materials[i]	= materialIdx;
	}

	output.write((char*)materials, n * sizeof(uint8_t));
	delete[] materials;
}


void MaterialInfo(const aiScene* pScene) {
	for (uint32_t i = 0; i < pScene->mNumMeshes; i++) {
		const aiMesh* mesh = pScene->mMeshes[i];
		const aiMaterial* pMaterial = pScene->mMaterials[mesh->mMaterialIndex];

		int nt = pMaterial->GetTextureCount(aiTextureType_DIFFUSE);
		printf("Mesh %d, Material Id: %d, TextureCount: %d", i, mesh->mMaterialIndex, nt);
		
		if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString path;
			std::string dir = ".";
			if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
				std::string fullPath = dir + "/" + path.data;
				printf(", Texture: %s", fullPath.c_str());
			}
		}
		puts("");
	}
}

bool ConvertMesh(const std::string& filename) {
	ofstream output(filename + ".m", ios::out | ios::binary);

	printf("converting mesh: %s.obj\n", filename.c_str());
	
	Assimp::Importer Importer;
	const aiScene* pScene = Importer.ReadFile(filename + ".obj", g_process_flags);
    const aiVector3D vecZero;

	std::string materialName = filename + ".mat";
    bool ret = false;
    if (pScene) {
		WriteHeader(output, pScene, materialName.length() + 1);
		WriteMaterialName(output, materialName);
		WritePositions(output, pScene);
		WriteNormals(output, pScene);
		WriteTexCoord(output, pScene);
		WriteIndices(output, pScene);
		WriteSubMeshes(output, pScene);
		WriteSubMeshMaterials(output, pScene);

		MaterialInfo(pScene);
    }
    else {
		printf("Error parsing '%s': '%s'\n", filename.c_str(), Importer.GetErrorString());
    }

	output.close();

    return ret;
}

bool MeshInfo(std::string& filename) {
	string filenameFull = filename + ".obj";
	printf("Mesh Info: %s\n", filenameFull.c_str());
    bool ret = false;
	Assimp::Importer Importer;

    const aiScene* pScene = Importer.ReadFile(filenameFull.c_str(), g_process_flags);
    const aiVector3D vecZero;

    if (pScene) {
		printf("#Meshes: %d\n", pScene->mNumMeshes);
		for (unsigned int i = 0 ; i < pScene->mNumMeshes ; i++) {
			printf("mesh[%d]:", i);
			const aiMesh* mesh = pScene->mMeshes[i];

			printf(" (normals: %s,", mesh->HasNormals() ? "yes" : "no");
			printf(" texcoord: %s)\n", mesh->HasTextureCoords(0) ? "yes" : "no");
			printf("\tVertices (%d):\n", mesh->mNumVertices);
			for (unsigned int v = 0 ; v < mesh->mNumVertices; v++) {
				const aiVector3D* pos = &(mesh->mVertices[v]);
				printf("\t\t(%.3f, %.3f, %.3f)\n", pos->x, pos->y, pos->z);
			}

			printf("\tNormals(%d):\n", mesh->mNumVertices);
			for (unsigned int v = 0 ; v < mesh->mNumVertices; v++) {
				const aiVector3D* normal = &(mesh->mNormals[v]);
				printf("\t\t(%.3f, %.3f, %.3f)\n", normal->x, normal->y, normal->z);
			}

			if (mesh->HasTextureCoords(0)) {
				printf("\tTexCoord(%d):\n", mesh->mNumVertices);
				for (unsigned int t = 0 ; t < mesh->mNumVertices; t++) {
					const aiVector3D& normal = mesh->mTextureCoords[0][t];
					printf("\t\t(%.3f, %.3f)\n", normal.x, normal.y);
				}
			}

			printf("\tFaces (%d):\n", mesh->mNumFaces);
			for (unsigned int f = 0 ; f < mesh->mNumFaces ; f++) {
				const aiFace& face = mesh->mFaces[f];
				assert(face.mNumIndices == 3);
				printf("\t\t%d: (%d, %d, %d)\n", f, face.mIndices[0], face.mIndices[1], face.mIndices[2]);
			}
		}
    }
    else {
		printf("Error parsing '%s': '%s'\n", filename.c_str(), Importer.GetErrorString());
    }

    return ret;
}

long fsize(FILE* f) {
	fseek(f, 0, SEEK_END); 
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	return size;
}

void WriteMaterial(const std::string& filename) {
	Assimp::Importer Importer;
    const aiScene* pScene = Importer.ReadFile(filename + ".obj", g_process_flags);

	ofstream output(filename + ".mat", ios::out | ios::binary);

	std::string path = filename+".mat";
	const char* matName = path.c_str();
	char name_size = (char) strlen(matName)+1;
	char n_subMat = (char) pScene->mNumMaterials - 1;	// doesnt count DefaultMaterial (Material[0])

	char header[2] = {name_size, n_subMat};
	output.write((char*)header, 2);
	output.write(matName, name_size*sizeof(char));

	printf("#Materials= %d\n", n_subMat);
	for (int i = 1; i <= n_subMat; i++) {
		const aiMaterial* pMaterial = pScene->mMaterials[i];
		printf("Writing material %d, ", i);
		printf("#Textures= %d", pMaterial->GetTextureCount(aiTextureType_DIFFUSE));

		aiString name;
		pMaterial->Get(AI_MATKEY_NAME, name);

		printf(", name=%s\n", name.C_Str());
		//if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString path;
			//std::string dir = ".";
			if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
				const char* diffuse_name = path.data;
				char diffuse_size = (char) strlen(diffuse_name) + 1;
				//printf("Texture: %s\n", fullPath.c_str());
				output.write(&diffuse_size, 1);
				output.write(diffuse_name, diffuse_size);
			}
		//}
	}

	output.close();
}

void ReadMaterial(const std::string& filename) {
	ifstream input(filename + ".mat", ios::in | ios::binary);

	char size_info[2];
	input.read(size_info, 2*sizeof(char));

	char name_size = size_info[0];
	char n_subMat = size_info[1];

	char name[256];
	input.read(name, name_size*sizeof(char));

	printf("read material: %s (%d submaterials)\n", name, n_subMat);

	for (int i = 0; i < n_subMat; i++) {
		char diffuse_size;
		input.read(&diffuse_size, 1);

		char tex_diffuse[256];
		input.read(tex_diffuse, diffuse_size);
		
		printf("\tMat %d: %s\n", i, tex_diffuse);
	}
}

/*
void read_input(int argc, char** argv, char* file_in, char* file_out) {
	for (int i = 1; i < argc; i++) {
		char* arg = argv[i];
		if (strcmp(arg, "-i") == 0)
			strcpy(file_in, argv[i+1]);
		else if (strcmp(arg, "-o") == 0)
			strcpy(file_out, argv[i+1]);
	}
}
*/

struct Obj {
	int id;
};

int main(int argc, char **argv) {
	if (argc < 2) {
		puts("usage: prog meshname");
		exit(0);
	}

	std::string filename = argv[1];
	//std::string filename = "box";

	Obj a[10];

	for (int i = 0; i < 10; i++) {
		a[i].id = i;
	}

	//Obj& a3 = a[3];
	//a3 = a[0];

	//for (int i = 0; i < 10; i++) {
	//	printf("a[%d].id = %d\n", i, a[i].id);
	//}

	//MeshInfo(filename);
	ConvertMesh(filename);
	WriteMaterial(filename);
	ReadMaterial(filename);

	system("pause");

    return 0;
}