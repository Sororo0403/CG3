// D3DResourceLeakChecker.cpp
#include "D3DResourceLeakChecker.h"

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
#ifdef _DEBUG
  // DXGI デバッグインターフェースからライブオブジェクトをレポート
  Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
    // どのカテゴリでも拾いたいので DXGI_DEBUG_ALL
    // 詳細表示（DXGI_DEBUG_RLO_DETAIL）で “誰が生き残っているか” を出す
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
  }
  // 失敗時は何もしない（リリースビルド互換・依存排除）
#endif
}
