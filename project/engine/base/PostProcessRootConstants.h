#pragma once

#include "PostProcess.h"

#include <array>
#include <cstdint>

namespace MyEngine::PostProcessRootConstants {

using RootConstantData = std::array<uint32_t, 20>; // ポストエフェクト用ルート定数データ

/// <summary>
/// 通常の1passポストエフェクト用ルート定数を作成する
/// </summary>
RootConstantData BuildTexture(PostEffectType effectType, const PostProcessSettings& settings);

/// <summary>
/// Gaussian Filter用ルート定数を作成する
/// </summary>
RootConstantData BuildGaussian(uint32_t direction, const PostProcessSettings& settings);

/// <summary>
/// Dissolve用ルート定数を作成する
/// </summary>
RootConstantData BuildDissolve(const PostProcessSettings& settings);

/// <summary>
/// Depth Outline用ルート定数を作成する
/// </summary>
RootConstantData BuildDepthOutline(
    const Math::Matrix4x4& projectionMatrix,
    const PostProcessSettings& settings);

} // namespace MyEngine::PostProcessRootConstants