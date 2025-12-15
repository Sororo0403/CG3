#pragma once
#include <memory>

class Scene;

/// <summary>
/// シーン管理クラス
/// </summary>
class SceneManager {
  public:
    void ChangeScene(std::unique_ptr<Scene> scene);
    void Update();
    void Draw();

  private:
    std::unique_ptr<Scene> currentScene_;
};
