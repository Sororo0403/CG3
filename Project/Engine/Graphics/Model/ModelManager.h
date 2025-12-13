#pragma once

#include <unordered_map>
#include <cstdint>

#include "Model.h"
#include "ModelRenderState.h"
#include "Camera/Camera.h"

class MeshManager;
class ModelRenderer;

/// <summary>
/// Model の生成・管理・描画順制御・一括描画を行うマネージャ
/// </summary>
class ModelManager {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    ModelManager(ModelRenderer *renderer, MeshManager *meshManager);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~ModelManager() = default;

    /// <summary>
    /// モデルを作成
    /// </summary>
    /// <param name="meshId"></param>
    /// <returns></returns>
    uint32_t Create(uint32_t meshId);

    /// <summary>
    /// 指定した ID の Model を削除する。
    /// </summary>
    /// <param name="id">削除する Model の ID</param>
    void Destroy(uint32_t id);

    /// <summary>
    /// 登録されているすべての Model を削除する。
    /// </summary>
    void Clear();

    /// <summary>
    /// フレーム開始時に呼び出す。
    /// </summary>
    void Begin();

    /// <summary>
    /// 登録されている Model をレイヤー順に並べ、
    /// 可視なものだけを一括描画する。
    /// </summary>
    void DrawAll(const Camera *camera);

    // Setter
    void SetVisible(uint32_t id, bool visible);
    void SetLayer(uint32_t id, uint32_t layer);

    // Getter
    bool IsVisible(uint32_t id) const;
    uint32_t GetLayer(uint32_t id) const;
    Model *GetModel(uint32_t id);

    // Editor
    template <class Fn> void ForEach(Fn fn) {
        for (auto &[id, entry] : models_) {
            fn(id, entry.model, entry.render);
        }
    }

  private:
    struct Entry {
        Model model;
        ModelRenderState render;
    };

  private:
    ModelRenderer *renderer_ = nullptr;
    MeshManager *meshManager_ = nullptr;

    std::unordered_map<uint32_t, Entry> models_;
    uint32_t nextId_ = 1;
};
