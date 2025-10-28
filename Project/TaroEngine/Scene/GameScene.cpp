#define NOMINMAX
#include "GameScene.h"

#include "Input.h"
#include "ModelRenderer.h"
#include "BufferUtility.h"
#include "SceneManager.h"
#include "clearScene.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <DirectXTex/d3dx12.h>
#include <DirectXTex/DirectXTex.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;
namespace {
	constexpr float kBlockDepth = 0.5f;
	constexpr float kPlayerDepth = 0.6f;
	constexpr float kPlayerZ = -0.26f; // -Z が手前

	constexpr float kFragileBreakTime = 1.35f; // armed→完全に消滅
	constexpr float kRegenRespawnTime = 2.0f;  // Regenが戻るまで
	constexpr float kFragileBlinkStart = 0.6f;  // 壊れる0.6秒前からフェード警告
	constexpr float kFragileBlinkFreq = 4.0f;  // Hz 揺らしの速さ
}

float GameScene::FragileBlinkFactor_(int tx, int ty) const {
	if (!InMap(tx, ty)) {
		return 1.0f;
	}

	Tile t = grid_[ty][tx];
	if (!IsFragile(t)) {
		return 1.0f;
	}

	const FragileState &fs = frag_[ty][tx];

	// もう消えてるなら描画しない or 透明扱い
	if (fs.gone) {
		return 0.0f; // 透明扱い
	}

	// まだarmedじゃない → 通常表示(点滅なし)
	if (!fs.armed) {
		return 1.0f;
	}

	// ここから「armed中は常に点滅」

	float elapsed = fs.t;
	float remaining = kFragileBreakTime - elapsed;
	if (remaining < 0.0f) remaining = 0.0f;

	// どれくらい崩壊に近いか。0.0 = 触れた直後 / 1.0 = ほぼ崩壊
	float danger = std::clamp(elapsed / kFragileBreakTime, 0.0f, 1.0f);

	// 点滅の周期は今まで通り
	float period = 1.0f / kFragileBlinkFreq;        // 秒
	float cyclePos = std::fmodf(elapsed, period);   // [0,period)
	float t01 = cyclePos / period;                  // [0,1)

	// 0→1→0 の滑らかな波 (cosベース)
	float wave = 0.5f * (1.0f - std::cos(t01 * 2.0f * 3.14159265f));
	// wave=0   → 0
	// wave=0.5 → 1
	// wave=1   → 0

	// dangerが高いほど、最低アルファを0.25まで下げる
	//  elapsed=0   → danger≈0   → minAlpha ≈1.0 (ほぼ通常)
	//  elapsed=1.35→ danger≈1.0 → minAlpha ≈0.25 (かなり消えかけ)
	float minAlpha = 1.0f - danger * 0.75f; // 1.0→0.25

	// waveで 1.0..minAlpha..1.0 を往復
	float alphaMul = 1.0f + (minAlpha - 1.0f) * wave;

	// clamp
	if (alphaMul < 0.0f) alphaMul = 0.0f;
	if (alphaMul > 1.0f) alphaMul = 1.0f;
	return alphaMul;
}



// ----------------- UTF-8 → UTF-16 -----------------
std::wstring GameScene::Widen_(const std::string &u8) {
	if (u8.empty()) return L"";
	int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
	std::wstring w(wlen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), wlen);
	if (!w.empty() && w.back() == L'\0') w.pop_back();
	return w;
}


// ----------------- 入力立ち上がり -----------------
bool GameScene::KeyPressed_(uint8_t dik) {
	auto *in = engineContext_->input;
	bool now = in->IsKeyPressed(dik);
	bool was = keyPrev_[dik] != 0;
	keyPrev_[dik] = now ? 1 : 0;
	return now && !was;
}

// ----------------- 属性 -----------------
bool GameScene::IsFragile(Tile t) {
	return t == Tile::FragileAny
		|| t == Tile::FragileTop
		|| t == Tile::FragileBottom
		|| t == Tile::Regen;
}
bool GameScene::IsSpring(Tile t) {
	return t == Tile::Spring;
}

// 「ブロックとしてぶつかるか？」
bool GameScene::IsBlockingAt(int tx, int ty) const {
	if (!InMap(tx, ty)) return false;
	Tile t = grid_[ty][tx];

	if (IsFragile(t)) {                     // fragile
		if (frag_[ty][tx].gone) return false;
		return true;
	}
	if (t == Tile::SwitchBlockOn)  return switchOn_;
	if (t == Tile::SwitchBlockOff) return !switchOn_;
	if (t == Tile::JumpOnly)       return true;   // JumpOnly 足場
	if (t == Tile::Solid)          return true;   // Solid
	return false;
}

// ----------------- マップ初期化 -----------------
void GameScene::ResetGrid() {
	for (int y = 0; y < kMapH; ++y) {
		for (int x = 0; x < kMapW; ++x) {
			grid_[y][x] = Tile::Empty;
			frag_[y][x] = FragileState{};
			regen_[y][x] = RegenState{};
		}
	}
	switchOn_ = false;
	spawnTx_ = 2;
	spawnTy_ = 2;
}

// ----------------- サンプルマップ（CSV無いとき用） -----------------
void GameScene::BuildSample() {
	ResetGrid();

	// 地面
	for (int x = 0; x < kMapW; ++x) {
		grid_[kMapH - 2][x] = Tile::Solid;
	}

	// 壊れる床
	for (int x = 3; x <= 8; ++x)   grid_[kMapH - 5][x] = Tile::FragileAny;
	for (int x = 11; x <= 14; ++x) grid_[kMapH - 7][x] = Tile::FragileTop;
	for (int x = 16; x <= 19; ++x) grid_[kMapH - 9][x] = Tile::FragileBottom;

	// ギミック
	grid_[kMapH - 3][6] = Tile::Spring;
	grid_[kMapH - 6][18] = Tile::Switch;
	grid_[kMapH - 6][20] = Tile::SwitchBlockOn;
	grid_[kMapH - 6][21] = Tile::SwitchBlockOn;
	grid_[kMapH - 6][23] = Tile::SwitchBlockOff;

	// スパイク
	for (int x = kMapW - 8; x < kMapW - 2; ++x) {
		grid_[kMapH - 3][x] = Tile::Spike;
	}

	// 復活床
	grid_[kMapH - 8][22] = Tile::Regen;
}

// ----------------- CSV保存・読み込み -----------------
bool GameScene::SaveCSV(const std::string &path) const {
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

bool GameScene::LoadCSV(const std::string &path) {
	std::ifstream ifs(path);
	if (!ifs) return false;

	ResetGrid();

	std::string line;
	if (!std::getline(ifs, line)) return false;

	// 1行目: "W,H,spawnTx,spawnTy"
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::istringstream ssHeader(line);

		std::string tok;
		std::vector<int> vals;
		while (std::getline(ssHeader, tok, ',')) {
			int v = 0;
			if (!tok.empty()) {
				try { v = std::stoi(tok); }
				catch (...) { v = 0; }
			}
			vals.push_back(v);
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

		std::string cell;
		int x = 0;
		while (x < kMapW && std::getline(ss, cell, ',')) {
			int id = 0;
			if (!cell.empty()) {
				try { id = std::stoi(cell); }
				catch (...) { id = 0; }
			}
			id = std::clamp(id, 0, (int)Tile::SwitchBlockOff);
			Tile t = (Tile)id;
			grid_[y][x] = t;
			if (IsFragile(t)) frag_[y][x] = FragileState{};
			if (t == Tile::Regen) regen_[y][x] = RegenState{};
			++x;
		}
		++y;
	}

	ClampSpawnToSafe();
	return true;
}

// ----------------- スポーン位置がブロックの中にいないか補正 -----------------
void GameScene::ClampSpawnToSafe() {
	int tx = std::clamp(spawnTx_, 0, kMapW - 1);
	int ty = std::clamp(spawnTy_, 0, kMapH - 1);
	if (IsBlockingAt(tx, ty)) {
		for (int r = ty - 1; r >= 0; --r) { // 上方向に安全地帯を探す
			if (!IsBlockingAt(tx, r)) { ty = r; break; }
		}
	}
	spawnTx_ = tx;
	spawnTy_ = ty;
}

// ----------------- テクスチャ読み込み + SRV作成 -----------------
bool GameScene::LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
	ComPtr<ID3D12Resource> &outTex,
	D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle) {

	auto *dx = engineContext_->directXCommon;
	ID3D12Device *device = dx->GetDevice();
	if (!device || fileU16.empty()) return false;

	DirectX::ScratchImage img, conv;
	HRESULT hr = DirectX::LoadFromWICFile(
		fileU16.c_str(),
		DirectX::WIC_FLAGS_FORCE_SRGB,
		nullptr,
		img
	);
	if (FAILED(hr)) return false;

	const DirectX::TexMetadata &meta = img.GetMetadata();
	DXGI_FORMAT targetFmt = meta.format;

	if (!DirectX::IsCompressed(meta.format) && !DirectX::IsSRGB(meta.format)) {
		hr = DirectX::Convert(
			img.GetImages(), img.GetImageCount(), meta,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			conv
		);
		if (FAILED(hr)) return false;
	}

	const DirectX::Image *srcImgs = conv.GetImages() ? conv.GetImages() : img.GetImages();
	DirectX::TexMetadata useMeta = conv.GetMetadata().width ? conv.GetMetadata() : meta;
	targetFmt = useMeta.format;

	D3D12_RESOURCE_DESC texDesc{};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = static_cast<UINT>(useMeta.width);
	texDesc.Height = static_cast<UINT>(useMeta.height);
	texDesc.DepthOrArraySize = static_cast<UINT16>(useMeta.arraySize);
	texDesc.MipLevels = static_cast<UINT16>(useMeta.mipLevels ? useMeta.mipLevels : 1);
	texDesc.Format = targetFmt;
	texDesc.SampleDesc = {1, 0};
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES heapDef{};
	heapDef.Type = D3D12_HEAP_TYPE_DEFAULT;

	hr = device->CreateCommittedResource(
		&heapDef,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&outTex));
	if (FAILED(hr)) return false;

	UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);
	ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

	// ワンショットコマンド
	ComPtr<ID3D12CommandQueue> queue;
	{
		D3D12_COMMAND_QUEUE_DESC qd{};
		qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
	}
	ComPtr<ID3D12CommandAllocator> alloc;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));

	ComPtr<ID3D12GraphicsCommandList> list;
	device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		alloc.Get(),
		nullptr,
		IID_PPV_ARGS(&list)
	);

	{
		std::vector<D3D12_SUBRESOURCE_DATA> subs(static_cast<size_t>(useMeta.mipLevels));
		for (size_t m = 0; m < useMeta.mipLevels; ++m) {
			const DirectX::Image &im = srcImgs[m];
			subs[m].pData = im.pixels;
			subs[m].RowPitch = im.rowPitch;
			subs[m].SlicePitch = im.slicePitch;
		}

		UpdateSubresources(
			list.Get(), outTex.Get(), upload.Get(),
			0, 0, static_cast<UINT>(useMeta.mipLevels),
			subs.data()
		);

		auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
			outTex.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		list->ResourceBarrier(1, &toSRV);
	}

	list->Close();
	ID3D12CommandList *lists[] = {list.Get()};
	queue->ExecuteCommandLists(1, lists);

	ComPtr<ID3D12Fence> fence;
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, evt);
		WaitForSingleObject(evt, INFINITE);
	}
	CloseHandle(evt);

	// SRV
	ID3D12DescriptorHeap *srvHeap = engineContext_->directXCommon->GetSrvHeap();
	const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
	cpu.ptr += SIZE_T(inc) * srvIndex;

	D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
	gpu.ptr += UINT64(inc) * srvIndex;
	outGpuHandle = gpu;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.Format = targetFmt;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Texture2D.MostDetailedMip = 0;
	srv.Texture2D.MipLevels = texDesc.MipLevels;

	device->CreateShaderResourceView(outTex.Get(), &srv, cpu);
	return true;
}

// ----------------- Initialize -----------------
void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
	engineContext_ = engineContext;
	renderContext_ = renderContext;
	sceneManager_ = engineContext_->sceneManager;

	cleared_ = false;
	elapsedTime_ = 0.0f;
	finalTime_ = 0.0f;

	auto *dx = engineContext_->directXCommon;
	ID3D12Device *device = dx->GetDevice();

	// モデルロード
	playerModel_.Initialize(device, "Resources/Model/Player/player.obj");
	mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
	mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
	mdlFragileTop_.Initialize(device, "Resources/Model/Block/fragile_top.obj");
	mdlFragileBottom_.Initialize(device, "Resources/Model/Block/fragile_bottom.obj");
	mdlRegen_.Initialize(device, "Resources/Model/Block/regen.obj");
	mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
	mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");
	mdlSwitch_.Initialize(device, "Resources/Model/Block/switch.obj");
	mdlSwitchBlockOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
	mdlSwitchBlockOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");
	mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

	// 各モデルのアルベドテクスチャをSRVに登録
	auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
		if (!m.GetAlbedoPath().empty()) {
			D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
			if (LoadTextureSRV_(Widen_(m.GetAlbedoPath()), slot, holder, gpu)) {
				m.SetAlbedoSRV(gpu);
			}
		}
		};
	setupTex(playerModel_, kSrvIndex_Player, texPlayer_);
	setupTex(mdlSolid_, kSrvIndex_Solid, texSolid_);
	setupTex(mdlFragileAny_, kSrvIndex_FragileAny, texFragileAny_);
	setupTex(mdlFragileTop_, kSrvIndex_FragileTop, texFragileTop_);
	setupTex(mdlFragileBottom_, kSrvIndex_FragileBottom, texFragileBottom_);
	setupTex(mdlRegen_, kSrvIndex_Regen, texRegen_);
	setupTex(mdlSpring_, kSrvIndex_Spring, texSpring_);
	setupTex(mdlSpike_, kSrvIndex_Spike, texSpike_);
	setupTex(mdlSwitch_, kSrvIndex_Switch, texSwitch_);
	setupTex(mdlSwitchBlockOn_, kSrvIndex_SwitchOn, texSwitchOn_);
	setupTex(mdlSwitchBlockOff_, kSrvIndex_SwitchOff, texSwitchOff_);
	setupTex(mdlJumpOnly_, kSrvIndex_JumpOnly, texJumpOnly_);

	// マップオフセット（中央寄せ）
	const float mapW = kMapW * kTile;
	xOffset_ = -mapW * 0.5f;

	// ステージCSVロード
	std::string stagePath;
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "stage%02d.csv", stageId_);
		stagePath = buf;
	}
	if (!LoadCSV(stagePath)) {
		if (!LoadCSV("stage.csv")) {
			BuildSample();
			(void)SaveCSV(stagePath); // 最初の一枚を出力しておく
		}
	}
	ClampSpawnToSafe();

	// プレイヤ初期配置
	playerTr_ = {};
	playerTr_.scale = {1,1,1};
	playerTr_.pos = {
		xOffset_ + spawnTx_ * kTile,
		TyToWorldY(spawnTy_) + 0.5f,
		kPlayerZ
	};

	vel_ = {0,0,0};
	onGround_ = false;

	std::memset(keyPrev_, 0, sizeof(keyPrev_));
	coyoteCounter_ = 0;
	jumpBuffer_ = 0;

	// カメラをマップ全体が見える正射影にセット
	{
		const float worldW = kMapW * kTile;
		const float worldH = kMapH * kTile;

		float screenW = (float)dx->GetWidth();
		float screenH = (float)dx->GetHeight();
		float screenAspect = screenW / std::max(1.0f, screenH);
		float mapAspect = worldW / worldH;

		float orthoW, orthoH;
		if (screenAspect >= mapAspect) {
			orthoH = worldH;
			orthoW = worldH * screenAspect;
		} else {
			orthoW = worldW;
			orthoH = worldW / screenAspect;
		}

		const float centerX = 0.0f;
		const float centerY = worldH * 0.5f;
		const float camZ = -50.0f;

		camera_.Initialize(
			{centerX, centerY, camZ},
			{0.0f,    0.0f,    0.0f},
			60.0f,
			screenAspect,
			0.1f,
			1000.0f
		);
		camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
		camera_.LookAt(
			{centerX, centerY, camZ},
			{centerX, centerY, 0.0f}
		);
		camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
	}

	// ステージ初期スナップを保存（死亡時リセット用）
	for (int y = 0; y < kMapH; ++y) {
		for (int x = 0; x < kMapW; ++x) {
			initialGrid_[y][x] = grid_[y][x];
			initialFrag_[y][x] = frag_[y][x];
			initialRegen_[y][x] = regen_[y][x];
		}
	}
	initialSwitchOn_ = switchOn_;
	initialSpawnTx_ = spawnTx_;
	initialSpawnTy_ = spawnTy_;
}

// ----------------- 物理解決：横 -----------------
void GameScene::ResolveHorizontal_() {
	if (vel_.x == 0.0f) return;

	float startX = playerTr_.pos.x;
	float targetX = startX + vel_.x;

	AABB boxNow = PlayerAabbFull_();

	const float minY = boxNow.y + kSkinY;
	const float maxY = boxNow.y + boxNow.h - 1e-4f;

	int tyBottom = ToTy(minY);
	int tyTop = ToTy(maxY);
	int tyMin = std::min(tyBottom, tyTop);
	int tyMax = std::max(tyBottom, tyTop);

	bool  collided = false;
	float stopX = targetX;

	if (vel_.x > 0.0f) {
		// 右方向
		float startRight = startX + pw_;
		float endRight = targetX + pw_;

		int colStart = ToTx(startRight - 1e-4f);
		int colEnd = ToTx(endRight - 1e-4f);
		if (colEnd < colStart) colEnd = colStart;

		for (int col = colStart; col <= colEnd; ++col) {
			for (int ty = tyMin; ty <= tyMax; ++ty) {
				if (!InMap(col, ty)) continue;
				if (!IsBlockingAt(col, ty)) continue;

				float bx = xOffset_ + col * kTile;
				float by = TyToWorldY(ty);

				float overlapY = std::min(maxY, by + kTile) - std::max(minY, by);
				if (overlapY <= 0.0f) continue;

				float wallLeft = bx;
				if (endRight > wallLeft) {
					float candidate = wallLeft - pw_ - kSkinX;
					if (!collided || candidate < stopX) {
						collided = true;
						stopX = candidate;
					}
				}
			}
		}
	} else {
		// 左方向
		float startLeft = startX;
		float endLeft = targetX;

		int colStart = ToTx(startLeft + 1e-4f);
		int colEnd = ToTx(endLeft + 1e-4f);
		if (colEnd > colStart) colEnd = colStart;

		for (int col = colStart; col >= colEnd; --col) {
			for (int ty = tyMin; ty <= tyMax; ++ty) {
				if (!InMap(col, ty)) continue;
				if (!IsBlockingAt(col, ty)) continue;

				float bx = xOffset_ + col * kTile;
				float by = TyToWorldY(ty);

				float overlapY = std::min(maxY, by + kTile) - std::max(minY, by);
				if (overlapY <= 0.0f) continue;

				float wallRight = bx + kTile;
				if (endLeft < wallRight) {
					float candidate = wallRight + kSkinX;
					if (!collided || candidate > stopX) {
						collided = true;
						stopX = candidate;
					}
				}
			}
		}
	}

	if (collided) {
		playerTr_.pos.x = stopX;
		vel_.x = 0.0f;
	} else {
		playerTr_.pos.x = targetX;
	}
}

// ----------------- 物理解決：縦＋ギミック＋死亡判定 -----------------
void GameScene::ResolveVertical_(float dt) {
	float startY = playerTr_.pos.y;
	float targetY = startY + vel_.y;

	// AABBの現在形
	AABB boxNow = PlayerAabbFull_();

	// X方向の列（プレイヤーが今またいでるタイル範囲）
	float minX = boxNow.x;
	float maxX = boxNow.x + boxNow.w - 1e-4f;
	int txL = ToTx(minX);
	int txR = ToTx(maxX);
	int txMin = std::min(txL, txR);
	int txMax = std::max(txL, txR);

	onGround_ = false;
	bool switchOverlapNow = false;

	if (vel_.y <= 0.0f) {
		// ====== 落下・着地処理 ======
		float startBottom = startY;
		float endBottom = targetY;

		int rowStart = ToTy(startBottom - kSkinY);
		int rowEnd = ToTy(endBottom - kSkinY);
		int rowMin = std::min(rowStart, rowEnd);
		int rowMax = std::max(rowStart, rowEnd);

		bool  hitFloor = false;
		float bestSnapY = targetY;

		for (int row = rowMin; row <= rowMax; ++row) {
			for (int tx = txMin; tx <= txMax; ++tx) {
				if (!InMap(tx, row)) continue;

				Tile tt = grid_[row][tx];

				float bx = xOffset_ + tx * kTile;
				float by = TyToWorldY(row);
				float topY = by + kTile; // タイル上面

				// スプリング / スイッチの当たりは「着地後の仮位置」で判定
				{
					AABB afterBox{boxNow.x, targetY, boxNow.w, boxNow.h};
					if (IsSpring(tt)) {
						if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
							vel_.y = kSpringVy;
						}
					}
					if (tt == Tile::Switch) {
						if (OverlapXY(afterBox, bx, by, kTile, kTile)) {
							switchOverlapNow = true;
						}
					}
				}

				if (!IsBlockingAt(tx, row)) continue;

				// 下向き移動で床にぶつかった？
				if ((startBottom - kSkinY) >= topY &&
					(endBottom - kSkinY) < topY) {

					float overlapX =
						std::min(boxNow.x + boxNow.w, bx + kTile)
						- std::max(boxNow.x, bx);

					if (overlapX > kMinGroundOverlap) {
						float snapY = topY + kSkinY;
						if (!hitFloor || snapY > bestSnapY) {
							hitFloor = true;
							bestSnapY = snapY;

							// fragile踏んだらarmed開始
							if (IsFragile(tt) && !frag_[row][tx].gone) {
								ArmFragile_(tx, row);
							}
						}
					}
				}
			}
		}

		if (hitFloor) {
			playerTr_.pos.y = bestSnapY;
			vel_.y = 0.0f;
			onGround_ = true;
		} else {
			playerTr_.pos.y = targetY;
		}
	} else {
		// ====== 上昇・頭ぶつけ処理 ======
		float startTop = startY + ph_;
		float endTop = targetY + ph_;

		int rowStart = ToTy(startTop + kSkinY);
		int rowEnd = ToTy(endTop + kSkinY);
		int rowMin = std::min(rowStart, rowEnd);
		int rowMax = std::max(rowStart, rowEnd);

		bool  hitCeil = false;
		float bestSnapY = targetY;

		for (int row = rowMin; row <= rowMax; ++row) {
			for (int tx = txMin; tx <= txMax; ++tx) {
				if (!InMap(tx, row)) continue;

				Tile tt = grid_[row][tx];

				float bx = xOffset_ + tx * kTile;
				float by = TyToWorldY(row);
				float bottomY = by; // タイル下面

				// 下から壊せるfragileにヒビを入れる
				if (IsFragile(tt) && !frag_[row][tx].gone) {
					bool canFromBelow = (tt == Tile::FragileAny || tt == Tile::FragileBottom);
					if (canFromBelow) {
						float overlapX =
							std::min(boxNow.x + boxNow.w, bx + kTile)
							- std::max(boxNow.x, bx);
						if (overlapX > kMinGroundOverlap) {
							ArmFragile_(tx, row);
						}
					}
				}


				if (!IsBlockingAt(tx, row)) continue;

				// 上昇で天井にぶつかった？
				if ((startTop + kSkinY) <= bottomY &&
					(endTop + kSkinY) > bottomY) {

					float overlapX =
						std::min(boxNow.x + boxNow.w, bx + kTile)
						- std::max(boxNow.x, bx);
					if (overlapX > kMinGroundOverlap) {

						float snapY = bottomY - ph_ - kSkinY;
						if (!hitCeil || snapY < bestSnapY) {
							hitCeil = true;
							bestSnapY = snapY;
						}
					}
				}
			}
		}

		if (hitCeil) {
			playerTr_.pos.y = bestSnapY;
			vel_.y = 0.0f;
		} else {
			playerTr_.pos.y = targetY;
		}
	}

	// 足元の微接地でもfragileにarmed付与（FragileAny/Top/Regen）
	{
		int txL2 = ToTx(playerTr_.pos.x);
		int txR2 = ToTx(playerTr_.pos.x + pw_ - 1e-4f);
		int txMin2 = std::min(txL2, txR2);
		int txMax2 = std::max(txL2, txR2);

		int rowBelow = ToTy(playerTr_.pos.y - kSkinY);

		for (int tx = txMin2; tx <= txMax2; ++tx) {
			if (!InMap(tx, rowBelow)) continue;
			Tile tt = grid_[rowBelow][tx];
			if (!IsFragile(tt) || frag_[rowBelow][tx].gone) continue;

			// 上から壊せる系（FragileAny/FragileTop/Regen）
			if (!(tt == Tile::FragileAny || tt == Tile::FragileTop || tt == Tile::Regen)) continue;

			float bx = xOffset_ + tx * kTile;
			float overlapX =
				std::min(playerTr_.pos.x + pw_, bx + kTile)
				- std::max(playerTr_.pos.x, bx);
			if (overlapX > kMinGroundOverlap) {
				ArmFragile_(tx, rowBelow);
			}

		}
	}

	// スイッチトグル（前フレーム非接触→今フレーム接触）
	static bool prevSw = false;
	if (switchOverlapNow && !prevSw) {
		switchOn_ = !switchOn_;
	}
	prevSw = switchOverlapNow;

	// コヨーテ/ジャンプバッファ処理
	if (onGround_) coyoteCounter_ = kCoyoteMaxFrames;
	else if (coyoteCounter_ > 0) --coyoteCounter_;

	if (jumpBuffer_ > 0) --jumpBuffer_;

	if ((onGround_ || coyoteCounter_ > 0) && jumpBuffer_ > 0) {
		vel_.y = kJumpVy;
		onGround_ = false;
		jumpBuffer_ = 0;
	}

	// 接地時の小さなズレ吸収（スロープっぽい段差揺れ防止）
	if (onGround_) {
		float stableY = std::floor((playerTr_.pos.y - kSkinY) / kTile) * kTile + kSkinY;
		if (std::fabs(playerTr_.pos.y - stableY) > 1e-4f) {
			playerTr_.pos.y = stableY;
		}
	}

	// fragile / regen タイマー進行と実際の破壊・復活
	for (int y = 0; y < kMapH; ++y) {
		for (int x = 0; x < kMapW; ++x) {
			Tile t = grid_[y][x];
			if (!IsFragile(t)) continue;

			auto &fs = frag_[y][x];
			if (!fs.gone) {
				if (fs.armed) {
					fs.t += dt;
					if (fs.t > kFragileBreakTime) {
						fs.gone = true;
						if (t == Tile::Regen) {
							regen_[y][x].respawn = 0.0f;
						}
					}
				}
			} else if (t == Tile::Regen) {
				regen_[y][x].respawn += dt;
				if (regen_[y][x].respawn >= kRegenRespawnTime) {
					fs = FragileState{}; // 復活: armed=false, t=0, gone=false
				}
			}
		}
	}

	// --- デス判定 ---
	{
		bool killed = false;

		// 奈落
		float deathY = -4.0f * kTile;
		if (playerTr_.pos.y < deathY) {
			killed = true;
		}

		// スパイク
		if (!killed) {
			AABB f = PlayerAabbFull_();
			int l3 = ToTx(f.x);
			int r3 = ToTx(f.x + f.w - 1e-4f);
			int t3 = ToTy(f.y);
			int b3 = ToTy(f.y + f.h - 1e-4f);

			int tyMin3 = std::min(t3, b3);
			int tyMax3 = std::max(t3, b3);
			int txMin3 = std::min(l3, r3);
			int txMax3 = std::max(l3, r3);

			for (int ty = tyMin3; ty <= tyMax3 && !killed; ++ty) {
				for (int tx = txMin3; tx <= txMax3; ++tx) {
					if (!InMap(tx, ty)) continue;
					if (grid_[ty][tx] != Tile::Spike) continue;
					float bx = xOffset_ + tx * kTile;
					float by = TyToWorldY(ty);

					if (OverlapXY(f, bx, by, kTile, kTile)) {
						killed = true;
						break;
					}
				}
			}
		}

		if (killed) {
			ResetStageAll_();
			return; // このフレームはここで終了
		}
	}
}

// ----------------- ステージリセット -----------------
void GameScene::ResetStageAll_() {
	// マップ配置・壊れ状態・再生床タイマー・スイッチ・スポーン位置を初期状態に戻す
	for (int y = 0; y < kMapH; ++y) {
		for (int x = 0; x < kMapW; ++x) {
			grid_[y][x] = initialGrid_[y][x];
			frag_[y][x] = initialFrag_[y][x];
			regen_[y][x] = initialRegen_[y][x];
		}
	}
	switchOn_ = initialSwitchOn_;
	spawnTx_ = initialSpawnTx_;
	spawnTy_ = initialSpawnTy_;

	ClampSpawnToSafe();

	// プレイヤーも初期リスポーンへ
	playerTr_.pos = {
		xOffset_ + spawnTx_ * kTile,
		TyToWorldY(spawnTy_) + 0.5f,
		kPlayerZ
	};
	vel_ = {0,0,0};
	onGround_ = false;
	coyoteCounter_ = 0;
	jumpBuffer_ = 0;
}

// ----------------- Update -----------------
void GameScene::Update(float dt) {
	auto *dx = engineContext_->directXCommon;
	auto *in = engineContext_->input;

	if (!cleared_) {
		elapsedTime_ += dt;
	}

	// 画面リサイズに合わせてカメラOrthoサイズを追従
	{
		float screenW = (float)dx->GetWidth();
		float screenH = (float)dx->GetHeight();
		float screenAspect = screenW / std::max(1.0f, screenH);

		const float worldW = kMapW * kTile;
		const float worldH = kMapH * kTile;
		float mapAspect = worldW / worldH;

		float orthoW, orthoH;
		if (screenAspect >= mapAspect) {
			orthoH = worldH;
			orthoW = worldH * screenAspect;
		} else {
			orthoW = worldW;
			orthoH = worldW / screenAspect;
		}

		if (camera_.IsOrtho()) {
			camera_.SetOrthoViewSize(orthoW, orthoH);
		}
		camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
	}

	// 入力
	int ax = 0;
	if (in->IsKeyPressed(DIK_A)) ax -= 1;
	if (in->IsKeyPressed(DIK_D)) ax += 1;
	if (KeyPressed_(DIK_SPACE))  jumpBuffer_ = kJumpBufferFrames;

	// 横速度
	vel_.x = (onGround_ ? kMoveGround : kMoveAir) * (float)ax;

	// 重力
	vel_.y += -kGravity;
	if (vel_.y < kMaxFallVy) vel_.y = kMaxFallVy;

	// 物理解決（軸分離スイープ）
	ResolveHorizontal_();
	ResolveVertical_(dt);

	// すべての壊せる床がなくなったらクリア
	if (!cleared_ && AllFragileGone_()) {
		cleared_ = true;
		finalTime_ = elapsedTime_;
		GoToClearScene_();
		return; // このフレームのUpdateはそこで終わり
	}

}

void GameScene::GoToClearScene_() {
	if (!sceneManager_) {
		// シーンマネージャ取れないなら何もしない（安全策）
		return;
	}

	// 次のステージ番号を計算
	int nextStage = stageId_ + 1;
	if (nextStage >= maxStageCount_) {
		nextStage = 0; // たとえば最後まで行ったら0に戻す
	}

	// ClearScene にクリアタイムと次のステージIDを渡して遷移
	engineContext_->sceneManager->ChangeScene(
		std::make_unique<ClearScene>(finalTime_, nextStage)
	);
}


bool GameScene::AllFragileGone_() const {
	for (int y = 0; y < kMapH; ++y) {
		for (int x = 0; x < kMapW; ++x) {
			Tile t = grid_[y][x];
			if (!IsFragile(t)) continue;

			// Regenも対象に含める:
			//   - まだ残ってる？ = gone==false
			//   - つまり1個でも "gone==false" があれば未クリア
			const FragileState &fs = frag_[y][x];
			if (!fs.gone) {
				// まだ実体ある
				return false;
			}
		}
	}
	// 1つも残ってない！
	return true;
}


// ----------------- 背景＆ステージ描画本体 -----------------
void GameScene::DrawBackgroundAndStage_() {
	auto *dx = engineContext_->directXCommon;
	auto *cmd = dx->GetCommandList();
	auto *renderer = renderContext_->modelRenderer;

	const float worldW = kMapW * kTile;
	const float worldH = kMapH * kTile;

	auto Deg = [](float d) { return XMConvertToRadians(d); };
	auto DrawM = [&](Model &m, const XMFLOAT3 &p, const XMFLOAT3 &s, const XMFLOAT3 &r) {
		Transform t{};
		t.pos = p;
		t.scale = s;
		t.rot = {Deg(r.x), Deg(r.y), Deg(r.z)};
		renderer->Draw(cmd, m, t);
		};
	auto Hash01 = [](int n) {
		uint32_t h = (uint32_t)(n * 2654435761u) ^ 0x9e3779b9u;
		h ^= (h >> 13); h *= 0x5bd1e995u; h ^= (h >> 15);
		return (h & 0xFFFF) / 65535.0f;
		};

	// 1) 夜空レイヤー
	{
		DrawM(mdlSolid_,
			{0.0f, worldH * 0.50f, 38.0f},
			{worldW * 2.8f, worldH * 2.2f, 0.25f},
			{0.0f, 0.0f, -4.5f});

		for (int i = -3; i <= 3; ++i) {
			float off = i * worldW * 0.25f;
			DrawM(mdlSolid_,
				{off, worldH * (0.6f + 0.08f * std::sin(i * 1.2f)), 37.6f},
				{worldW * 0.9f, worldH * 0.3f, 0.05f},
				{0, 0, (i % 2 == 0) ? -10.0f : 8.0f});
		}

		DrawM(mdlSwitchBlockOff_,
			{worldW * 0.35f, worldH * 0.85f, 37.0f},
			{1.4f, 1.4f, 0.2f},
			{0, 0, 0});
	}

	// 2) ビル群
	auto DrawBuildings = [&](float z, float yBase, float span,
		float wMin, float wMax,
		float hMin, float hMax,
		float tilt) {
			int count = int(worldW / span) + 6;
			for (int i = -count / 2; i <= count / 2; i++) {
				float rx = i * span;
				float rw = wMin + (wMax - wMin) * Hash01(i * 31 + int(z * 10));
				float rh = hMin + (hMax - hMin) * Hash01(i * 97 + int(z * 20));

				// ビル本体
				DrawM(mdlSolid_, {rx, yBase + rh * 0.5f, z},
					{rw, rh * 0.5f, 0.22f},
					{0, 0, ((i & 1) ? tilt : -tilt)});

				// 屋上の装置っぽい箱
				DrawM(mdlSwitchBlockOff_,
					{rx + rw * 0.15f, yBase + rh + 0.10f, z - 0.05f},
					{rw * 0.12f, rw * 0.12f, 0.18f},
					{0, 0, (i & 1) ? -6.0f : 6.0f});

				// 警告灯(点滅風ライト)
				if ((i + (int)z) % 4 == 0) {
					DrawM(mdlSwitchBlockOn_,
						{rx, yBase + rh + 0.25f, z - 0.06f},
						{0.10f, 0.10f, 0.15f},
						{0, 0, 0});
				}
			}
		};
	DrawBuildings(33.0f, worldH * 0.06f,
		worldW * 0.14f,
		worldW * 0.06f, worldW * 0.10f,
		worldH * 0.14f, worldH * 0.28f,
		2.0f);
	DrawBuildings(30.0f, worldH * 0.08f,
		worldW * 0.12f,
		worldW * 0.07f, worldW * 0.12f,
		worldH * 0.18f, worldH * 0.34f,
		3.0f);
	DrawBuildings(27.0f, worldH * 0.10f,
		worldW * 0.10f,
		worldW * 0.08f, worldW * 0.14f,
		worldH * 0.22f, worldH * 0.40f,
		4.0f);

	// 3) クレーン
	{
		DrawM(mdlJumpOnly_,
			{-worldW * 0.30f, worldH * 0.86f, 24.8f},
			{0.06f, worldH * 0.55f, 0.30f},
			{0,0,0});
		DrawM(mdlSolid_,
			{-worldW * 0.05f, worldH * 1.05f, 24.6f},
			{worldW * 0.55f, 0.06f, 0.30f},
			{0,0,-9.0f});
		DrawM(mdlSolid_,
			{worldW * 0.22f, worldH * 0.88f, 24.5f},
			{0.035f, worldH * 0.28f, 0.25f},
			{0,0,0});
		DrawM(mdlSwitch_,
			{worldW * 0.22f, worldH * 0.72f, 24.4f},
			{0.14f, 0.14f, 0.22f},
			{0,0,0});

		// 吊られてる鉄骨
		DrawM(mdlSolid_,
			{worldW * 0.22f, worldH * 0.55f, 24.3f},
			{0.35f, 0.08f, 0.25f},
			{0,0,4.0f});

		// 作業灯
		DrawM(mdlSwitchBlockOn_,
			{worldW * 0.22f, worldH * 0.47f, 24.2f},
			{0.15f, 0.08f, 0.22f},
			{0,0,0});
	}

	// 4) 投光器
	auto Flood = [&](XMFLOAT3 b, float rotZ, bool blink) {
		DrawM(mdlSolid_,
			{b.x, b.y, 22.0f},
			{0.05f, 0.55f, 0.25f},
			{0,0,0});
		DrawM(mdlSwitchBlockOn_,
			{b.x, b.y + 0.38f, 21.9f},
			{0.22f, 0.12f, 0.22f},
			{0,0,rotZ});
		DrawM(mdlSpike_,
			{b.x + 0.10f, b.y + 0.25f, 21.7f},
			{worldW * 0.22f, worldH * 0.10f, 0.05f},
			{0,0,rotZ - 12.0f});
		DrawM(mdlSpike_,
			{b.x + 0.08f, b.y + 0.27f, 21.6f},
			{worldW * 0.24f, worldH * 0.11f, 0.05f},
			{0,0,rotZ - 14.0f});
		if (blink) {
			DrawM(mdlSwitch_,
				{b.x, b.y - 0.45f, 21.8f},
				{0.12f, 0.12f, 0.15f},
				{0,0,0});
		}
		};
	Flood({-worldW * 0.48f, worldH * 0.82f, 0}, 10.0f, true);
	Flood({worldW * 0.52f, worldH * 0.74f, 0}, 18.0f, false);

	// 5) タイル描画（足場・ギミック・点滅対応）
	auto Hash4 = [](int x, int y) {
		uint32_t h = (uint32_t)(x * 73856093u) ^ (uint32_t)(y * 19349663u);
		h ^= (h >> 13); h *= 0x5bd1e995u;
		float r0 = (float)((h) & 0xFF) / 255.0f;
		float r1 = (float)((h >> 8) & 0xFF) / 255.0f;
		float r2 = (float)((h >> 16) & 0xFF) / 255.0f;
		float r3 = (float)((h >> 24) & 0xFF) / 255.0f;
		return XMFLOAT4(r0, r1, r2, r3);
		};

	for (int ty = 0; ty < kMapH; ++ty) {
		for (int tx = 0; tx < kMapW; ++tx) {
			Tile t = grid_[ty][tx];

			// 壊れて消滅済み（Regen以外）はもう描かない
// 壊れて消滅済みのfragileブロックは描かない（Regenも含む）
			if (IsFragile(t) && frag_[ty][tx].gone) {
				continue;
			}


			// スイッチ連動ブロック可視判定
			if (t == Tile::SwitchBlockOn && !switchOn_) continue;
			if (t == Tile::SwitchBlockOff && switchOn_) continue;

			Model *m = nullptr;
			bool isFrag = false;
			switch (t) {
			case Tile::Solid:           m = &mdlSolid_;          break;
			case Tile::FragileAny:      m = &mdlFragileAny_;     isFrag = true; break;
			case Tile::FragileTop:      m = &mdlFragileTop_;     isFrag = true; break;
			case Tile::FragileBottom:   m = &mdlFragileBottom_;  isFrag = true; break;
			case Tile::Regen:           m = &mdlRegen_;          isFrag = true; break;
			case Tile::Spring:          m = &mdlSpring_;         break;
			case Tile::Spike:           m = &mdlSpike_;          break;
			case Tile::Switch:          m = &mdlSwitch_;         break;
			case Tile::SwitchBlockOn:   m = &mdlSwitchBlockOn_;  break;
			case Tile::SwitchBlockOff:  m = &mdlSwitchBlockOff_; break;
			case Tile::JumpOnly:        m = &mdlJumpOnly_;       break;
			default: break;
			}
			if (!m) continue;

			float wx = xOffset_ + tx * kTile;
			float wy = TyToWorldY(ty);

			// 壊れかけ床用フェードアルファ（通常は1.0）
			float alphaMul = isFrag ? FragileBlinkFactor_(tx, ty) : 1.0f;

			// このタイルの基本Transform
			Transform base{};
			base.pos = {wx + 0.5f * kTile, wy + 0.5f * kTile, 0.0f};
			base.scale = {0.5f, 0.5f, 0.5f * kBlockDepth};
			base.rot = {0,0,0};

			// 足場にちょっと工事現場っぽい歪み
			{
				XMFLOAT4 r = {0,0,0,0};
				{
					// 同じHash4()でランダム傾き・ばらつき
					uint32_t h = (uint32_t)(tx * 73856093u) ^ (uint32_t)(ty * 19349663u);
					h ^= (h >> 13); h *= 0x5bd1e995u;
					float r0 = (float)((h) & 0xFF) / 255.0f;
					float r1 = (float)((h >> 8) & 0xFF) / 255.0f;
					float r2 = (float)((h >> 16) & 0xFF) / 255.0f;
					float r3 = (float)((h >> 24) & 0xFF) / 255.0f;
					r = {r0,r1,r2,r3};
				}

				base.rot.z = Deg((r.x * 2.0f - 1.0f) * 4.0f);
				base.scale.x *= (1.0f + (r.y * 0.1f - 0.05f));
				base.scale.y *= (1.0f + (r.z * 0.1f - 0.05f));
			}

			// 本体ブロック
			renderer->Draw(cmd, *m, base, alphaMul);

			// ==== デコレーション系 ==== 
			// 足場支柱・テープ
			bool plat = (t == Tile::Solid || t == Tile::JumpOnly ||
				t == Tile::FragileAny || t == Tile::FragileTop ||
				t == Tile::FragileBottom || t == Tile::Regen);
			if (plat) {
				struct Corner { float ox, oy; };
				Corner cs[4] = {
					{-0.35f,-0.35f},{0.35f,-0.35f},
					{0.35f, 0.35f},{-0.35f, 0.35f}
				};
				for (auto &c : cs) {
					Transform leg{};
					leg.pos = {
						base.pos.x + c.ox * base.scale.x,
						base.pos.y + c.oy * base.scale.y - 0.175f,
						base.pos.z + 0.05f
					};
					leg.scale = {0.03f, 0.175f, base.scale.z * 0.8f};
					leg.rot = {0,0,0};
					renderer->Draw(cmd, mdlSolid_, leg, alphaMul);
				}

				Transform tape{};
				tape.pos = {
					base.pos.x,
					base.pos.y + base.scale.y * 0.8f,
					base.pos.z - 0.05f
				};
				tape.scale = {
					base.scale.x * 0.8f,
					base.scale.y * 0.18f,
					base.scale.z
				};
				tape.rot = base.rot;
				renderer->Draw(cmd, mdlSolid_, tape, alphaMul);
			}

			// ヒビ・警告サイン（壊れる床だけ）
			if (isFrag) {
				auto Crack = [&](float ox, float oy, float hw, float hh, float deg) {
					Transform c = base;
					c.pos.x += ox;
					c.pos.y += oy;
					c.pos.z -= 0.06f;
					c.scale.x = hw;
					c.scale.y = hh;
					c.scale.z = base.scale.z * 0.6f;
					c.rot.z = DirectX::XMConvertToRadians(deg);
					renderer->Draw(cmd, mdlSolid_, c, alphaMul);
					};

				{   // ランダムっぽい亀裂2本
					uint32_t h2 = (uint32_t)(tx * 13 + 7) ^ (uint32_t)(ty * 17 + 3);
					h2 ^= (h2 >> 13); h2 *= 0x5bd1e995u;
					float r0 = (float)((h2) & 0xFF) / 255.0f;
					float r1 = (float)((h2 >> 8) & 0xFF) / 255.0f;
					float r2 = (float)((h2 >> 16) & 0xFF) / 255.0f;
					float r3 = (float)((h2 >> 24) & 0xFF) / 255.0f;

					Crack((r0 * 0.2f - 0.1f),
						(r1 * 0.2f - 0.1f),
						0.28f, 0.03f,
						r2 * 60.0f - 30.0f);

					Crack((r1 * 0.3f - 0.15f),
						(r2 * 0.3f - 0.15f),
						0.18f, 0.02f,
						r3 * 100.0f - 50.0f);
				}

				Transform sign{};
				sign.pos = {
					base.pos.x,
					base.pos.y + base.scale.y * 0.6f,
					base.pos.z - 0.12f
				};
				sign.scale = {
					base.scale.x * 0.45f,
					base.scale.y * 0.32f,
					base.scale.z
				};
				sign.rot = {0,0,DirectX::XMConvertToRadians(12.0f)};
				renderer->Draw(cmd, mdlSolid_, sign, alphaMul);

				Transform stick{};
				stick.pos = {
					sign.pos.x,
					sign.pos.y - sign.scale.y * 0.9f,
					sign.pos.z + 0.01f
				};
				stick.scale = {
					sign.scale.x * 0.12f,
					sign.scale.y * 0.9f,
					sign.scale.z
				};
				stick.rot = {0,0,0};
				renderer->Draw(cmd, mdlSolid_, stick, alphaMul);
			}

			// スパイク（バリケード）
			if (t == Tile::Spike) {
				Transform bar{};
				bar.pos = {
					base.pos.x,
					base.pos.y + base.scale.y * 0.4f,
					base.pos.z - 0.07f
				};
				bar.scale = {
					base.scale.x * 0.9f,
					base.scale.y * 0.22f,
					base.scale.z
				};
				bar.rot = {0,0,DirectX::XMConvertToRadians(-6.0f)};
				renderer->Draw(cmd, mdlSolid_, bar, alphaMul);

				auto Leg = [&](float s) {
					Transform l{};
					l.pos = {
						base.pos.x + base.scale.x * 0.7f * s,
						base.pos.y - base.scale.y * 0.1f,
						base.pos.z - 0.05f
					};
					l.scale = {
						base.scale.x * 0.18f,
						base.scale.y * 0.7f,
						base.scale.z
					};
					l.rot = {0,0,DirectX::XMConvertToRadians(15.0f * s)};
					renderer->Draw(cmd, mdlSolid_, l, alphaMul);
					};
				Leg(-1.0f);
				Leg(+1.0f);
			}

			// スプリング
			if (t == Tile::Spring) {
				Transform basePlate{};
				basePlate.pos = {
					base.pos.x,
					base.pos.y - base.scale.y * 0.6f,
					base.pos.z - 0.05f
				};
				basePlate.scale = {
					base.scale.x * 0.8f,
					base.scale.y * 0.4f,
					base.scale.z
				};
				basePlate.rot = {0,0,0};
				renderer->Draw(cmd, mdlSolid_, basePlate, alphaMul);

				Transform pillar{};
				pillar.pos = {
					base.pos.x,
					base.pos.y + base.scale.y * 0.1f,
					base.pos.z - 0.07f
				};
				pillar.scale = {
					base.scale.x * 0.25f,
					base.scale.y * 0.9f,
					base.scale.z
				};
				pillar.rot = {0,0,0};
				renderer->Draw(cmd, mdlSolid_, pillar, alphaMul);

				Transform head{};
				head.pos = {
					base.pos.x,
					base.pos.y + base.scale.y * 0.9f,
					base.pos.z - 0.09f
				};
				head.scale = {
					base.scale.x * 0.7f,
					base.scale.y * 0.25f,
					base.scale.z
				};
				head.rot = {0,0,DirectX::XMConvertToRadians(5.0f)};
				renderer->Draw(cmd, mdlSolid_, head, alphaMul);
			}

			// スイッチ（ON/OFFレバー）
			if (t == Tile::Switch) {
				Transform box{};
				box.pos = {
					base.pos.x + base.scale.x * 0.7f,
					base.pos.y + base.scale.y * 0.2f,
					base.pos.z - 0.06f
				};
				box.scale = {
					base.scale.x * 0.45f,
					base.scale.y * 0.55f,
					base.scale.z
				};
				box.rot = {0,0,DirectX::XMConvertToRadians(-5.0f)};
				renderer->Draw(cmd, mdlSolid_, box, alphaMul);

				Transform lever{};
				lever.pos = {
					box.pos.x + box.scale.x * 0.3f,
					box.pos.y + box.scale.y * 0.2f,
					box.pos.z - 0.02f
				};
				lever.scale = {
					box.scale.x * 0.25f,
					box.scale.y * 0.6f,
					box.scale.z
				};
				lever.rot = {
					0,0,
					DirectX::XMConvertToRadians(switchOn_ ? 30.0f : -30.0f)
				};
				renderer->Draw(cmd, mdlSolid_, lever, alphaMul);
			}
		}
	}



	// 6) プレイヤー
	{
		float s = 0.5f;
		XMFLOAT3 mn = playerModel_.GetLocalMin();
		Transform p{};
		p.pos = {playerTr_.pos.x + pw_ * 0.5f,
				 playerTr_.pos.y - (mn.y * s),
				 kPlayerZ};
		p.scale = {s * (pw_  / 1.0f),s * (ph_ / 1.0f), s * kPlayerDepth};
		p.rot = {0,0,0};
		renderer->Draw(cmd, playerModel_, p);

		// 安全帯/ベルトっぽいアクセント
		Transform belt{};
		belt.pos = {p.pos.x,
					p.pos.y + p.scale.y * 0.2f,
					p.pos.z - 0.03f};
		belt.scale = {p.scale.x * 0.7f,
					  p.scale.y * 0.12f,
					  p.scale.z};
		belt.rot = {0,0,Deg(8)};
		renderer->Draw(cmd, mdlSolid_, belt);
	}

	// 7) 手前フレーム
	{
		float y = -0.5f * kTile;

		Transform rail{};
		rail.pos = {0.0f, y, -0.40f};
		rail.scale = {worldW * 0.65f, 0.05f, 0.22f};
		renderer->Draw(cmd, mdlSolid_, rail);

		for (int i = -3; i <= 3; ++i) {
			float x = i * (worldW * 0.16f);
			Transform t{};
			t.pos = {x, y + 0.2f, -0.41f};
			t.scale = {worldW * 0.09f, 0.03f, 0.22f};
			t.rot = {0,0,Deg((i % 2 == 0) ? -10.0f : 12.0f)};
			renderer->Draw(cmd, mdlSolid_, t);
		}

		for (int i = -2; i <= 2; ++i) {
			Transform c{};
			c.pos = {i * (worldW * 0.18f),
					 y + 0.5f,
					 -0.42f};
			c.scale = {worldW * 0.08f, 0.01f, 0.2f};
			c.rot = {0,0,Deg(10.0f * std::sinf(static_cast<float>(i)))};
			renderer->Draw(cmd, mdlJumpOnly_, c);
		}
	}
}

// ----------------- Draw -----------------
void GameScene::Draw() {
	auto *dx = engineContext_->directXCommon;
	auto *cmd = dx->GetCommandList();
	auto *renderer = renderContext_->modelRenderer;

	renderer->Begin(cmd, dx, camera_);
	DrawBackgroundAndStage_();
	renderer->End(cmd);

	// もうHUDやImGuiウィンドウは出さない
}

// ----------------- Finalize -----------------
void GameScene::Finalize() {
	// ComPtrで自動解放
}

void GameScene::ArmFragile_(int tx, int ty) {
	if (!InMap(tx, ty)) return;
	if (!IsFragile(grid_[ty][tx])) return;

	FragileState &fs = frag_[ty][tx];
	if (fs.gone) return;

	if (!fs.armed) {
		fs.armed = true;
		// 触れた瞬間から点滅させたいので、いきなり警告フェーズまでタイマーを進める
		float blinkStartT = kFragileBreakTime - kFragileBlinkStart;
		if (fs.t < blinkStartT) {
			fs.t = blinkStartT;
		}
	}
}
