#include "ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include <cstring>

using namespace MyEngine;

namespace {
constexpr size_t kGroupNameInputBufferSize = 256; // グループ名入力用バッファサイズ
constexpr float kImGuiTranslateStep = 0.01f; // 発生位置の調整幅
constexpr int kImGuiCountStep = 1; // 発生数の調整幅
constexpr int kImGuiCountMin = 0; // 発生数の最小値
constexpr int kImGuiCountMax = 1000; // 発生数の最大値
constexpr float kImGuiFrequencyStep = 0.01f; // 発生間隔の調整幅
constexpr float kImGuiFrequencyMin = 0.0f; // 発生間隔の最小値
constexpr float kImGuiFrequencyMax = 100.0f; // 発生間隔の最大値
constexpr float kAlwaysEmitFrequencyThreshold = 0.0f; // 毎フレーム発生に切り替える発生間隔の下限
constexpr float kEmitterElapsedTimeStart = 0.0f; // 発生経過時間の初期値
} // namespace

#ifdef USE_IMGUI
static_assert(true, "ImGui available");
#endif

/// <summary>
/// 現在の設定に従ってパーティクルを発生させる。
/// </summary>
void ParticleEmitter::Emit()
{
    // グループ未設定なら発生させない
    if (groupName.empty()) {
        return;
    }

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
/// 経過時間を進め、発生間隔を超えた分だけ発生させる。
/// </summary>
void ParticleEmitter::Update(float deltaTime)
{
    // 経過時間を進める
    elapsed += deltaTime;
    // 発生間隔が0以下なら毎フレーム発生させる
    if (frequency <= kAlwaysEmitFrequencyThreshold) {
        if (count) {
            Emit();
        }
        elapsed = kEmitterElapsedTimeStart;
        return;
    }
    // 発生間隔を超えたら発生させる
    while (elapsed >= frequency) {
        Emit();
        elapsed -= frequency; // 余剰も考慮
    }
}

/// <summary>
/// ImGuiでエミッター設定を編集する。
/// </summary>
void ParticleEmitter::DrawImGui()
{
    // グループ名の編集用バッファを用意
    char buf[kGroupNameInputBufferSize] = {};
    // 現在のグループ名をバッファにコピー（安全な関数を使用）
    strncpy_s(buf, sizeof(buf), groupName.c_str(), _TRUNCATE);
    // 入力内容が変わったらグループ名を更新する
#ifdef USE_IMGUI
    if (ImGui::InputText("GroupName", buf, sizeof(buf))) {
        groupName = std::string(buf);
    }
    // 発生位置を編集する
    ImGui::DragFloat3("Translate", &transform.translate.x, kImGuiTranslateStep);
    // 発生数を編集する
    int tmpCount = static_cast<int>(count);
    // 変更があったら発生数へ反映する
    if (ImGui::DragInt("Count", &tmpCount, kImGuiCountStep, kImGuiCountMin, kImGuiCountMax))
        count = static_cast<uint32_t>(tmpCount);
    ImGui::DragFloat("Frequency", &frequency, kImGuiFrequencyStep, kImGuiFrequencyMin, kImGuiFrequencyMax);
    ImGui::Checkbox("Use Hit Effect", &useHitEffect);
    ImGui::Checkbox("Use Ring Effect", &useRingEffect);
    ImGui::Checkbox("Use Cylinder Effect", &useCylinderEffect);
#else
    (void)buf;
#endif
}
