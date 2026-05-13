#pragma once
#include "../../engine/base/IScene.h"

#include <array>
#include <memory>
#include <vector>

// 前方宣言
namespace MyEngine {
class Sprite;
class Object3d;
class SpriteCommon;
class Object3dCommon;
class ParticleManager;
class TextureManager;
class Camera;
class SkyBox;
}

#include "../../engine/particle/ParticleEmitter.h"

using namespace MyEngine;

/// <summary>
/// プレイシーンのクラス。IScene インターフェースを実装して、ゲームのプレイ中のシーンを表す。
/// </summary>
class PlayScene : public IScene {
public: // メンバ関数

    /// <summary>
    /// コンストラクタ
    /// </summary>
    PlayScene();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PlayScene();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(const SceneContext& ctx) override;

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
    ///　シーンに入るときの処理
    /// </summary>
    void OnEnter() override;

    /// <summary>
    /// シーンから出る時の処理
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// 描画モードの更新を受け取る
    /// </summary>
    void SetSelectedDrawType(int t) override;

    /// <summary>
    /// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
    /// </summary>
    void FillObject3dPointers(std::vector<Object3d*>* out) override;

    /// <summary>
    /// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
    /// </summary>
    void FillSpritePointers(std::vector<Sprite*>* out) override;

    /// <summary>
    /// シーンの名前を取得
    /// </summary>
    std::string GetName() const override { return "Play"; }

private://メンバ変数

    SceneContext ctx_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::vector<std::unique_ptr<Object3d>> objects3d_;
    std::unique_ptr<Object3d> particlePlane_;
    ParticleEmitter pmEmitter_;
    std::unique_ptr<MyEngine::SkyBox> skybox_;
};
