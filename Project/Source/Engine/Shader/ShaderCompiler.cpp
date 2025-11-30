#include "ShaderCompiler.h"
#include <cassert>
#include <vector>

using namespace Microsoft::WRL;

ShaderCompiler::ShaderCompiler() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
    assert(SUCCEEDED(hr));

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
    assert(SUCCEEDED(hr));

    hr = utils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

ComPtr<IDxcBlob> ShaderCompiler::Compile(
    const std::wstring &filePath,
    const std::wstring &profile) {

    // ----- ファイル読み込み -----
    ComPtr<IDxcBlobEncoding> source = nullptr;
    HRESULT hr = utils_->LoadFile(filePath.c_str(), nullptr, &source);
    assert(SUCCEEDED(hr));

    DxcBuffer buffer{};
    buffer.Ptr = source->GetBufferPointer();
    buffer.Size = source->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    // ----- コンパイル引数 -----
    std::vector<LPCWSTR> arguments = {
        filePath.c_str(),             // [0] ファイルパス
        L"-E", L"main",               // [1] エントリポイント
        L"-T", profile.c_str(),       // [2] Shader profile
        L"-Zi",                       // [3] デバッグ情報
        L"-Qembed_debug",             // [4] PDBを埋め込み
        L"-Zpr"                       // [5] 行優先行列
    };

    ComPtr<IDxcResult> result;
    hr = compiler_->Compile(
        &buffer,
        arguments.data(),
        (UINT32)arguments.size(),
        includeHandler_.Get(),
        IID_PPV_ARGS(&result));
    assert(SUCCEEDED(hr));

    // ----- エラー出力があるか -----
    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

    if (errors != nullptr && errors->GetStringLength() != 0) {
        OutputDebugStringA(errors->GetStringPointer());
    }

    // ----- 成功したバイナリを取得 -----
    ComPtr<IDxcBlob> shaderBlob;
    hr = result->GetOutput(
        DXC_OUT_OBJECT,
        IID_PPV_ARGS(&shaderBlob),
        nullptr);
    assert(SUCCEEDED(hr));

    return shaderBlob;
}
