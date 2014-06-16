/*!
    \file objloader.cpp
    \brief load an OBJ file and store its geometry/material in memory

    This code was adapted from the project Embree from Intel.
    Copyright 2009-2012 Intel Corporation

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
 
    Copyright 2013 Scratchapixel

    Compile with: clang++/c++ -o objloader objloader.cpp -O3 -Wall -std=c++0x
 */

#ifdef _ENTER_

#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <map>
#include <cstdint>

#if defined(__APPLE__)
#include <pthread.h>
#include <tr1/memory>

namespace std {
    using std::tr1::shared_ptr;
}
#endif

#if defined(__linux__)
#include <pthread.h>
#include <memory>
#endif

#define MAX_LINE_LENGTH 10000

template<typename T>
class Vec2
{
public:
    T x, y;
    Vec2() : x(0), y(0) {}
    Vec2(T xx, T yy) : x(xx), y(yy) {}
};

template<typename T>
class Vec3
{
public:
    T x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}
    friend std::ostream & operator << (std::ostream &os, const Vec3<T> &v)
    { os << v.x << ", " << v.y << ", " << v.z; return os; }
};

typedef Vec3<float> Vec3f;
typedef Vec3<int> Vec3i;
typedef Vec2<float> Vec2f;

Vec3f getVec3(std::ifstream &ifs) { float x, y, z; ifs >> x >> y >> z; return Vec3f(x, y, z); }

/*! returns the path of a file */
std::string getFilePath(const std::string &filename)
{
    size_t pos = filename.find_last_of('/');
    if (pos == std::string::npos) return filename;
    return filename.substr(0, pos);
}

/*! \struct Material
 *  \brief a simple structure to store material's properties
 */
struct Material
{
    Vec3f Ka, Kd, Ks;   /*! ambient, diffuse and specular rgb coefficients */
    float d;            /*! transparency */
    float Ns, Ni;       /*! specular exponent and index of refraction */
	std::string name;

	Material(std::string _name) : name(_name){};
};

/*! \class TriangleMesh
 *  \brief a basic class to store a triangle mesh data
 */
class TriangleMesh
{
public:
    Vec3f *positions;   /*! position/vertex array */
    Vec3f *normals;     /*! normal array (can be null) */
    Vec2f *texcoords;   /*! texture coordinates (can be null) */
    int numTriangles;   /*! number of triangles */
    int *triangles;     /*! triangle index list */
    int nPositions;   /*! number of vertices */
    int nNormals;   /*! number of normals*/
    int nTexCoord;   /*! number of texcoords*/
    TriangleMesh() : positions(nullptr), normals(nullptr), texcoords(nullptr), triangles(nullptr) {}
    ~TriangleMesh()
    {
        if (positions) delete [] positions;
        if (normals) delete [] normals;
        if (texcoords) delete [] texcoords;
        if (triangles) delete [] triangles;
    }
};

/*! \class Primitive
 *  \brief a basic class to store a primitive (defined by a mesh and a material)
 */
struct Primitive
{
    Primitive(const std::shared_ptr<TriangleMesh> &m, const std::shared_ptr<Material> &mat) : 
        mesh(m), material(mat) {}
    const std::shared_ptr<TriangleMesh> mesh;   /*! the object's geometry */
    const std::shared_ptr<Material> material;   /*! the object's material */
};

/*! Three-index vertex, indexing start at 0, -1 means invalid vertex. */
struct Vertex {
    int v, vt, vn;
    Vertex() {};
    Vertex(int v) : v(v), vt(v), vn(v) {};
    Vertex(int v, int vt, int vn) : v(v), vt(vt), vn(vn) {};
};

typedef std::tr1::shared_ptr<Primitive> PrimitiveSharedPtr;
typedef std::tr1::shared_ptr<TriangleMesh> MeshSharedPtr;

// need to declare this operator if we want to use Vertex in a map
static inline bool operator < ( const Vertex& a, const Vertex& b ) {
    if (a.v  != b.v)  return a.v  < b.v;
    if (a.vn != b.vn) return a.vn < b.vn;
    if (a.vt != b.vt) return a.vt < b.vt;
    return false;
}

/*! Parse separator. */
static inline const char* parseSep(const char*& token) {
    size_t sep = strspn(token, " \t");
    if (!sep) throw std::runtime_error("separator expected");
    return token+=sep;
}

/*! Read float from a string. */
static inline float getFloat(const char*& token) {
    token += strspn(token, " \t");
    float n = (float)atof(token);
    token += strcspn(token, " \t\r");
    return n;
}

/*! Read Vec2f from a string. */
static inline Vec2f getVec2f(const char*& token) {
    float x = getFloat(token);
    float y = getFloat(token);
    return Vec2f(x,y);
}

/*! Read Vec3f from a string. */
static inline Vec3f getVec3f(const char*& token) {
    float x = getFloat(token);
    float y = getFloat(token);
    float z = getFloat(token);
    return Vec3f(x, y, z);
}

/*! Parse optional separator. */
static inline const char* parseSepOpt(const char*& token) {
    return token+=strspn(token, " \t");
}

/*! Determine if character is a separator. */
static inline bool isSep(const char c) {
    return (c == ' ') || (c == '\t');
}

class ObjReader
{
public:
    ObjReader(const char *filename);
    Vertex getInt3(const char*& token);
    int fix_v(int index) { return(index > 0 ? index - 1 : (index == 0 ? 0 : (int)v .size() + index)); }
    int fix_vt(int index) { return(index > 0 ? index - 1 : (index == 0 ? 0 : (int)vt.size() + index)); }
    int fix_vn(int index) { return(index > 0 ? index - 1 : (index == 0 ? 0 : (int)vn.size() + index)); }
    std::vector<Vec3f> v, vn;
    std::vector<Vec2f> vt;
    std::vector<std::vector<Vertex> > curGroup;
    std::map<std::string, std::shared_ptr<Material> > materials;
    std::shared_ptr<Material> curMaterial;
    void loadMTL(const std::string &mtlFilename);
    void flushFaceGroup();
    uint32_t getVertex(std::map<Vertex, uint32_t>&, std::vector<Vec3f>&, std::vector<Vec3f>&, std::vector<Vec2f>&, const Vertex&);
    std::vector<std::shared_ptr<Primitive> > model;
};


/*! Parse differently formated triplets like: n0, n0/n1/n2, n0//n2, n0/n1.          */
/*! All indices are converted to C-style (from 0). Missing entries are assigned -1. */
Vertex ObjReader::getInt3(const char*& token)
{
    Vertex v(-1);
    v.v = fix_v(atoi(token));
    token += strcspn(token, "/ \t\r");
    if (token[0] != '/') return(v);
    token++;
    
    // it is i//n
    if (token[0] == '/') {
        token++;
        v.vn = fix_vn(atoi(token));
        token += strcspn(token, " \t\r");
        return(v);
    }
    
    // it is i/t/n or i/t
    v.vt = fix_vt(atoi(token));
    token += strcspn(token, "/ \t\r");
    if (token[0] != '/') return(v);
    token++;
    
    // it is i/t/n
    v.vn = fix_vn(atoi(token));
    token += strcspn(token, " \t\r");
    return(v);
}

/*! \brief load a OBJ material file
 *  \param mtlFilename is the full path to the material file
 */
void ObjReader::loadMTL(const std::string &mtlFilename)
{
    std::ifstream ifs;
    ifs.open(mtlFilename.c_str());
    if (!ifs.is_open()) {
        std::cerr << "can't open " << mtlFilename << std::endl;
        return;
    }
    std::shared_ptr<Material> mat;
    while (ifs.peek() != EOF) {
        char line[MAX_LINE_LENGTH];
        ifs.getline(line, sizeof(line), '\n');
        const char* token = line + strspn(line, " \t"); // ignore spaces and tabs
        if (token[0] == 0) continue; // ignore empty lines
        if (token[0] == '#') continue; // ignore comments

        if (!strncmp(token, "newmtl", 6)) {
            parseSep(token += 6);
            std::string name(token); printf("Name of the material %s\n", name.c_str());
            materials[name] = mat = std::shared_ptr<Material>(new Material (name));
            continue;
        }

        if (!mat) throw std::runtime_error("invalid material file: newmtl expected first");
        
        if (!strncmp(token, "d", 1)) { parseSep(token += 1); mat->d = getFloat(token); continue; }
        if (!strncmp(token, "Ns", 1)) { parseSep(token += 2); mat->Ns = getFloat(token); continue; }
        if (!strncmp(token, "Ns", 1)) { parseSep(token += 2); mat->Ni = getFloat(token); continue; }
        if (!strncmp(token, "Ka", 2)) { parseSep(token += 2); mat->Ka = getVec3f(token); continue; }
        if (!strncmp(token, "Kd", 2)) { parseSep(token += 2); mat->Kd = getVec3f(token); continue; }
        if (!strncmp(token, "Ks", 2)) { parseSep(token += 2); mat->Ks = getVec3f(token); continue; }
    }
    ifs.close();
}

/*! \brief load the geometry defined in an OBJ/Wavefront file
 *  \param filename is the path to the OJB file
 */
ObjReader::ObjReader(const char *filename)
{
    std::ifstream ifs;
    // extract the path from the filename (used to read the material file)
    std::string path = getFilePath(filename);
    try {
        ifs.open(filename);
        if (ifs.fail()) throw std::runtime_error("can't open file " + std::string(filename));

        // create a default material
        std::shared_ptr<Material> defaultMaterial(new Material("Default"));
        curMaterial = defaultMaterial;

        char line[MAX_LINE_LENGTH]; // line buffer
        
        while (ifs.peek() != EOF) // read each line until EOF is found
        {
            ifs.getline(line, sizeof(line), '\n');
            const char* token = line + strspn(line, " \t"); // ignore space and tabs

			//printf("strncmp(token, vt,  2) --> %d\n", strncmp(token, "vt",  2));
            if (token[0] == 0) continue; // line is empty, ignore
            // read a vertex
            if (token[0] == 'v' && isSep(token[1])) { v.push_back(getVec3f(token += 2)); continue; }
            // read a normal
            if (!strncmp(token, "vn",  2) && isSep(token[2])) { vn.push_back(getVec3f(token += 3)); continue; }
            // read a texture coordinates
            if (!strncmp(token, "vt",  2) && isSep(token[2])) { vt.push_back(getVec2f(token += 3)); continue; }
            //if (!strncmp(token, "vt",  2) && isSep(token[2])) { printf("vt: %.3f\n", getVec2f(token += 3).x); continue; }
            // read a face
            if (token[0] == 'f' && isSep(token[1])) {
                parseSep(token += 1);
                std::vector<Vertex> face;
                while (token[0]) {
                    face.push_back(getInt3(token));
                    parseSepOpt(token);
                }
                curGroup.push_back(face);
                continue;
            }
            
            /*! use material */
            if (!strncmp(token, "usemtl", 6) && isSep(token[6]))
            {
                flushFaceGroup();
                std::string name(parseSep(token += 6));
                if (materials.find(name) == materials.end()) curMaterial = defaultMaterial;
                else curMaterial = materials[name];
                continue;
            }
            
            /* load material library */
            if (!strncmp(token, "mtllib", 6) && isSep(token[6])) {
                loadMTL(path + "/" + std::string(parseSep(token += 6)));
                continue;
            }
        }
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    flushFaceGroup(); // flush the last loaded object
    ifs.close();
}


/*! \brief utility function to keep track of the vertex already used while creating a new mesh
 *  \param vertexMap is a map used to keep track of the vertices already inserted in the position list
 *  \param position is a position list for the newly created mesh
 *  \param normals is a normal list for the newly created mesh
 *  \param texcoords is a texture coordinate list for the newly created mesh
 *  \param i is the Vertex looked for or inserted in vertexMap
 *  \return the index of this Vertex in the position vector list.
 */
uint32_t ObjReader::getVertex(
    std::map<Vertex, uint32_t> &vertexMap, 
    std::vector<Vec3f> &positions, 
    std::vector<Vec3f> &normals,
    std::vector<Vec2f> &texcoords,
    const Vertex &i)
{
    const std::map<Vertex, uint32_t>::iterator& entry = vertexMap.find(i);
    if (entry != vertexMap.end()) return(entry->second);
    
    positions.push_back(v[i.v]);
	//printf("added vertex: (%.3f, %.3f, %.3f)\n", v[i.v].x, v[i.v].y, v[i.v].z);
    if (i.vn >= 0) normals.push_back(vn[i.vn]);
	//printf("i.vt=%d\n", i.vt);
    if (i.vt >= 0) texcoords.push_back(vt[i.vt]);
    return (vertexMap[i] = int(positions.size()) - 1);
}

/*! \brief flush the current content of currGroup and create new mesh 
 */
void ObjReader::flushFaceGroup()
{
    if (curGroup.empty()) return;
    
    // temporary data arrays
    std::vector<Vec3f> positions;
    std::vector<Vec3f> normals;
    std::vector<Vec2f> texcoords;
    std::vector<Vec3i> triangles;
    std::map<Vertex, uint32_t> vertexMap;
    
    // merge three indices into one
    for (size_t j = 0; j < curGroup.size(); j++)
    {
        /* iterate over all faces */
        const std::vector<Vertex>& face = curGroup[j];
        Vertex i0 = face[0], i1 = Vertex(-1), i2 = face[1];
        
        /* triangulate the face with a triangle fan */
        for (size_t k = 2; k < face.size(); k++) {
            i1 = i2; i2 = face[k];
            uint32_t v0 = getVertex(vertexMap, positions, normals, texcoords, i0);
            uint32_t v1 = getVertex(vertexMap, positions, normals, texcoords, i1);
            uint32_t v2 = getVertex(vertexMap, positions, normals, texcoords, i2);
            triangles.push_back(Vec3i(v0, v1, v2));
        }
    }
    curGroup.clear();

    // create new triangle mesh, allocate memory and copy data
    std::shared_ptr<TriangleMesh> mesh = std::shared_ptr<TriangleMesh>(new TriangleMesh);
    mesh->numTriangles = triangles.size();
    mesh->triangles = new int[mesh->numTriangles * 3];
	mesh->nPositions = positions.size();
	mesh->nNormals = normals.size();
	mesh->nTexCoord = texcoords.size();
    memcpy(mesh->triangles, &triangles[0], sizeof(Vec3i) * mesh->numTriangles);
    mesh->positions = new Vec3f[positions.size()];
    memcpy(mesh->positions, &positions[0], sizeof(Vec3f) * positions.size());
    if (normals.size()) {
        mesh->normals = new Vec3f[normals.size()];
        memcpy(mesh->normals, &normals[0], sizeof(Vec3f) * normals.size());
    }
	//printf("texcoords.size()= %d\n", texcoords.size());
    if (texcoords.size()) {
        mesh->texcoords = new Vec2f[texcoords.size()];
        memcpy(mesh->texcoords, &texcoords[0], sizeof(Vec2f) * texcoords.size());
    }
    model.push_back(std::shared_ptr<Primitive>(new Primitive(mesh, curMaterial)));
}

void conv_obj(const std::vector<std::shared_ptr<Primitive> >& model, const char* filename);

#endif