#pragma once
#include "../../engine/base/IScene.h"

using namespace MyEngine;

/// <summary>
/// タイトルシーンのクラス。IScene インターフェースを実装して、ゲームのタイトル画面のシーンを表す。
/// </summary>
class TitleScene : public IScene {
public: // メンバ関数

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
    /// シーンに入るときの処理
    /// </summary>
    void OnEnter() override;


    /// <summary>
    /// シーンから出るときの処理
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// シーンの名前を取得
    /// </summary>
    std::string GetName() const override { return "Title"; }
};
