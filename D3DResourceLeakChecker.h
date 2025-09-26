#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

/// <summary>
/// DXGI の ReportLiveObjects をデストラクタで呼ぶだけのクラス。
/// </summary>
class D3DResourceLeakChecker {
public:
  /// <summary>
  /// デストラクタ
  /// </summary>
  ~D3DResourceLeakChecker();
};
