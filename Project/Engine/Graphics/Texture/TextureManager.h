#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include <d3d12.h>
#include <wrl.h>

#include "DirectXTex/DirectXTex.h"
#include "Texture.h"

class DirectXCommon;

/// <summary>
/// DirectXTex と DirectX 12 を用いてテクスチャを管理するクラス。
/// シングルトンを使用せず、DirectXCommon を依存注入（DI）によって受け取る設計。
/// </summary>
class TextureManager {
  public:
    /// <summary>
    /// コンストラクタ。
    /// 使用する DirectXCommon を外部から注入する。
    /// </summary>
    /// <param name="dx">DirectXCommon のインスタンス</param>
    TextureManager(DirectXCommon *dx);

    /// <summary>
    /// デストラクタ。
    /// 管理しているテクスチャリソースを解放する。
    /// </summary>
    ~TextureManager();

    /// <summary>
    /// 初期化処理。
    /// テクスチャ管理用テーブルのリセットなどを行う。
    /// </summary>
    void Initialize();

    /// <summary>
    /// テクスチャを読み込み、専用のテクスチャ ID を返す。
    /// 同じパスの画像は再利用され、重複読み込みは行われない。
    /// </summary>
    /// <param name="filePath">画像ファイルのパス（UTF-8 文字列）</param>
    /// <returns>割り当てられたテクスチャ ID</returns>
    uint32_t LoadTexture(const std::string &filePath);

    /// <summary>
    /// 登録済みテクスチャ情報を取得する。
    /// </summary>
    /// <param name="id">テクスチャ ID</param>
    /// <returns>テクスチャ情報（GPU ハンドルとリソース）</returns>
    const Texture &GetTexture(uint32_t id) const;

  private:
    /// <summary>
    /// DirectXTex により画像ファイルを読み込み、ミップマップ生成を行う。
    /// </summary>
    /// <param name="filePath">画像ファイルのパス</param>
    /// <returns>ミップレベルを含む ScratchImage</returns>
    DirectX::ScratchImage LoadTextureFromFile(const std::string &filePath);

    /// <summary>
    /// DirectX 12 上にテクスチャリソースを確保する。
    /// </summary>
    /// <param name="device">D3D12 デバイス</param>
    /// <param name="metadata">読み込んだ画像のメタデータ</param>
    /// <returns>作成されたテクスチャリソース</returns>
    Microsoft::WRL::ComPtr<ID3D12Resource>
    CreateTextureResource(ID3D12Device *device,
                          const DirectX::TexMetadata &metadata);

    /// <summary>
    /// テクスチャの全ミップレベルに対して GPU へデータをアップロードする。
    /// </summary>
    /// <param name="texture">アップロード先の D3D12 リソース</param>
    /// <param name="mipImages">ミップレベルを含む ScratchImage</param>
    void UploadTextureData(ID3D12Resource *texture,
                           const DirectX::ScratchImage &mipImages);

  private:
    DirectXCommon *dx_ = nullptr;
    uint32_t nextIndex_ = 1;

    std::unordered_map<std::string, uint32_t> pathToId_;
    std::unordered_map<uint32_t, Texture> textures_;
};
