#pragma once
#include <d3d12.h>
class DirectXCommon;
class SpriteCommon;

// エンジン全体で共有する長寿命オブジェクト束
struct EngineContext {
    DirectXCommon *dx = nullptr;
    ID3D12Device *device = nullptr;
    SpriteCommon *spriteCommon = nullptr;
};

// フレームごとに変わる描画用ハンドル
struct RenderContext {
    ID3D12GraphicsCommandList *cmdList = nullptr;
};
