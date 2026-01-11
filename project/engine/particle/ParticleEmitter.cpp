#include "ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"

void ParticleEmitter::Emit() {
    if (groupName.empty()) { return; }
    MyEngine::ParticleManager::GetInstance()->Emit(groupName, transform.translate, count);
}

void ParticleEmitter::Update(float deltaTime) {
    elapsed += deltaTime;
    if (frequency <= 0.0f) {
        if (count) { Emit(); }
        elapsed = 0.0f;
        return;
    }
    while (elapsed >= frequency) {
        Emit();
        elapsed -= frequency; // 余剰も考慮
    }
}
