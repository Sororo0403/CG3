#define NOMINMAX
#include "Model.h"
#include "Vertex.h"
#include "VertexRef.h"

#include <DirectXMath.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace DirectX;

// OBJのインデックス (1-based / -1 from end) を0スタートに直す
static inline int ResolveIndex(int idx, int count) {
	if (idx > 0) return idx - 1;
	if (idx < 0) return count + idx;
	return -1;
}

// "v/t/n" / "v//n" / "v/t" / "v"
static inline VertexRef ParseVertexRef(const std::string &token) {
	VertexRef r{}; r.v = 0; r.vt = -1; r.vn = -1;
	int stage = 0; std::string a, b, c;
	for (char ch : token) {
		if (ch == '/') { ++stage; continue; }
		if (stage == 0) a.push_back(ch);
		else if (stage == 1) b.push_back(ch);
		else                 c.push_back(ch);
	}
	if (!a.empty()) r.v = std::stoi(a);
	if (!b.empty()) r.vt = std::stoi(b);
	if (!c.empty()) r.vn = std::stoi(c);
	return r;
}

// 三角面の法線
static inline XMFLOAT3 FaceNormal(const XMFLOAT3 &p0, const XMFLOAT3 &p1, const XMFLOAT3 &p2) {
	XMVECTOR P0 = XMLoadFloat3(&p0);
	XMVECTOR P1 = XMLoadFloat3(&p1);
	XMVECTOR P2 = XMLoadFloat3(&p2);
	XMVECTOR e1 = XMVectorSubtract(P1, P0);
	XMVECTOR e2 = XMVectorSubtract(P2, P0);
	XMVECTOR n = XMVector3Normalize(XMVector3Cross(e1, e2));
	XMFLOAT3 out{}; XMStoreFloat3(&out, n);
	return out;
}

// 簡易MTL
struct MtlMat {
	XMFLOAT3 Kd{1,1,1};
	float d = 1.0f;
	std::string map_Kd;
};

static std::unordered_map<std::string, MtlMat> LoadMtlFile_(const std::string &mtlPath) {
	std::unordered_map<std::string, MtlMat> mats;
	std::ifstream ifs(mtlPath);
	if (!ifs) return mats;

	std::string line, cur;
	while (std::getline(ifs, line)) {
		if (line.empty() || line[0] == '#') continue;
		std::istringstream iss(line);
		std::string tag; iss >> tag;
		if (tag == "newmtl") {
			iss >> cur;
			mats[cur] = MtlMat{};
		} else if (tag == "Kd" && !cur.empty()) {
			float r, g, b; iss >> r >> g >> b;
			mats[cur].Kd = {r,g,b};
		} else if ((tag == "d" || tag == "Tr") && !cur.empty()) {
			float v; iss >> v;
			mats[cur].d = (tag == "Tr") ? (1.0f - v) : v;
		} else if (tag == "map_Kd" && !cur.empty()) {
			std::string tex;
			std::getline(iss, tex);

			// 先頭の空白/タブをstrip
			while (!tex.empty() && (tex[0] == ' ' || tex[0] == '\t')) {
				tex.erase(tex.begin());
			}
			// 末尾の空白/タブ/CR/LFをstrip
			while (!tex.empty() && (tex.back() == ' ' || tex.back() == '\t' ||
				tex.back() == '\r' || tex.back() == '\n')) {
				tex.pop_back();
			}

			mats[cur].map_Kd = tex;
		}

	}
	return mats;
}

void Model::Initialize(ID3D12Device *device, const std::string &path) {
	assert(device);
	assert(!path.empty() && "Model::Initialize: OBJ path is required (non-empty).");
	LoadFromOBJ_(device, path);
}

void Model::LoadFromOBJ_(ID3D12Device *device, const std::string &path) {
	std::ifstream ifs(path);
	assert(ifs && "Failed to open OBJ file. Check path.");

	// OBJ のディレクトリ（mtllib / map_Kd用）
	const auto slash = path.find_last_of("/\\");
	const std::string dir = (slash == std::string::npos) ? "" : path.substr(0, slash + 1);

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;
	std::vector<XMFLOAT2> texcoords;

	std::vector<Vertex>   outVertices;
	std::vector<uint32_t> outIndices;
	outVertices.reserve(1024);
	outIndices.reserve(2048);

	std::unordered_map<VertexRef, uint32_t, VertexRefHash> dedup;

	std::unordered_map<std::string, MtlMat> materials;
	std::string currentMtl;

	std::string line;
	while (std::getline(ifs, line)) {
		if (line.empty() || line[0] == '#') continue;
		std::istringstream iss(line);
		std::string tag; iss >> tag;

		if (tag == "v") {
			XMFLOAT3 p{}; iss >> p.x >> p.y >> p.z; positions.push_back(p);
		} else if (tag == "vt") {
			XMFLOAT2 t{}; iss >> t.x >> t.y; texcoords.push_back(t);
		} else if (tag == "vn") {
			XMFLOAT3 n{}; iss >> n.x >> n.y >> n.z; normals.push_back(n);
		} else if (tag == "mtllib") {
			std::string mtlFile; iss >> mtlFile;
			auto m = LoadMtlFile_(dir + mtlFile);
			materials.insert(m.begin(), m.end()); // マージ
		} else if (tag == "usemtl") {
			iss >> currentMtl;

			// 初回の map_Kd をモデルに記憶
			if (albedoPath_.empty()) {
				auto it = materials.find(currentMtl);
				if (it != materials.end() && !it->second.map_Kd.empty()) {
					albedoPath_ = dir + it->second.map_Kd;
				}
			}
		} else if (tag == "f") {
			std::vector<VertexRef> refs;
			std::string tok;
			while (iss >> tok) refs.push_back(ParseVertexRef(tok));
			if (refs.size() < 3) continue;

			MtlMat mat{};
			if (!currentMtl.empty()) {
				auto it = materials.find(currentMtl);
				if (it != materials.end()) mat = it->second;
			}

			// 多角形を扇分割
			for (size_t k = 1; k + 1 < refs.size(); ++k) {
				VertexRef tri[3] = {refs[0], refs[k], refs[k + 1]};

				int vi[3], ti[3], ni[3];
				XMFLOAT3 P[3]; XMFLOAT2 T[3]; XMFLOAT3 N[3];
				bool hasT[3] = {}, hasN[3] = {};

				for (int m = 0; m < 3; ++m) {
					vi[m] = ResolveIndex(tri[m].v, (int)positions.size());
					ti[m] = (tri[m].vt == -1) ? -1 : ResolveIndex(tri[m].vt, (int)texcoords.size());
					ni[m] = (tri[m].vn == -1) ? -1 : ResolveIndex(tri[m].vn, (int)normals.size());

					assert(vi[m] >= 0 && vi[m] < (int)positions.size());
					P[m] = positions[(size_t)vi[m]];

					if (ti[m] >= 0) { T[m] = texcoords[(size_t)ti[m]]; hasT[m] = true; } else { T[m] = XMFLOAT2{0,0}; }
					if (ni[m] >= 0) { N[m] = normals[(size_t)ni[m]]; hasN[m] = true; } else { N[m] = XMFLOAT3{0,0,0}; }
				}

				// 法線なければ面法線
				if (!(hasN[0] && hasN[1] && hasN[2])) {
					const XMFLOAT3 fn = FaceNormal(P[0], P[1], P[2]);
					N[0] = N[1] = N[2] = fn;
					ni[0] = ni[1] = ni[2] = -1;
				}

				for (int m = 0; m < 3; ++m) {
					VertexRef key{vi[m], hasT[m] ? ti[m] : -1, ni[m]};
					auto it = dedup.find(key);
					uint32_t idx;
					if (it == dedup.end()) {
						Vertex v{};
						v.pos = P[m];
						v.nrm = N[m];
						XMFLOAT2 uvFixed = T[m];
						uvFixed.y = 1.0f - uvFixed.y;
						v.uv = uvFixed;

						v.color = {mat.Kd.x, mat.Kd.y, mat.Kd.z, mat.d}; // Kd/dを頂点カラーに焼く
						idx = (uint32_t)outVertices.size();
						outVertices.push_back(v);
						dedup.emplace(key, idx);
					} else {
						idx = it->second;
					}
					outIndices.push_back(idx);
				}
			}
		}
	}

	assert(!outVertices.empty() && !outIndices.empty() && "OBJ contained no geometry");
	mesh_.CreateFromVertices(device, outVertices, outIndices);
}
