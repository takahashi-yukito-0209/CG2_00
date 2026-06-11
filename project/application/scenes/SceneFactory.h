#pragma once
#include "../../engine/base/IScene.h"
#include <memory>
#include <string>

namespace GameApp {

using namespace MyEngine;

/// <summary>
/// シーンファクトリークラス: シーンの名前からシーンオブジェクトを生成するためのクラス
/// </summary>
class SceneFactory {
public: // メンバ関数
    // シンプルに名前からシーンを生成する
    static std::unique_ptr<IScene> Create(const std::string& name);
};

} // namespace GameApp
