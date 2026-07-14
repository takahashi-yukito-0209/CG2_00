#include "PlayScene.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/base/DirectXCommon.h"

using namespace MyEngine;

namespace {
constexpr int kGaussianFirstPassIndex = 0; // Gaussian Filterの横方向pass番号
constexpr int kGaussianSecondPassIndex = 1; // Gaussian Filterの縦方向pass番号
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

    postProcessIntermediateTarget_.Begin(true);
    postProcess_.DrawTexture(sceneRenderTarget_, PostEffectType::Distortion);
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
        postProcess_.DrawDepthOutline(postProcessSourceSrvIndex, sceneRenderTarget_, ctx_.camera->GetProjectionMatrix());
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