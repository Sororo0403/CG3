#pragma once

#define NOMINMAX
#include <unknwn.h>
#include <Windows.h>         
#include <d3d12.h>
#include <wrl.h>
#include <dxcapi.h>
#include <string>
#include <vector>

class LoggerManager;

class ShaderCompiler {
public:
	/// <summary>
	/// デストラクタ。
	/// </summary>
	~ShaderCompiler();

	/// <summary>
	/// 初期化処理。
	/// </summary>
	/// <param name="loggerManager">ログマネージャー</param>
	void Initialize(LoggerManager *loggerManager);

	/// <summary>
	/// HLSLをコンパイルして、DXILバイナリ(IDxcBlob)を返す
	/// </summary>
	/// <param name="path">.hlsl のパス</param>
	/// <param name="entry">エントリポイント名 (例: L"main")</param>
	/// <param name="target">ターゲット (例: L"vs_6_8", L"ps_6_8")</param>
	/// <param name="defs">マクロ定義 (例: {L"USE_FOG=1"})</param>
	/// <param name="optimize">最適化フラグ</param>
	/// <param name="debug">デバッグ情報埋め込み</param>
	Microsoft::WRL::ComPtr<IDxcBlob> CompileFromFile(
		const std::wstring &path,
		const std::wstring &entry,
		const std::wstring &target,
		const std::vector<std::wstring> &defs = {},
		bool optimize = true,
		bool debug = false);

private:
	// DXC
	Microsoft::WRL::ComPtr<IDxcUtils> utils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_;

	// ログマネージャー
	LoggerManager *loggerManager_ = nullptr;

	/// <summary>
	/// HLSLコンパイル時にDXCへ渡すコマンドライン引数を構築します。
	/// </summary>
	/// <param name="out">生成された引数を格納する配列。</param>
	/// <param name="path">コンパイル対象のHLSLファイルパス。</param>
	/// <param name="entry">エントリポイント名 (例: L"main")。</param>
	/// <param name="target">シェーダターゲット (例: L"vs_6_8" や L"ps_6_8")。</param>
	/// <param name="defs">コンパイル時に定義するマクロ一覧 (例: {L"USE_FOG=1"})。</param>
	/// <param name="optimize">最適化を有効にする場合はtrue。無効の場合はfalse。</param>
	/// <param name="debug">デバッグ情報を埋め込む場合はtrue。</param>
	void BuildArgs(
		std::vector<const wchar_t *> &out,
		const std::wstring &path,
		const std::wstring &entry,
		const std::wstring &target,
		const std::vector<std::wstring> &defs,
		bool optimize, 
		bool debug);
};
