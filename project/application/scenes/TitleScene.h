#pragma once
#include "../../engine/base/IScene.h"
#include "../../engine/base/RenderTarget.h"

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
    /// Scene View用のオフスクリーン描画だけにするか設定する。
    /// </summary>
    void SetSceneViewOnly(bool enabled) override;

    /// <summary>
    /// Scene Viewに表示するSRV番号を取得する。
    /// </summary>
    uint32_t GetSceneViewSrvIndex() const override;

    /// <summary>
    /// ウィンドウリサイズ時にScene View用RenderTargetを追従させる。
    /// </summary>
    void OnWindowResize(uint32_t width, uint32_t height) override;

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

    /// <summary>
    /// 所有中の3Dオブジェクトから参照用ビューを作り直す。
    /// </summary>
    void RebuildObjectPointerView();

private:
    MyEngine::SceneContext ctx_; // シーンへ渡された共通コンテキスト
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects3d_; // タイトル表示用3Dオブジェクト
    std::vector<MyEngine::Object3d*> objectPointerView_; // ImGuiなど外部参照用の3Dオブジェクト一覧
    std::vector<std::unique_ptr<MyEngine::Sprite>> sprites_; // タイトル表示用スプライト
    std::unique_ptr<MyEngine::Object3d> particlePlane_; // GPUパーティクル確認用の描画平面
    MyEngine::RenderTarget sceneRenderTarget_; // Scene Viewに表示するタイトル描画結果
    bool sceneViewOnly_ = false; // Scene View用RTだけへ描画するか
};
