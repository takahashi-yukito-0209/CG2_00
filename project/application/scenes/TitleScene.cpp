#include "TitleScene.h"

#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/base/SrvManager.h"
#include "../../engine/2d/TextureManager.h"

#include <iostream>

using namespace MyEngine;

/// <summary>
/// デフォルトコンストラクタ
/// </summary>
TitleScene::TitleScene() { }

/// <summary>
/// デストラクタ
/// </summary>
TitleScene::~TitleScene() { }

/// <summary>
/// タイトルシーンを初期化する
/// </summary>
void TitleScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx; // シーン共通リソースを保持する
    std::cout << "TitleScene Initialize\n";

    if (ctx_.object3dCommon) {
        terrain_ = std::make_unique<Object3d>(); // タイトル表示用の地形
        terrain_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        terrain_->SetModel("terrain/terrain.obj");
        terrain_->SetScale({ 5.0f, 5.0f, 5.0f });
    }

    DirectXCommon* dxCommon =
        DirectXCommon::GetInstance(); // オフスクリーン描画に使用するDirectX基盤
    if (!dxCommon) {
        return;
    }

    rtHandle_ = dxCommon->CreateRenderTarget(
        800,
        600,
        dxCommon->GetSwapChainFormat(),
        true,
        { 0.53f, 0.71f, 0.82f, 1.0f },
        true);

    if (rtHandle_ >= 0 && ctx_.srvManager) {
        rtSrvIndex_ = ctx_.srvManager->Allocate();
        dxCommon->CreateRenderTargetSRV(rtHandle_, rtSrvIndex_);
        rtDepthSrvIndex_ = ctx_.srvManager->Allocate();
        dxCommon->CreateRenderTargetDepthSRV(
            rtHandle_,
            rtDepthSrvIndex_);
    }

    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture("noise0.png");
        ctx_.textureManager->ExecuteResourceUpload();
        dissolveMaskSrvIndex_ =
            ctx_.textureManager->GetSrvIndex("noise0.png");
    }

    gaussianRtHandle_ = dxCommon->CreateRenderTarget(
        800,
        600,
        dxCommon->GetSwapChainFormat(),
        false,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        true);

    if (gaussianRtHandle_ >= 0 && ctx_.srvManager) {
        gaussianSrvIndex_ = ctx_.srvManager->Allocate();
        dxCommon->CreateRenderTargetSRV(
            gaussianRtHandle_,
            gaussianSrvIndex_);
    }

    postProcess_.Initialize(dxCommon);
}

/// <summary>
/// タイトルシーンが保持するリソースを解放する
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";

    terrain_.reset();

    DirectXCommon* dxCommon =
        DirectXCommon::GetInstance(); // レンダーターゲットを管理するDirectX基盤
    if (dxCommon && rtHandle_ >= 0) {
        dxCommon->DestroyRenderTarget(rtHandle_);
        rtHandle_ = -1;
        rtSrvIndex_ = UINT32_MAX;
        rtDepthSrvIndex_ = UINT32_MAX;
    }

    if (dxCommon && gaussianRtHandle_ >= 0) {
        dxCommon->DestroyRenderTarget(gaussianRtHandle_);
        gaussianRtHandle_ = -1;
        gaussianSrvIndex_ = UINT32_MAX;
    }

    postProcess_.Finalize();
    dissolveMaskSrvIndex_ = UINT32_MAX;
    ctx_ = {};
}

/// <summary>
/// タイトルシーンの状態を更新する
/// </summary>
void TitleScene::Update(float dt)
{
    postProcess_.Update(dt);

    if (ctx_.camera) {
        ctx_.camera->Update();
    }

    if (terrain_ && ctx_.camera) {
        terrain_->Update(
            ctx_.camera->GetViewMatrix(),
            ctx_.camera->GetProjectionMatrix());
    }
}

/// <summary>
/// タイトルシーンを描画する
/// </summary>
void TitleScene::Draw()
{
    DirectXCommon* dxCommon =
        DirectXCommon::GetInstance(); // 描画先を切り替えるDirectX基盤
    if (!dxCommon || rtHandle_ < 0) {
        return;
    }

    dxCommon->BeginRenderTo(rtHandle_, true);

    if (terrain_ && ctx_.object3dCommon) {
        ctx_.object3dCommon->SetCommonDrawSetting();
        terrain_->Draw();
    }

    dxCommon->EndRenderTo(rtHandle_);

    if (postProcess_.IsReady() && rtSrvIndex_ != UINT32_MAX) {
        if (postProcess_.GetEffectType() == PostEffectType::GaussianFilter
            && gaussianRtHandle_ >= 0
            && gaussianSrvIndex_ != UINT32_MAX) {
            postProcess_.DrawGaussianTexture(
                rtSrvIndex_,
                gaussianRtHandle_,
                gaussianSrvIndex_);
        } else if (
            postProcess_.GetEffectType() == PostEffectType::DepthOutline
            && rtDepthSrvIndex_ != UINT32_MAX
            && ctx_.camera) {
            postProcess_.DrawDepthOutline(
                rtSrvIndex_,
                rtDepthSrvIndex_,
                ctx_.camera->GetProjectionMatrix());
        } else if (
            postProcess_.GetEffectType() == PostEffectType::Dissolve
            && dissolveMaskSrvIndex_ != UINT32_MAX) {
            postProcess_.DrawDissolveTexture(
                rtSrvIndex_,
                dissolveMaskSrvIndex_);
        } else {
            postProcess_.DrawTexture(rtSrvIndex_);
        }
    }
}

/// <summary>
/// タイトルシーンへ入ったときの処理を行う
/// </summary>
void TitleScene::OnEnter()
{
    std::cout << "TitleScene OnEnter\n";
}

/// <summary>
/// タイトルシーンから出るときの処理を行う
/// </summary>
void TitleScene::OnExit()
{
    std::cout << "TitleScene OnExit\n";
}

/// <summary>
/// タイトルシーンが所有する3DオブジェクトをImGuiへ渡す
/// </summary>
void TitleScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) {
        return;
    }

    out->clear();
    if (terrain_) {
        out->push_back(terrain_.get());
    }
}
