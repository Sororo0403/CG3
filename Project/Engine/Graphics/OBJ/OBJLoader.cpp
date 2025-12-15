#include "ObjLoader.h"

#include "Vector2.h"
#include "Vector3.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>

struct OBJIndex {
    int v;
    int vt;
    int vn;

    bool operator==(const OBJIndex &other) const {
        return v == other.v && vt == other.vt && vn == other.vn;
    }
};

namespace std {
template <> struct hash<OBJIndex> {
    size_t operator()(const OBJIndex &k) const {
        size_t h = 0;
        h ^= std::hash<int>()(k.v) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.vt) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.vn) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
} // namespace std

std::unique_ptr<Mesh> OBJLoader::Load(const std::string &path) {
    std::ifstream file(path);
    assert(file && "Failed to open OBJ file");

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> uvs;

    auto mesh = std::make_unique<Mesh>();

    std::unordered_map<OBJIndex, uint32_t> indexMap;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        // ------------------------
        // Vertex position
        // ------------------------
        if (tag == "v") {
            Vector3 p{};
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        // ------------------------
        // Texcoord
        // ------------------------
        else if (tag == "vt") {
            Vector2 uv{};
            ss >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y; // OBJ は上下逆
            uvs.push_back(uv);
        }
        // ------------------------
        // Normal
        // ------------------------
        else if (tag == "vn") {
            Vector3 n{};
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        // ------------------------
        // Face
        // ------------------------
        else if (tag == "f") {
            std::vector<OBJIndex> face;

            std::string vtx;
            while (ss >> vtx) {
                OBJIndex idx{};

                std::stringstream vs(vtx);
                std::string token;

                std::getline(vs, token, '/');
                idx.v = std::stoi(token) - 1;

                if (std::getline(vs, token, '/') && !token.empty()) {
                    idx.vt = std::stoi(token) - 1;
                } else {
                    idx.vt = -1;
                }

                if (std::getline(vs, token, '/') && !token.empty()) {
                    idx.vn = std::stoi(token) - 1;
                } else {
                    idx.vn = -1;
                }

                face.push_back(idx);
            }

            // 三角形化（fan）
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                OBJIndex tri[3] = {face[0], face[i], face[i + 1]};

                for (int k = 0; k < 3; ++k) {
                    const OBJIndex &oi = tri[k];

                    auto it = indexMap.find(oi);
                    if (it != indexMap.end()) {
                        mesh->indices.push_back(it->second);
                        continue;
                    }

                    Mesh::Vertex v{};
                    v.position = positions[oi.v];
                    v.normal = (oi.vn >= 0) ? normals[oi.vn] : Vector3{0, 1, 0};
                    v.uv = (oi.vt >= 0) ? uvs[oi.vt] : Vector2{0, 0};

                    uint32_t newIndex =
                        static_cast<uint32_t>(mesh->vertices.size());

                    mesh->vertices.push_back(v);
                    mesh->indices.push_back(newIndex);
                    indexMap[oi] = newIndex;
                }
            }
        }
    }

    assert(!mesh->vertices.empty());
    assert(!mesh->indices.empty());

    return mesh;
}
