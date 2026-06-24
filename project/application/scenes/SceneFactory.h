#pragma once
#include "../../engine/base/IScene.h"
#include <memory>
#include <string>

namespace GameApp {

/// <summary>
/// シーン名から対応するシーンを生成するファクトリクラス
/// </summary>
class SceneFactory {
public:
    /// <summary>
    /// 指定されたシーン名に対応するシーンを生成する
    /// </summary>
    static std::unique_ptr<MyEngine::IScene> Create(const std::string& name);
};

} // namespace GameApp
