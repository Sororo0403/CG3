#include "ShaderCompiler.h"
#include "Logger/Logger.h"
#include "String/StringUtil.h"
#include <cassert>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

ShaderCompiler::ShaderCompiler() {
  LOG_INFO("ShaderCompiler: Initializing DXC compiler");

  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
  assert(SUCCEEDED(hr));

  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
  assert(SUCCEEDED(hr));

  hr = utils_->CreateDefaultIncludeHandler(&includeHandler_);
  assert(SUCCEEDED(hr));

  LOG_INFO("ShaderCompiler: DXC initialized successfully");
}

ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::wstring &filePath,
                                         const std::wstring &profile,
                                         const std::wstring &includeDir) {
  LOG_INFO(std::string("ShaderCompiler: Compile start → ") +
           StringUtil::ToUTF8(filePath));

  // ファイル読み込み
  LOG_DEBUG("ShaderCompiler: Loading shader file...");

  ComPtr<IDxcBlobEncoding> source = nullptr;
  HRESULT hr = utils_->LoadFile(filePath.c_str(), nullptr, &source);
  if (FAILED(hr)) {
    LOG_ERROR("ShaderCompiler: Failed to load file");
    assert(false);
  }

  DxcBuffer buffer{};
  buffer.Ptr = source->GetBufferPointer();
  buffer.Size = source->GetBufferSize();
  buffer.Encoding = DXC_CP_UTF8;

  LOG_DEBUG("ShaderCompiler: File loaded OK");

  // コンパイル引数
  LOG_DEBUG("ShaderCompiler: Setting compiler arguments...");

  std::vector<LPCWSTR> args = {
      filePath.c_str(), L"-E",  L"main", L"-T", profile.c_str(), L"-Zi",
      L"-Qembed_debug", L"-Od", L"-Zpr", L"-WX"};

  if (!includeDir.empty()) {
    args.push_back(L"-I");
    args.push_back(includeDir.c_str());

    LOG_DEBUG(std::string("ShaderCompiler: IncludeDir = ") +
              StringUtil::ToUTF8(includeDir));
  }

  // コンパイル
  LOG_INFO("ShaderCompiler: Compiling...");

  ComPtr<IDxcResult> result;
  hr = compiler_->Compile(&buffer, args.data(), (UINT32)args.size(),
                          includeHandler_.Get(), IID_PPV_ARGS(&result));

  if (FAILED(hr)) {
    LOG_ERROR("ShaderCompiler: Compile failed at DXC call");
    assert(false);
  }

  // エラー出力
  ComPtr<IDxcBlobUtf8> errors = nullptr;
  result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

  if (errors && errors->GetStringLength() > 0) {
    LOG_ERROR("ShaderCompiler: HLSL compile errors detected:");
    LOG_ERROR(errors->GetStringPointer());

    OutputDebugStringA("=== HLSL Compile Error ===\n");
    OutputDebugStringA(errors->GetStringPointer());
    OutputDebugStringA("\n==========================\n");

    assert(false && "Shader compile error");
  }

  LOG_INFO("ShaderCompiler: Compile completed (no errors)");

  // バイナリ取得
  ComPtr<IDxcBlob> shaderBlob = nullptr;
  hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
  if (FAILED(hr)) {
    LOG_ERROR("ShaderCompiler: Failed to retrieve shader object");
    assert(false);
  }

  LOG_INFO("ShaderCompiler: Shader object output OK");

  return shaderBlob;
}
