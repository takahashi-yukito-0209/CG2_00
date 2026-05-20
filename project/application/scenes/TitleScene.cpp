#include "TitleScene.h"
#include <iostream>

using namespace MyEngine;

/// <summary>
/// コンストラクタ
/// </summary>
TitleScene::TitleScene() { }

/// <summary>
/// デストラクタ
/// </summary>
TitleScene::~TitleScene() { }

/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize(const SceneContext& ctx)
{
    // 初期化処理（サンプル）
    (void)ctx;
    std::cout << "TitleScene Initialize\n";
}

/// <summary>
/// 終了処理
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";
}

/// <summary>
/// 更新処理（引数は前のフレームからの経過時間）。ここでタイトルシーンのロゴのアニメーションや、ユーザー入力の処理などを行うことができる。
/// </summary>
void TitleScene::Update(float dt)
{
    // サンプル: 何もしない
    (void)dt;
}

/// <summary>
/// 描画処理。ここでタイトルシーンのロゴや背景などを描画することができる。
/// </summary>
void TitleScene::Draw()
{
    // サンプル: 何もしない
}

/// <summary>
/// シーンに入るときの処理。ここでシーンが切り替わったときの初期化や、BGMの再生などを行うことができる。
/// </summary>
void TitleScene::OnEnter()
{
    std::cout << "TitleScene OnEnter\n";
}

/// <summary>
/// シーンから出るときの処理。ここでシーンが切り替わる前のクリーンアップや、BGMの停止などを行うことができる。
/// </summary>
void TitleScene::OnExit()
{
    std::cout << "TitleScene OnExit\n";
}
