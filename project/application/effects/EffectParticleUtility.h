#pragma once

#include "../../engine/utility/MathTypes.h"

namespace EffectParticleUtility {

/// <summary>
/// GPUパーティクルプリセットを指定位置で再生する。
/// </summary>
bool PlayGpuParticlePreset(const char* ownerName, const char* presetName, const Math::Vector3& position);

} // namespace EffectParticleUtility
