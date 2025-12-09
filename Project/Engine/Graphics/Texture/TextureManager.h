#pragma once
#include "DirectXTex/DirectXTex.h"
#include "Texture.h"
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class DirectXCommon;

class TextureManager {
public:
  /// <summary>
  /// 唯一のインスタンス取得
  /// </summary>
  static TextureManager *GetInstance();

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~TextureManager();

  /// <summary>
  /// 初期化処理
  /// </summary>
  /// <param name="dx">DirectXCommonのポインタ</param>
  void Initialize(DirectXCommon *dx);

  /// <summary>
  /// テクスチャをロードして ID を返す（同一パスは再利用）
  /// </summary>
  uint32_t LoadTexture(const std::string &filePath);

  /// <summary>
  /// テクスチャ情報取得
  /// </summary>
  const Texture &GetTexture(uint32_t id) const;

private:
  // シングルトン
  TextureManager() = default;
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;

  /// <summary>
  /// DirectXTex を用いてミップマップ付きの ScratchImage を生成
  /// </summary>
  /// <param name="filePath">読み込む画像ファイルのパス</param>
  /// <returns>ミップマップを含む ScratchImage オブジェクト</returns>
  DirectX::ScratchImage LoadTextureFromFile(const std::string &filePath);

  /// <summary>
  /// GPU 上にテクスチャ用の ID3D12Resource を生成
  /// </summary>
  /// <param name="device">D3D12 デバイス</param>
  /// <param name="metadata">DirectXTex が生成したテクスチャのメタ情報</param>
  /// <returns>作成されたテクスチャリソース</returns>
  Microsoft::WRL::ComPtr<ID3D12Resource>
  CreateTextureResource(ID3D12Device *device,
                        const DirectX::TexMetadata &metadata);

  /// <summary>
  /// すべてのミップレベルのピクセルデータを書き込みます
  /// </summary>
  /// <param name="texture">書き込み対象の ID3D12Resource（テクスチャ）</param>
  /// <param name="mipImages">各ミップレベルを含む ScratchImage</param>
  void UploadTextureData(ID3D12Resource *texture,
                         const DirectX::ScratchImage &mipImages);

private:
  DirectXCommon *dx_ = nullptr;
  uint32_t nextIndex_ = 1;

  std::unordered_map<std::string, uint32_t> pathToId_;
  std::unordered_map<uint32_t, Texture> textures_;
};
