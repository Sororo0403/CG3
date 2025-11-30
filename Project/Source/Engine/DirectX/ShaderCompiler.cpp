#include "ShaderCompiler.h"
#include "LoggerManager.h"
#include "LogLevel.h"
#include <cassert>
#include <filesystem>

using Microsoft::WRL::ComPtr;

ShaderCompiler::~ShaderCompiler() {
	include_.Reset();
	compiler_.Reset();
	utils_.Reset();
}

void ShaderCompiler::Initialize(LoggerManager *loggerManager) {
	loggerManager_ = loggerManager;

	if (compiler_) return;

	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
	if (FAILED(hr) || !utils_) {
		loggerManager_->Log(LogLevel::ERR, L"[ShaderCompiler] DxcUtils の生成に失敗しました。");
		assert(false);
		return;
	}

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
	if (FAILED(hr) || !compiler_) {
		loggerManager_->Log(LogLevel::ERR, L"[ShaderCompiler] DxcCompiler の生成に失敗しました。");
		assert(false);
		return;
	}

	hr = utils_->CreateDefaultIncludeHandler(&include_);
	if (FAILED(hr) || !include_) {
		loggerManager_->Log(LogLevel::ERR, L"[ShaderCompiler] IncludeHandler の作成に失敗しました。");
		assert(false);
		return;
	}

	loggerManager_->Log(LogLevel::INFO, L"[ShaderCompiler] DXC を初期化しました。");
}

void ShaderCompiler::BuildArgs(
	std::vector<const wchar_t *> &out,
	const std::wstring &path,
	const std::wstring &entry,
	const std::wstring &target,
	const std::vector<std::wstring> &defs,
	bool optimize,
	bool debug) {
	out.push_back(path.c_str());
	out.push_back(L"-E"); out.push_back(entry.c_str());
	out.push_back(L"-T"); out.push_back(target.c_str());
	out.push_back(optimize ? L"-O3" : L"-O0");

	if (debug) {
		out.push_back(L"-Zi");
		out.push_back(L"-Qembed_debug");
	}

	const std::filesystem::path dir = std::filesystem::path(path).parent_path();
	if (!dir.empty()) {
		std::wstring includeDir = dir.wstring();
		out.push_back(L"-I");
		out.push_back(includeDir.c_str());
	}

	for (auto &d : defs) {
		out.push_back(L"-D"); out.push_back(d.c_str());
	}
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileFromFile(
	const std::wstring &path,
	const std::wstring &entry,
	const std::wstring &target,
	const std::vector<std::wstring> &defs,
	bool optimize,
	bool debug) {
	assert(compiler_ && utils_ && include_ && "ShaderCompiler not initialized");

	// ファイル読み込み
	ComPtr<IDxcBlobEncoding> src;
	HRESULT hr = utils_->LoadFile(path.c_str(), nullptr, &src);
	if (FAILED(hr) || !src) {
		loggerManager_->Log(LogLevel::ERR, L"[HLSL] ファイルの読み込みに失敗しました: " + path);
		return {};
	}

	DxcBuffer buf{};
	buf.Ptr = src->GetBufferPointer();
	buf.Size = src->GetBufferSize();
	buf.Encoding = DXC_CP_ACP;

	// 引数を構築
	std::vector<const wchar_t *> args;
	BuildArgs(args, path, entry, target, defs, optimize, debug);

	// コンパイル
	ComPtr<IDxcResult> result;
	hr = compiler_->Compile(&buf, args.data(), (UINT)args.size(), include_.Get(), IID_PPV_ARGS(&result));
	if (FAILED(hr) || !result) {
		loggerManager_->Log(LogLevel::ERR, L"[DXC] コンパイル呼び出しに失敗しました。");
		return {};
	}

	// エラーメッセージをその場で出力
	ComPtr<IDxcBlobUtf8> errors;
	ComPtr<IDxcBlobUtf16> dummyName;
	if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), dummyName.GetAddressOf())) &&
		errors && errors->GetStringLength() > 0) {
		const char *msgU8 = errors->GetStringPointer();
		int wlen = MultiByteToWideChar(CP_UTF8, 0, msgU8, -1, nullptr, 0);
		std::wstring wmsg(wlen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, msgU8, -1, wmsg.data(), wlen);
		loggerManager_->Log(LogLevel::WARN, wmsg);
	}

	// ステータスチェック
	HRESULT status = S_OK;
	hr = result->GetStatus(&status);
	if (FAILED(hr) || FAILED(status)) {
		loggerManager_->Log(LogLevel::ERR, L"[DXC] コンパイル失敗: " + path);
		return {};
	}

	// 出力オブジェクト取得
	ComPtr<IDxcBlob> blob;
	hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), dummyName.GetAddressOf());
	if (FAILED(hr) || !blob) {
		loggerManager_->Log(LogLevel::ERR, L"[DXC] 出力オブジェクト取得に失敗: " + path);
		return {};
	}

	loggerManager_->Log(LogLevel::INFO, L"[HLSL] 成功: " + path + L" (Entry=" + entry + L" / Target=" + target + L")");
	return blob;
}
