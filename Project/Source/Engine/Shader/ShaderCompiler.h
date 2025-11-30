#pragma once
#include <string>
#include <wrl.h>
#include <dxcapi.h>

class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler() = default;

    Microsoft::WRL::ComPtr<IDxcBlob> Compile(
        const std::wstring &filePath,
        const std::wstring &profile);

private:
    Microsoft::WRL::ComPtr<IDxcUtils> utils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
