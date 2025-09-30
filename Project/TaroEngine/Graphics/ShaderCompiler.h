#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <wrl.h>
#include <dxcapi.h>

/// <summary>
/// HLSL シェーダのコンパイルを行うユーティリティクラス。<br/>
/// DXC（IDxcCompiler3）を用いたファイル/文字列入力のコンパイル、
/// マクロ定義や最適化レベル、デバッグ情報の制御を提供する。
/// </summary>
class ShaderCompiler {
public:
	/// <summary>
	/// 事前定義マクロ（-DNAME=VALUE）を表すユニット。
	/// </summary>
	struct Define {
		std::wstring name;
		std::wstring value; // 空なら -DNAME だけを付与
	};

	/// <summary>
	/// コンパイル結果。成功/失敗に関係なくエラーログはここへ。
	/// </summary>
	struct Result {
		Microsoft::WRL::ComPtr<IDxcBlob>       object;     ///< DXC_OUT_OBJECT  (DXIL/CSO 等)
		Microsoft::WRL::ComPtr<IDxcBlob>       pdb;        ///< DXC_OUT_PDB     (存在すれば)
		std::wstring pdbName;    ///< DXC_OUT_PDB のファイル名（埋め込み名）
		Microsoft::WRL::ComPtr<IDxcBlobUtf8>   errors;     ///< DXC_OUT_ERRORS  (UTF-8)
		bool                   succeeded = false; ///< Compile 成否（GetStatus + object 取得で判定）
	};

public:
	ShaderCompiler() = default;
	~ShaderCompiler() = default;

	/// <summary>
	/// DXC ユーティリティ/コンパイラ/インクルードハンドラを初期化する。
	/// </summary>
	/// <returns>初期化に成功したら true。</returns>
	bool Initialize();

	/// <summary>
	/// デバッグ情報埋め込みの有効/無効を切り替える（既定:true）。
	/// </summary>
	void SetEnableDebug(bool enable) noexcept { enableDebug_ = enable; }

	/// <summary>
	/// 最適化レベルを設定する（0: -Od / 1: -O1 / 2: -O2 / 3: -O3）。
	/// </summary>
	void SetOptimizationLevel(int level) noexcept { optLevel_ = level; }

	/// <summary>
	/// HLSL ファイルからコンパイルする。
	/// </summary>
	/// <param name="filePath">HLSL ファイルパス</param>
	/// <param name="entry">エントリポイント（例: L"VSMain"）</param>
	/// <param name="profile">シェーダプロファイル（例: L"vs_6_0"）</param>
	/// <param name="defines">事前定義マクロ</param>
	/// <param name="extraArgs">追加引数（-I 等）</param>
	/// <returns>コンパイル結果</returns>
	Result CompileFromFile(const std::wstring &filePath,
		const std::wstring &entry,
		const std::wstring &profile,
		const std::vector<Define> &defines = {},
		const std::vector<std::wstring> &extraArgs = {}) const;

	/// <summary>
	/// メモリ上の UTF-8 ソース文字列からコンパイルする。
	/// </summary>
	/// <param name="virtualFileName">仮想ファイル名（エラーログ表示用）</param>
	/// <param name="sourceUtf8">UTF-8 ソース文字列</param>
	/// <param name="entry">エントリポイント</param>
	/// <param name="profile">シェーダプロファイル</param>
	/// <param name="defines">事前定義マクロ</param>
	/// <param name="extraArgs">追加引数（-I 等）</param>
	/// <returns>コンパイル結果</returns>
	Result CompileFromSource(const std::wstring &virtualFileName,
		const std::string &sourceUtf8,
		const std::wstring &entry,
		const std::wstring &profile,
		const std::vector<Define> &defines = {},
		const std::vector<std::wstring> &extraArgs = {}) const;

private:
	/// <summary>
	/// DXC に渡す引数配列（LPCWSTR の配列）を組み立てる。
	/// ※内部で thread_local な wstring バッファに文字を保持し、.c_str() の寿命を担保する。
	/// </summary>
	std::vector<LPCWSTR> BuildArguments(const std::wstring &inputName,
		const std::wstring &entry,
		const std::wstring &profile,
		const std::vector<Define> &defines,
		const std::vector<std::wstring> &extraArgs) const;

	/// <summary>
	/// 実際に IDxcCompiler3::Compile を叩き、Result を構築する。
	/// </summary>
	Result DoCompile(const DxcBuffer &buffer, std::vector<LPCWSTR> &args) const;

private:
	// DXC core
	Microsoft::WRL::ComPtr<IDxcUtils>      dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3>  dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

	// options
	bool enableDebug_ = true;
	int  optLevel_ = 0;   // 0..3
};
