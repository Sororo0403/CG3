#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

/// <summary>
/// DXGIのReportLiveObjectsをデストラクタで呼ぶだけのクラス。<br/>
/// スコープ終了時(デストラクション時)に、DXGI管理リソースのリークを検出する。
/// </summary>
class D3DResourceLeakChecker {
public:
  /// <summary>
  /// デストラクタ。<br/>
  /// _DEBUGビルドでDXGIのReportLiveObjectsを実行する。
  /// </summary>
  ~D3DResourceLeakChecker();
};