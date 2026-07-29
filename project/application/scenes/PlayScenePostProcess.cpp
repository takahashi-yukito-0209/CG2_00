#include "PlayScene.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/base/WinApp.h"

#include <array>

using namespace MyEngine;

namespace {
constexpr int kGaussianFirstPassIndex = 0; // Gaussian Filterの横方向pass番号
constexpr int kGaussianSecondPassIndex = 1; // Gaussian Filterの縦方向pass番号
constexpr std::array<float, 4> kSceneRenderTargetClearColor = { 0.53f, 0.71f, 0.82f, 1.0f }; // シーン描画RTのクリア色
constexpr std::array<float, 4> kTransparentRenderTargetClearColor = { 0.0f, 0.0f, 0.0f, 1.0f }; // 中間RTと最終RTのクリア色
constexpr bool kUseDepthBuffer = true; // RenderTargetに深度バッファを作成する
constexpr bool kNoDepthBuffer = false; // RenderTargetに深度バッファを作成しない
constexpr bool kCreateDepthSrv = true; // 深度SRVを作成する
constexpr bool kNoDepthSrv = false; // 深度SRVを作成しない
constexpr const char* kDissolveMaskTextureName = "noise0.png"; // Dissolveに使用するノイズマスク名

/// <summary>
/// ポストプロセス用のRenderTarget設定を作成する
/// </summary>
RenderTargetDesc CreatePostProcessRenderTargetDesc(
    DXGI_FORMAT format,
    bool useDepth,
    bool createDepthSrv,
    const std::array<float, 4>& clearColor)
{
    RenderTargetDesc desc {}; // 作成するRenderTarget設定
    desc.width = WinApp::kWindowWidth;
    desc.height = WinApp::kWindowHeight;
    desc.format = format;
    desc.useDepth = useDepth;
    desc.createColorSrv = true;
    desc.createDepthSrv = createDepthSrv;
    desc.resizeWithWindow = true;
    desc.clearColor = clearColor;
    return desc;
}
}

/// <summary>
/// ポストプロセス用レンダーターゲットを初期化する
/// </summary>
void PlayScene::InitializePostProcessTargets()
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 初期化に使用するDirectX基盤
    if (!directXCommon) {
        return;
    }

    const DXGI_FORMAT renderTargetFormat = directXCommon->GetSwapChainFormat(); // 各RTで使用するカラーフォーマット
    const RenderTargetDesc sceneRenderTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kUseDepthBuffer,
        kCreateDepthSrv,
        kSceneRenderTargetClearColor); // シーン描画用RT設定
    sceneRenderTarget_.Initialize(directXCommon, sceneRenderTargetDesc);

    const RenderTargetDesc intermediateTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kNoDepthBuffer,
        kNoDepthSrv,
        kTransparentRenderTargetClearColor); // ポストプロセス中間RT設定
    postProcessIntermediateTarget_.Initialize(directXCommon, intermediateTargetDesc);

    const RenderTargetDesc finalRenderTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kNoDepthBuffer,
        kNoDepthSrv,
        kTransparentRenderTargetClearColor); // Scene View表示用RT設定
    finalRenderTarget_.Initialize(directXCommon, finalRenderTargetDesc);

    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture(kDissolveMaskTextureName);
        dissolveMaskSrvIndex_ = ctx_.textureManager->GetSrvIndex(kDissolveMaskTextureName);
    }

    postProcess_.Initialize(directXCommon);
    postProcess_.SetEffectType(PostEffectType::Copy);
}

/// <summary>
/// ポストプロセス用リソースを解放する。
/// </summary>
void PlayScene::FinalizePostProcessTargets()
{
    sceneRenderTarget_.Finalize();
    postProcessIntermediateTarget_.Finalize();
    finalRenderTarget_.Finalize();
    dissolveMaskSrvIndex_ = UINT32_MAX;
    postProcess_.Finalize();
}

/// <summary>
/// Scene View用のオフスクリーン描画だけにするか設定する
/// </summary>
void PlayScene::SetSceneViewOnly(bool enabled)
{
    sceneViewOnly_ = enabled;
}

/// <summary>
/// Scene Viewへ表示するSRV番号を取得する。
/// </summary>
uint32_t PlayScene::GetSceneViewSrvIndex() const
{
    if (finalRenderTarget_.HasColorSrv()) {
        return finalRenderTarget_.GetColorSrvIndex();
    }
    if (sceneRenderTarget_.HasColorSrv()) {
        return sceneRenderTarget_.GetColorSrvIndex();
    }
    return UINT32_MAX;
}

/// <summary>
/// シーンが使用しているポストプロセスを取得する
/// </summary>
PostProcess* PlayScene::GetPostProcess()
{
    return &postProcess_;
}
/// <summary>
/// ポストプロセス描画が利用できるか判定する
/// </summary>
bool PlayScene::CanUsePostProcess() const
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 描画に使用するDirectX基盤
    return directXCommon
        && sceneRenderTarget_.IsValid()
        && sceneRenderTarget_.HasColorSrv()
        && postProcess_.IsReady();
}

/// <summary>
/// シーン描画結果をポストプロセス入力用RTへ描画する
/// </summary>
void PlayScene::DrawSceneToPostProcessTarget()
{
    sceneRenderTarget_.Begin(true);
    DrawSceneContent();
    DrawDebugLines3D();
    sceneRenderTarget_.End();
}

/// <summary>
/// 時間演出用のポストプロセス連鎖を適用する
/// </summary>
void PlayScene::ApplyTemporalPostProcessChain(uint32_t& postProcessSourceSrvIndex, PostEffectType& finalEffectType)
{
    const bool canUseTemporalChain = temporalRiftEffect_.IsPostProcessChainPhase()
        && postProcessIntermediateTarget_.IsValid()
        && postProcessIntermediateTarget_.HasColorSrv(); // 2pass目に渡すRTが必要
    if (!canUseTemporalChain) {
        return;
    }

    const uint32_t sceneColorSrvIndex = sceneRenderTarget_.GetColorSrvIndex(); // 時間演出1pass目の入力SRV
    postProcessIntermediateTarget_.Begin(true);
    postProcess_.DrawTexture(sceneColorSrvIndex, PostEffectType::Distortion);
    postProcessIntermediateTarget_.End();

    postProcessSourceSrvIndex = postProcessIntermediateTarget_.GetColorSrvIndex();
    finalEffectType = PostEffectType::RadialBlur;
}

/// <summary>
/// Gaussian Filterの2pass描画が利用できるか判定する
/// </summary>
bool PlayScene::CanUseGaussianFilter(PostEffectType finalEffectType) const
{
    return finalEffectType == PostEffectType::GaussianFilter
        && postProcessIntermediateTarget_.IsValid()
        && postProcessIntermediateTarget_.HasColorSrv();
}

/// <summary>
/// Scene View表示用の最終RTが利用できるか判定する
/// </summary>
bool PlayScene::CanUseFinalRenderTarget() const
{
    return sceneViewOnly_
        && finalRenderTarget_.IsValid()
        && finalRenderTarget_.HasColorSrv();
}

/// <summary>
/// Gaussian Filterの1pass目を中間RTへ描画する
/// </summary>
void PlayScene::ApplyGaussianFirstPass(uint32_t& postProcessSourceSrvIndex)
{
    postProcessIntermediateTarget_.Begin(true);
    postProcess_.DrawGaussianPass(postProcessSourceSrvIndex, kGaussianFirstPassIndex);
    postProcessIntermediateTarget_.End();
    postProcessSourceSrvIndex = postProcessIntermediateTarget_.GetColorSrvIndex();
}

/// <summary>
/// 最終ポストプロセス描画を実行する
/// </summary>
void PlayScene::DrawFinalPostProcessPass(uint32_t postProcessSourceSrvIndex, PostEffectType finalEffectType, bool useGaussianFilter)
{
    if (useGaussianFilter) {
        postProcess_.DrawGaussianPass(postProcessSourceSrvIndex, kGaussianSecondPassIndex);
    } else if (finalEffectType == PostEffectType::DepthOutline && sceneRenderTarget_.HasDepthSrv() && ctx_.camera) {
        const uint32_t sceneDepthSrvIndex = sceneRenderTarget_.GetDepthSrvIndex(); // Depth Outlineで参照する深度SRV
        postProcess_.DrawDepthOutline(postProcessSourceSrvIndex, sceneDepthSrvIndex, ctx_.camera->GetProjectionMatrix());
    } else if (finalEffectType == PostEffectType::Dissolve && dissolveMaskSrvIndex_ != UINT32_MAX) {
        postProcess_.DrawDissolveTexture(postProcessSourceSrvIndex, dissolveMaskSrvIndex_);
    } else {
        postProcess_.DrawTexture(postProcessSourceSrvIndex, finalEffectType);
    }
}

/// <summary>
/// 最終描画に必要なポストプロセス状態を作成する
/// </summary>
PlayScene::PostProcessDrawContext PlayScene::BuildPostProcessDrawContext()
{
    PostProcessDrawContext drawContext {}; // ポストプロセス描画で共有する状態
    drawContext.sourceSrvIndex = sceneRenderTarget_.GetColorSrvIndex();
    drawContext.finalEffectType = postProcess_.GetEffectType();

    ApplyPostProcessPrePasses(drawContext);
    drawContext.useFinalRenderTarget = CanUseFinalRenderTarget();
    return drawContext;
}

/// <summary>
/// 最終描画前に必要なポストプロセスの前段パスを適用する。
/// </summary>
void PlayScene::ApplyPostProcessPrePasses(PostProcessDrawContext& drawContext)
{
    ApplyTemporalPostProcessChain(drawContext.sourceSrvIndex, drawContext.finalEffectType);

    drawContext.useGaussianFilter = CanUseGaussianFilter(drawContext.finalEffectType);
    if (drawContext.useGaussianFilter) {
        ApplyGaussianFirstPass(drawContext.sourceSrvIndex);
    }
}

/// <summary>
/// Scene View用RTが必要な場合だけ描画先を切り替える
/// </summary>
void PlayScene::BeginSceneViewRenderTargetIfNeeded(bool useFinalRenderTarget)
{
    if (!useFinalRenderTarget) {
        return;
    }

    finalRenderTarget_.Begin(true);
}

/// <summary>
/// Scene View用RTへ描画していた場合だけ描画先を戻す
/// </summary>
void PlayScene::EndSceneViewRenderTargetIfNeeded(bool useFinalRenderTarget)
{
    if (!useFinalRenderTarget) {
        return;
    }

    finalRenderTarget_.End();
}

/// <summary>
/// 現在の描画先へポストプロセス結果とスプライトを描画する
/// </summary>
void PlayScene::DrawPostProcessOutputToCurrentTarget(const PostProcessDrawContext& drawContext)
{
    DrawFinalPostProcessPass(
        drawContext.sourceSrvIndex,
        drawContext.finalEffectType,
        drawContext.useGaussianFilter);
    DrawSprites();
    DrawDebugLines2D();
}

/// <summary>
/// 作成済みのポストプロセス状態に従って最終結果を描画する
/// </summary>
void PlayScene::DrawPostProcessResult(const PostProcessDrawContext& drawContext)
{
    BeginSceneViewRenderTargetIfNeeded(drawContext.useFinalRenderTarget);
    DrawPostProcessOutputToCurrentTarget(drawContext);
    EndSceneViewRenderTargetIfNeeded(drawContext.useFinalRenderTarget);
}

/// <summary>
/// ポストプロセス付きでシーンを描画する
/// </summary>
bool PlayScene::DrawPostProcessedScene()
{
    if (!CanUsePostProcess()) {
        return false;
    }

    DrawSceneToPostProcessTarget();

    const PostProcessDrawContext drawContext = BuildPostProcessDrawContext(); // 最終描画に使用する状態
    DrawPostProcessResult(drawContext);
    return true;
}