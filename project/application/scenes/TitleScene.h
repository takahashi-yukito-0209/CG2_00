#pragma once

#include "../../engine/base/IScene.h"
#include "../../engine/base/PostProcess.h"

#include <memory>
#include <vector>

namespace MyEngine {
class Object3d;
}

using namespace MyEngine;

/// <summary>
/// タイトル画面を管理するシーンクラス
/// </summary>
class TitleScene : public IScene {
public:
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    TitleScene();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~TitleScene();

    /// <summary>
    /// タイトルシーンを初期化する
    /// </summary>
    void Initialize(const SceneContext& ctx) override;

    /// <summary>
    /// タイトルシーンが保持するリソースを解放する
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// タイトルシーンの状態を更新する
    /// </summary>
    void Update(float dt) override;

    /// <summary>
    /// タイトルシーンを描画する
    /// </summary>
    void Draw() override;

    /// <summary>
    /// タイトルシーンへ入ったときの処理を行う
    /// </summary>
    void OnEnter() override;

    /// <summary>
    /// タイトルシーンから出るときの処理を行う
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// シーン名を取得する
    /// </summary>
    std::string GetName() const override { return "Title"; }

    /// <summary>
    /// タイトルシーンが使用しているポストプロセスを取得する
    /// </summary>
    PostProcess* GetPostProcess() override { return &postProcess_; }

    /// <summary>
    /// タイトルシーンが所有する3DオブジェクトをImGuiへ渡す
    /// </summary>
    void FillObject3dPointers(std::vector<Object3d*>* out) override;

private:
    SceneContext ctx_; // タイトルシーンで使用する共通リソース
    std::unique_ptr<Object3d> terrain_; // タイトル画面に表示する地形
    int rtHandle_ = -1; // オフスクリーン用レンダーターゲットハンドル
    uint32_t rtSrvIndex_ = UINT32_MAX; // オフスクリーンテクスチャのSRV番号
    uint32_t rtDepthSrvIndex_ = UINT32_MAX; // オフスクリーン深度のSRV番号
    int gaussianRtHandle_ = -1; // Gaussian Filterの横方向処理結果
    uint32_t gaussianSrvIndex_ = UINT32_MAX; // Gaussian中間テクスチャのSRV番号
    uint32_t dissolveMaskSrvIndex_ = UINT32_MAX; // Dissolve用ノイズマスクのSRV番号
    PostProcess postProcess_; // タイトル画面に適用するポストプロセス
};
