#ifndef _MESH_H_
#define _MESH_H_

//#include <gctypes.h>
#include <cstdint>

typedef float f32;
typedef uint16_t u16;

struct Vec3 {
	f32 x, y, z;

	void set(f32 px, f32 py, f32 pz) {
		x = px;
		y = py;
		z = pz;
	}

	/*
	Vec3(f32 px, f32 py, f32 pz) {
		set(px, py, pz);
	}
	*/
};

struct Vec2 {
	f32 x, y;

	void set(f32 px, f32 py) {
		x = px;
		y = py;
	}
	/*
	Vec2(f32 px, f32 py) {
		set(px, py);
	}
	*/
};

struct SubMesh {
	u16 start;
	u16 size;

	void set(u16 pStart, u16 pSize) {
		start = pStart;
		size = pSize;
	}
};

struct Mesh {
	u16 n_vertices;
	u16 n_tris;
	u16 n_texcoord;
	u16 n_normals;
	u16 n_subMeshes;

	Vec3* vertices;
	u16* indices;
	
	Vec2* texcoord;
	Vec3* normals;
	SubMesh* subMeshes;

	Mesh() {
		vertices = 0;
		//faces = 0;
		texcoord = 0;
		normals = 0;
		subMeshes = 0;
	}
};

#endif