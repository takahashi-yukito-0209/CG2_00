#include "ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include <cstring>

using namespace MyEngine;

#ifdef USE_IMGUI
static_assert(true, "ImGui available");
#endif

/// <summary>
/// 発生させるパーティクルグループの名前を指定してエミッタを作成
/// </summary>
void ParticleEmitter::Emit() {
    // グループ名が空なら発生させない
    if (groupName.empty()) { return; }

    if (useCylinderEffect) {
        ParticleManager::GetInstance()->EmitCylinderEffect(groupName, transform.translate, count);
    } else if (useRingEffect) {
        ParticleManager::GetInstance()->EmitRingEffect(groupName, transform.translate, count);
    } else if (useHitEffect) {
        ParticleManager::GetInstance()->EmitHitEffect(groupName, transform.translate, count);
    } else {
        ParticleManager::GetInstance()->Emit(groupName, transform.translate, count);
    }
}

/// <summary>
///  経過時間を加算し、発生間隔を超えたら発生させる
/// </summary>
void ParticleEmitter::Update(float deltaTime) {
    // 経過時間を加算
    elapsed += deltaTime;
    // 発生間隔が0以下なら毎フレーム発生
    if (frequency <= 0.0f) {
        if (count) { Emit(); }
        elapsed = 0.0f;
        return;
    }
    // 発生間隔を超えたら発生させる
    while (elapsed >= frequency) {
        Emit();
        elapsed -= frequency; // 余剰も考慮
    }
}

/// <summary>
/// ImGuiコントロールを描画（プロパティ編集）
/// </summary>
void ParticleEmitter::DrawImGui()
{
    // グループ名の編集用バッファを用意
    char buf[256] = {};
    // 現在のグループ名をバッファにコピー（安全な関数を使用）
    strncpy_s(buf, sizeof(buf), groupName.c_str(), _TRUNCATE);
    // ImGuiのInputTextで編集。変更があったらグループ名を更新
#ifdef USE_IMGUI
    if (ImGui::InputText("GroupName", buf, sizeof(buf))) {
        groupName = std::string(buf);
    }
    // 座標（平行移動）の編集
    ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
    // スケールの編集
    int tmpCount = static_cast<int>(count);
    // 1以上1000以下の整数として編集。変更があったらcountを更新
    if (ImGui::DragInt("Count", &tmpCount, 1, 0, 1000)) count = static_cast<uint32_t>(tmpCount);
    ImGui::DragFloat("Frequency", &frequency, 0.01f, 0.0f, 100.0f);
    ImGui::Checkbox("Use Hit Effect", &useHitEffect);
    ImGui::Checkbox("Use Ring Effect", &useRingEffect);
    ImGui::Checkbox("Use Cylinder Effect", &useCylinderEffect);
#else
    (void)buf;
#endif
}
