#include "EffectParticleUtility.h"

#include "../../engine/particle/ParticleManager.h"
#include "../../engine/utility/Logger.h"

#include <string>

using namespace Math;
using namespace MyEngine;

namespace EffectParticleUtility {

/// <summary>
/// GPUパーティクルプリセットを指定位置で再生する。
/// </summary>
bool PlayGpuParticlePreset(const char* ownerName, const char* presetName, const Vector3& position)
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // GPUプリセット再生に使うパーティクル管理
    if (!particleManager) {
        Logger::Warn(std::string(ownerName) + "::PlayGpuParticlePreset: ParticleManager is null. preset=" + presetName + "\n");
        return false;
    }

    if (!particleManager->PlayGpuEmitterPreset(presetName, position)) {
        Logger::Warn(std::string(ownerName) + "::PlayGpuParticlePreset: failed to play preset " + presetName + "\n");
        return false;
    }

    return true;
}

} // namespace EffectParticleUtility
