#include "ShaderCompiler.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

bool ShaderCompiler::Initialize() {
    if (dxcUtils_ && dxcCompiler_ && includeHandler_)
        return true;

    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    if (FAILED(hr)) return false;

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    if (FAILED(hr)) return false;

    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    if (FAILED(hr)) return false;

    return true;
}

ShaderCompiler::Result ShaderCompiler::CompileFromFile(
    const std::wstring &filePath, const std::wstring &entry,
    const std::wstring &profile, const std::vector<Define> &defines,
    const std::vector<std::wstring> &extraArgs) const {
    Result out{};

    if (!dxcUtils_ || !dxcCompiler_ || !includeHandler_) {
        return out;
    }

    // ファイル読み込み → UTF-8 へ正規化
    ComPtr<IDxcBlobEncoding> sourceRaw;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &sourceRaw);
    if (FAILED(hr) || !sourceRaw) {
        return out;
    }

    ComPtr<IDxcBlobUtf8> sourceUtf8;
    hr = dxcUtils_->GetBlobAsUtf8(sourceRaw.Get(), &sourceUtf8);
    if (FAILED(hr) || !sourceUtf8) {
        return out;
    }

    DxcBuffer buffer{};
    buffer.Ptr = sourceUtf8->GetBufferPointer();
    buffer.Size = sourceUtf8->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    // 追加引数に「ソースのあるディレクトリ」を -I で自動付与（相対 #include を自然に）
    std::vector<std::wstring> extraWithDir = extraArgs;
    try {
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            extraWithDir.push_back(L"-I");
            extraWithDir.push_back(p.parent_path().wstring());
        }
    }
    catch (...) {
        // パス解析に失敗しても致命ではないので黙殺
    }

    auto args = BuildArguments(filePath, entry, profile, defines, extraWithDir);
    return DoCompile(buffer, args);
}

ShaderCompiler::Result ShaderCompiler::CompileFromSource(
    const std::wstring &virtualFileName, const std::string &sourceUtf8,
    const std::wstring &entry, const std::wstring &profile,
    const std::vector<Define> &defines,
    const std::vector<std::wstring> &extraArgs) const {
    Result out{};

    if (!dxcUtils_ || !dxcCompiler_ || !includeHandler_) {
        return out;
    }

    // UTF-8 バッファを Blob に変換
    ComPtr<IDxcBlobEncoding> source;
    HRESULT hr = dxcUtils_->CreateBlob(sourceUtf8.data(),
        static_cast<UINT32>(sourceUtf8.size()),
        DXC_CP_UTF8, &source);
    if (FAILED(hr) || !source) {
        return out;
    }

    DxcBuffer buffer{};
    buffer.Ptr = source->GetBufferPointer();
    buffer.Size = source->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    auto args = BuildArguments(virtualFileName, entry, profile, defines, extraArgs);
    return DoCompile(buffer, args);
}

std::vector<LPCWSTR> ShaderCompiler::BuildArguments(
    const std::wstring &inputName, const std::wstring &entry,
    const std::wstring &profile, const std::vector<Define> &defines,
    const std::vector<std::wstring> &extraArgs) const {
    // DXC に渡す引数配列
    std::vector<LPCWSTR> args;

    // 入力ファイル名（仮想名でもOK）
    args.push_back(inputName.c_str());

    // -E Entry
    args.push_back(L"-E");
    args.push_back(entry.c_str());

    // -T Profile
    args.push_back(L"-T");
    args.push_back(profile.c_str());

    // デバッグ/最適化フラグ
    if (enableDebug_) {
        args.push_back(L"-Zi");
        args.push_back(L"-Qembed_debug");
        // 必要に応じて：args.push_back(L"-Qsource-embed"); // ソース埋め込み
    }

    switch (optLevel_) {
    default:
    case 0: args.push_back(L"-Od"); break;
    case 1: args.push_back(L"-O1"); break;
    case 2: args.push_back(L"-O2"); break;
    case 3: args.push_back(L"-O3"); break;
    }

    // 行優先レイアウト（packoffset の直感性向上）
    args.push_back(L"-Zpr");

    // （任意）言語バージョンを固定したい場合
    // args.push_back(L"-HV"); args.push_back(L"2021");

    // （任意）警告をエラー化したい場合
    // args.push_back(L"-WX");

    // Defines
    static thread_local std::vector<std::wstring> defineStorage;
    defineStorage.clear();
    defineStorage.reserve(defines.size());

    for (const auto &d : defines) {
        if (d.value.empty()) {
            defineStorage.push_back(L"-D" + d.name);
        } else {
            defineStorage.push_back(L"-D" + d.name + L"=" + d.value);
        }
        args.push_back(defineStorage.back().c_str());
    }

    // 追加引数（-I など）
    static thread_local std::vector<std::wstring> extraStorage;
    extraStorage.clear();
    extraStorage.reserve(extraArgs.size());

    for (const auto &a : extraArgs) {
        extraStorage.push_back(a);
        args.push_back(extraStorage.back().c_str());
    }

    // （任意）リリースでサイズを詰めたい場合
    // args.push_back(L"-Qstrip_debug");
    // args.push_back(L"-Qstrip_reflect");

    return args;
}

ShaderCompiler::Result ShaderCompiler::DoCompile(
    const DxcBuffer &buffer, std::vector<LPCWSTR> &args) const {
    Result out{};

    if (!dxcCompiler_)
        return out;

    ComPtr<IDxcResult> result;
    HRESULT hr = dxcCompiler_->Compile(
        &buffer, args.data(), static_cast<UINT32>(args.size()),
        includeHandler_.Get(), IID_PPV_ARGS(&result));

    if (FAILED(hr) || !result) {
        return out;
    }

    // ステータス確認（重要：エラーでも S_OK のことがある）
    HRESULT status = S_OK;
    (void)result->GetStatus(&status);

    // エラー/警告出力
    ComPtr<IDxcBlobUtf8> errors;
    ComPtr<IDxcBlobWide> dummy;
    hr = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), &dummy);
    if (SUCCEEDED(hr) && errors && errors->GetStringLength() > 0) {
        out.errors = errors;
    }

    // DXIL / CSO 本体
    ComPtr<IDxcBlob> shader;
    ComPtr<IDxcBlobWide> nameW;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), &nameW);
    if (SUCCEEDED(hr) && shader) {
        out.object = shader;
    }

    // PDB
    ComPtr<IDxcBlob> pdb;
    ComPtr<IDxcBlobWide> pdbNameW;
    hr = result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdbNameW);
    if (SUCCEEDED(hr) && pdb) {
        out.pdb = pdb;
        if (pdbNameW) {
            out.pdbName.assign(pdbNameW->GetStringPointer(),
                pdbNameW->GetStringLength());
        }
    }

    // 成否確定：GetStatus とオブジェクト有無の両方を見る
    out.succeeded = (SUCCEEDED(status) && out.object != nullptr);
    return out;
}
