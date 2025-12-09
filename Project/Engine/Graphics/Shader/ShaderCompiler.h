#pragma once
#include <string>
#include <wrl.h>
#include <dxcapi.h>

class ShaderCompiler {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    ShaderCompiler();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~ShaderCompiler() = default;

    /// <summary>
    /// HLSL ファイルをコンパイルしてバイナリを取得
    /// </summary>
    /// <param name="filePath">コンパイルする .hlsl のパス</param>
    /// <param name="profile">シェーダープロファイル</param>
    /// <returns>コンパイル後のシェーダーバイナリ</returns>
    Microsoft::WRL::ComPtr<IDxcBlob>
    Compile(const std::wstring &filePath, const std::wstring &profile,
            const std::wstring &includeDir = L"");

private:
    Microsoft::WRL::ComPtr<IDxcUtils> utils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
