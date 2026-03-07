#include "SceneFactory.h"
#include "TitleScene.h"
#include "PlayScene.h"

using namespace GameApp;
using namespace MyEngine;

/// <summary>
/// シーンの名前からシーンオブジェクトを生成する。ここで新しいシーンを追加していく。
/// </summary>
std::unique_ptr<IScene> SceneFactory::Create(const std::string& name) {

    // シーンの名前に応じて対応するシーンオブジェクトを生成して返す
    if (name == "Title") {
        return std::unique_ptr<IScene>(new TitleScene());
    } else if (name == "Play") {
        return std::unique_ptr<IScene>(new PlayScene());
    }

    // 存在しない名前の場合は nullptr を返す
    return nullptr;
}
