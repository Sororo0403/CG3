#define NOMINMAX
#include "TileMap3D.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>

static inline unsigned RGBAu32(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (unsigned(r) << 24) | (unsigned(g) << 16) | (unsigned(b) << 8) | unsigned(a);
}

// ====== 属性 ======
bool TileMap3D::IsFragile(Tile t) {
    return t == Tile::FragileAny || t == Tile::FragileTop || t == Tile::FragileBottom || t == Tile::Regen;
}
bool TileMap3D::IsSpring(Tile t) { return t == Tile::Spring; }
bool TileMap3D::IsSolidLikeBase(Tile t, bool switchOn) {
    if (t == Tile::SwitchBlockOn)  return switchOn;
    if (t == Tile::SwitchBlockOff) return !switchOn;
    if (t == Tile::JumpOnly)       return true;
    return t == Tile::Solid;
}

bool TileMap3D::IsBlockingAt(int tx, int ty) const {
    if (!InMap(tx, ty)) return false;
    Tile t = grid_[ty][tx];
    if (IsFragile(t)) return !fragile_[ty][tx].gone;
    return IsSolidLikeBase(t, switchOn_);
}

void TileMap3D::ResetGrid() {
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            grid_[y][x] = Tile::Empty;
            fragile_[y][x] = FragileState{};
            regen_[y][x] = RegenState{};
        }
    }
    switchOn_ = false;
    spawnTx_ = 2; spawnTy_ = 2;
}

void TileMap3D::BuildSample() {
    ResetGrid();
    for (int x = 0; x < kMapW; ++x) grid_[kMapH - 2][x] = Tile::Solid;
    for (int x = 3; x <= 8; ++x)    grid_[kMapH - 5][x] = Tile::FragileAny;
    for (int x = 11; x <= 14; ++x)  grid_[kMapH - 7][x] = Tile::FragileTop;
    for (int x = 16; x <= 19; ++x)  grid_[kMapH - 9][x] = Tile::FragileBottom;
    grid_[kMapH - 3][6] = Tile::Spring;
    grid_[kMapH - 6][18] = Tile::Switch;
    grid_[kMapH - 6][20] = Tile::SwitchBlockOn;
    grid_[kMapH - 6][21] = Tile::SwitchBlockOn;
    grid_[kMapH - 6][23] = Tile::SwitchBlockOff;
    for (int x = kMapW - 8; x < kMapW - 2; ++x) grid_[kMapH - 3][x] = Tile::Spike;
    grid_[kMapH - 8][22] = Tile::Regen;
}

static std::string NowYYYYMMDD_HHMMSS() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

bool TileMap3D::SaveCSV(const std::string &path) const {
    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << kMapW << "," << kMapH << "," << spawnTx_ << "," << spawnTy_ << "\n";
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            ofs << (int)grid_[y][x];
            if (x + 1 < kMapW) ofs << ",";
        }
        ofs << "\n";
    }
    return true;
}

bool TileMap3D::LoadCSV(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    ResetGrid();

    std::string line;
    if (!std::getline(ifs, line)) return false;
    {   // header: w,h,spawnTx,spawnTy
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string token; std::vector<int> vals;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                try { vals.push_back(std::stoi(token)); }
                catch (...) { vals.push_back(0); }
            } else vals.push_back(0);
        }
        if (vals.size() >= 4) {
            spawnTx_ = std::clamp(vals[2], 0, kMapW - 1);
            spawnTy_ = std::clamp(vals[3], 0, kMapH - 1);
        }
    }
    int y = 0;
    while (y < kMapH && std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string cell; int x = 0;
        while (x < kMapW && std::getline(ss, cell, ',')) {
            int id = 0; if (!cell.empty()) { try { id = std::stoi(cell); } catch (...) { id = 0; } }
            id = std::clamp(id, 0, (int)Tile::SwitchBlockOff);
            Tile t = (Tile)id;
            grid_[y][x] = t;
            if (IsFragile(t)) fragile_[y][x] = FragileState{};
            if (t == Tile::Regen) regen_[y][x] = RegenState{};
            ++x;
        }
        ++y;
    }
    ClampSpawnToSafe();
    return true;
}

bool TileMap3D::CreateSnapshot(const std::string &baseCsvPath) const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path base(baseCsvPath);
    fs::path bakDir = base.parent_path() / "backups";
    fs::create_directories(bakDir, ec);
    fs::path dst = bakDir / (base.stem().string() + "_" + NowYYYYMMDD_HHMMSS() + base.extension().string());
    return SaveCSV(dst.string());
}

void TileMap3D::SetSpawnTile(int tx, int ty) {
    spawnTx_ = std::clamp(tx, 0, kMapW - 1);
    spawnTy_ = std::clamp(ty, 0, kMapH - 1);
}

void TileMap3D::ClampSpawnToSafe() {
    int tx = std::clamp(spawnTx_, 0, kMapW - 1);
    int ty = std::clamp(spawnTy_, 0, kMapH - 1);
    if (IsBlockingAt(tx, ty)) {
        for (int y = ty - 1; y >= 0; --y) { if (!IsBlockingAt(tx, y)) { ty = y; break; } }
    }
    spawnTx_ = tx; spawnTy_ = ty;
}

void TileMap3D::SetTile(int tx, int ty, Tile t) {
    if (!InMap(tx, ty)) return;
    grid_[ty][tx] = t;
    if (IsFragile(t)) fragile_[ty][tx] = FragileState{};
    if (t == Tile::Regen) regen_[ty][tx] = RegenState{};
}

int TileMap3D::CountRemainingFragile() const {
    int c = 0;
    for (int y = 0; y < kMapH; ++y)
        for (int x = 0; x < kMapW; ++x)
            if (IsFragile(grid_[y][x]) && !fragile_[y][x].gone) ++c;
    return c;
}

void TileMap3D::StepStates(float dt) {
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            Tile t = grid_[y][x];
            if (!IsFragile(t)) continue;
            auto &fs = fragile_[y][x];
            if (!fs.gone) {
                if (fs.armed) {
                    fs.t += dt;
                    const float kFragileBreakTime = 1.35f;
                    if (fs.t > kFragileBreakTime) {
                        fs.gone = true;
                        if (t == Tile::Regen) regen_[y][x].respawn = 0.0f;
                    }
                }
            } else if (t == Tile::Regen) {
                regen_[y][x].respawn += dt;
                const float kRegenRespawnTime = 2.0f;
                if (regen_[y][x].respawn >= kRegenRespawnTime) {
                    fs = FragileState{};
                }
            }
        }
    }
}

unsigned TileMap3D::TileRGBA(Tile t, bool switchOn, bool blink) {
    switch (t) {
    case Tile::Solid:          return RGBAu32(70, 70, 90, 255);
    case Tile::FragileAny:     return blink ? RGBAu32(255, 230, 80, 140) : RGBAu32(255, 210, 50, 255);
    case Tile::FragileTop:     return blink ? RGBAu32(255, 170, 90, 140) : RGBAu32(255, 140, 60, 255);
    case Tile::FragileBottom:  return blink ? RGBAu32(255, 140, 190, 140) : RGBAu32(255, 100, 160, 255);
    case Tile::Spring:         return RGBAu32(120, 255, 120, 255);
    case Tile::Spike:          return RGBAu32(230, 70, 90, 255);
    case Tile::JumpOnly:       return RGBAu32(120, 210, 255, 255);
    case Tile::Regen:          return blink ? RGBAu32(210, 150, 255, 140) : RGBAu32(200, 120, 255, 255);
    case Tile::Switch:         return switchOn ? RGBAu32(120, 170, 255, 255) : RGBAu32(90, 120, 200, 255);
    case Tile::SwitchBlockOn:  return switchOn ? RGBAu32(140, 190, 255, 255) : RGBAu32(70, 90, 130, 120);
    case Tile::SwitchBlockOff: return !switchOn ? RGBAu32(140, 190, 255, 255) : RGBAu32(70, 90, 130, 120);
    default: return RGBAu32(0, 0, 0, 0);
    }
}

// ...中略...
void TileMap3D::Draw(ModelRenderer *renderer, ID3D12GraphicsCommandList *cmd, const Model &cubeModel) const {
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            Tile t = grid_[y][x];
            if (t == Tile::Empty) continue;
            if (IsFragile(t) && fragile_[y][x].gone) {
                if (t != Tile::Regen) continue;
            }

            Transform tr{};
            tr.pos = {xOffset_ + x * kTile, 0.0f, y * kTile}; // XZに敷設
            tr.scale = {1.0f, 1.0f, 1.0f};
            renderer->Draw(cmd, cubeModel, tr);
        }
    }
}
