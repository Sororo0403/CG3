#pragma once

#include <Unknwn.h>

#include <dxcapi.h>
#include <wrl/client.h>

#include <string>

class ShaderCompiler {
public:
  /// <summary>
  /// コンストラクタ。
  /// DXC リソースの生成は行わないため、使用前に Initialize() を呼び出すこと。
  /// </summary>
  ShaderCompiler() = default;

  /// <summary>
  /// デストラクタ。
  /// Finalize() を通じて DXC のリソースを解放する。
  /// </summary>
  ~ShaderCompiler();

  /// <summary>
  /// DXC コンパイラを初期化する。
  /// DxcUtils・DxcCompiler・IncludeHandler の生成を行う。
  /// </summary>
  void Initialize();

  /// <summary>
  /// 指定された HLSL ファイルをコンパイルし、DXIL バイナリ（IDxcBlob）を返す。
  /// </summary>
  /// <param name="filePath">HLSL ファイルへのパス（UTF-8 文字列）</param>
  /// <param name="entryPoint">エントリポイント名（例：VSMain, PSMain）</param>
  /// <param name="profile">シェーダープロファイル（例：vs_6_0, ps_6_0）</param>
  /// <param name="includeDir">#include を探索するディレクトリ（任意、未指定可）</param>
  /// <returns>コンパイルされたシェーダーバイナリ（IDxcBlob）</returns>
  Microsoft::WRL::ComPtr<IDxcBlob>
  CompileShader(const std::string &filePath, const std::string &entryPoint,
                const std::string &profile, const std::string &includeDir = "");

  // Setter
  void SetShaderRoot(const std::string &root) { shaderRoot_ = root; }

private:
  bool initialized_ = false;

  std::string shaderRoot_;

  Microsoft::WRL::ComPtr<IDxcUtils> utils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
