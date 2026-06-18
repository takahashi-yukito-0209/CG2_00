#include "TitleScene.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/base/SrvManager.h"
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

    // デモ用のレンダーターゲットとSRVの作成
    auto dx = DirectXCommon::GetInstance();
    if (dx) {
        rtHandle_ = dx->CreateRenderTarget(
            800, 600, dx->GetSwapChainFormat(),
            true, { 0.53f, 0.71f, 0.82f, 1.0f }, true);

        if (rtHandle_ >= 0 && ctx.srvManager) {
            rtSrvIndex_ = ctx.srvManager->Allocate();
            dx->CreateRenderTargetSRV(rtHandle_, rtSrvIndex_);
        }
        postProcess_.Initialize(dx);
    }
}

/// <summary>
/// 終了処理
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";
    // デモ用のレンダーターゲットとSRVの破棄
    auto dx = DirectXCommon::GetInstance();
    if (dx) {
        if (rtHandle_ >= 0) {
            dx->DestroyRenderTarget(rtHandle_);
            rtHandle_ = -1;
            rtSrvIndex_ = UINT32_MAX;
        }
    }
    postProcess_.Finalize();
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
    auto dx = DirectXCommon::GetInstance();
    if (!dx)
        return;

    // 描画前処理（SRVヒープの設定など）
    if (rtHandle_ >= 0) {
        // デモ用のレンダーターゲットに描画してみる
        dx->BeginRenderTo(rtHandle_, true);
        dx->EndRenderTo(rtHandle_);

        // ポストプロセスで描画してみる
        if (postProcess_.IsReady() && rtSrvIndex_ != UINT32_MAX) {
            postProcess_.DrawTexture(rtSrvIndex_);
        }
    }
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
