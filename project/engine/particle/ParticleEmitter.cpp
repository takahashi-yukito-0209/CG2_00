#include "ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include "externals/imgui/imgui.h"
#include <cstring>

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

void ParticleEmitter::DrawImGui()
{
    // safer approach: edit group name via temporary buffer
    char buf[256] = {};
    strncpy_s(buf, sizeof(buf), groupName.c_str(), _TRUNCATE);
    if (ImGui::InputText("GroupName", buf, sizeof(buf))) {
        groupName = std::string(buf);
    }
    ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
    int tmpCount = static_cast<int>(count);
    if (ImGui::DragInt("Count", &tmpCount, 1, 0, 1000)) count = static_cast<uint32_t>(tmpCount);
    ImGui::DragFloat("Frequency", &frequency, 0.01f, 0.0f, 100.0f);
}
