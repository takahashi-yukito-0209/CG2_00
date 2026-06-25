#pragma once
#include "../../engine/base/IScene.h"

#include <memory>
#include <vector>

namespace MyEngine {
class Object3d;
class Sprite;
}

/// <summary>
/// タイトルシーンを管理するクラス
/// </summary>
class TitleScene : public MyEngine::IScene {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    TitleScene();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~TitleScene();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(const MyEngine::SceneContext& ctx) override;

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update(float dt) override;

    /// <summary>
    /// 描画処理
    /// </summary>
    void Draw() override;

    /// <summary>
    /// シーンに入るときの処理
    /// </summary>
    void OnEnter() override;

    /// <summary>
    /// シーンから出るときの処理
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// 描画モードの更新を受け取る
    /// </summary>
    void SetSelectedDrawType(int type) override;

    /// <summary>
    /// シーンが所有する3Dオブジェクトのポインタを収集する
    /// </summary>
    void FillObject3dPointers(std::vector<MyEngine::Object3d*>* out) override;

    /// <summary>
    /// シーンが所有するスプライトのポインタを収集する
    /// </summary>
    void FillSpritePointers(std::vector<MyEngine::Sprite*>* out) override;

    /// <summary>
    /// シーン名を取得する
    /// </summary>
    std::string GetName() const override { return "Title"; }

private:
    /// <summary>
    /// タイトル用の3Dオブジェクトを描画する
    /// </summary>
    void DrawWorldObjects();

private:
    MyEngine::SceneContext ctx_; // シーンへ渡された共通コンテキスト
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects3d_; // タイトル表示用3Dオブジェクト
    std::vector<std::unique_ptr<MyEngine::Sprite>> sprites_; // タイトル表示用スプライト
};
