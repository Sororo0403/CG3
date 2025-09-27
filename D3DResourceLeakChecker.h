#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

/// <summary>
/// DXGI の ReportLiveObjects をデストラクタで呼ぶだけのクラス。<br/>
/// スコープ終了時（デストラクション時）に、DXGI
/// 管理リソースのリークを検出する。
/// </summary>
class D3DResourceLeakChecker {
public:
  /// <summary>
  /// デストラクタ。<br/>
  /// _DEBUG ビルドで DXGI の ReportLiveObjects を実行する。
  /// </summary>
  ~D3DResourceLeakChecker();
};