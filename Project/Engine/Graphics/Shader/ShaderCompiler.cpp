#include "ShaderCompiler.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cassert>
#include <vector>

#include "Logger/Logger.h"
#include "String/StringUtil.h"

using Microsoft::WRL::ComPtr;

ShaderCompiler::~ShaderCompiler() {
    if (!initialized_) {
        return;
    }

    includeHandler_.Reset();
    compiler_.Reset();
    utils_.Reset();
    initialized_ = false;

    LOG_INFO("ShaderCompiler: Finalized.");
}

void ShaderCompiler::Initialize() {
    if (initialized_) {
        LOG_INFO("ShaderCompiler: Already initialized.");
        return;
    }

    LOG_INFO("ShaderCompiler: Initializing DXC...");

    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
    if (FAILED(hr)) {
        LOG_ERROR("ShaderCompiler: Failed to create DxcUtils.");
        assert(false);
    }

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
    if (FAILED(hr)) {
        LOG_ERROR("ShaderCompiler: Failed to create DxcCompiler.");
        assert(false);
    }

    hr = utils_->CreateDefaultIncludeHandler(&includeHandler_);
    if (FAILED(hr)) {
        LOG_ERROR("ShaderCompiler: Failed to create IncludeHandler.");
        assert(false);
    }

    initialized_ = true;
    LOG_INFO("ShaderCompiler: DXC initialization completed.");
}

ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const std::string &filePath,
                                               const std::string &entryPoint,
                                               const std::string &profile,
                                               const std::string &includeDir) {
    LOG_INFO("ShaderCompiler: Compile start → " + filePath);

    std::string fullPath;
    if (!shaderRoot_.empty()) {
        fullPath = shaderRoot_ + "/" + filePath;
    } else {
        fullPath = filePath;
    }

    LOG_INFO("ShaderCompiler: LoadFile → " + fullPath);

    // UTF-8 → UTF-16 変換（★ fullPath を使う）
    std::wstring wFilePath = StringUtil::UTF8ToUTF16(fullPath);
    std::wstring wEntryPoint = StringUtil::UTF8ToUTF16(entryPoint);
    std::wstring wProfile = StringUtil::UTF8ToUTF16(profile);
    std::wstring wIncludeDir = StringUtil::UTF8ToUTF16(includeDir);

    // HLSL ファイル読み込み
    ComPtr<IDxcBlobEncoding> source = nullptr;
    HRESULT hr = utils_->LoadFile(wFilePath.c_str(), nullptr, &source);

    if (FAILED(hr)) {
        LOG_ERROR("ShaderCompiler: Failed to load shader file → " + filePath);
        assert(false);
    }

    DxcBuffer buffer{};
    buffer.Ptr = source->GetBufferPointer();
    buffer.Size = source->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    // -----------------------------
    // DXC コンパイル引数
    // -----------------------------
    std::vector<LPCWSTR> args = {
        wFilePath.c_str(), // ソース名（エラー表示用）
        L"-E",
        wEntryPoint.c_str(), // エントリポイント
        L"-T",
        wProfile.c_str(), // シェーダープロファイル
        L"-Zi", // デバッグ情報
        L"-Qembed_debug", // DXIL 内にデバッグ情報埋め込み
        L"-Zpr", // 行列を列メジャー指定
        L"-Od", // 最適化無効（デバッグ重視）
        L"-WX" // 警告をエラー扱い
    };

    // include ディレクトリ（任意）
    if (!wIncludeDir.empty()) {
        args.push_back(L"-I");
        args.push_back(wIncludeDir.c_str());
    }

    // -----------------------------
    // コンパイル実行
    // -----------------------------
    ComPtr<IDxcResult> result;
    hr = compiler_->Compile(&buffer, args.data(), (UINT32) args.size(),
                            includeHandler_.Get(), IID_PPV_ARGS(&result));

    if (FAILED(hr)) {
        LOG_ERROR("ShaderCompiler: DXC compile call failed.");
        assert(false);
    }

    // -----------------------------
    // エラーチェック
    // -----------------------------
    ComPtr<IDxcBlobUtf8> errors = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

    if (errors && errors->GetStringLength() > 0) {
        LOG_ERROR("ShaderCompiler: Errors detected:");
        OutputDebugStringA(errors->GetStringPointer());
        assert(false);
    }

    // -----------------------------
    // シェーダーオブジェクト取得
    // -----------------------------
    ComPtr<IDxcBlob> shaderBlob = nullptr;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

    if (FAILED(hr)) {
        LOG_ERROR("ShaderCompiler: Failed to retrieve compiled shader object.");
        assert(false);
    }

    LOG_INFO("ShaderCompiler: Compile completed → OK");

    return shaderBlob;
}
