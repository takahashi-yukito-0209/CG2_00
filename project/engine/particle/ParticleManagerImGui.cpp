#include "ParticleManager.h"
#include "GpuEmitterSettingsUtility.h"
#include "ImGuiManager.h"
#include "engine/base/PostProcess.h"
#include "engine/utility/FileUtility.h"
#include <algorithm>
#include <cstdio>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr float kImGuiFineStep = 0.01f; // 細かい値の調整幅
constexpr float kImGuiPhysicsStep = 0.1f; // 物理系値の調整幅
constexpr float kImGuiLifeMin = 0.1f; // 寿命設定の最小値
constexpr float kImGuiLifeMax = 100.0f; // 寿命設定の最大値
constexpr float kImGuiSpawnPositionMin = -50.0f; // 発生位置範囲の最小値
constexpr float kImGuiSpawnPositionMax = 50.0f; // 発生位置範囲の最大値
constexpr float kImGuiScaleMin = 0.01f; // スケール範囲の最小値
constexpr float kImGuiScaleMax = 10.0f; // スケール範囲の最大値
constexpr float kImGuiPhysicsMin = -100.0f; // 物理系値の最小値
constexpr float kImGuiPhysicsMax = 100.0f; // 物理系値の最大値
constexpr float kImGuiDampingMin = 0.0f; // 減衰率の最小値
constexpr float kImGuiDampingMax = 100.0f; // 減衰率の最大値
constexpr float kBoundsCenterRate = 0.5f; // 範囲の中心位置を求める倍率
} // namespace

#ifdef USE_IMGUI

/// <summary>
/// ImGuiでGPU Emitterの基本情報を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterStatusImGui()
{
    UpdateGpuAliveCountEstimate();
    ImGui::Text("Ready: %s", gpuParticleReady_ ? "true" : "false");
    ImGui::Text("GPU Draw Request: %u / %u", gpuEmitterVisibleCount_, GetParticleLimit());
    ImGui::Text("GPU Alive Estimate: %u / %u", gpuAliveCountEstimate_, GetParticleLimit());
}

/// <summary>
/// ImGuiでGPU Emitterのeffect情報を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterEffectImGui()
{
    char effectNameBuffer[64] {}; // effect名入力用バッファ
    std::snprintf(effectNameBuffer, sizeof(effectNameBuffer), "%s", gpuEmitterEffectName_.c_str());
    if (ImGui::InputText("Effect Name", effectNameBuffer, sizeof(effectNameBuffer))) {
        gpuEmitterEffectName_ = effectNameBuffer;
    }

    char descriptionBuffer[160] {}; // 説明文入力用バッファ
    std::snprintf(descriptionBuffer, sizeof(descriptionBuffer), "%s", gpuEmitterDescription_.c_str());
    if (ImGui::InputTextMultiline("Description", descriptionBuffer, sizeof(descriptionBuffer), ImVec2(0.0f, 42.0f))) {
        gpuEmitterDescription_ = descriptionBuffer;
    }

    char texturePathBuffer[128] {}; // GPU描画に使うテクスチャパス入力用バッファ
    std::snprintf(texturePathBuffer, sizeof(texturePathBuffer), "%s", gpuEmitterTexturePath_.c_str());
    if (ImGui::InputText("GPU Texture", texturePathBuffer, sizeof(texturePathBuffer))) {
        gpuEmitterTexturePath_ = texturePathBuffer;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply GPU Texture")) {
        ApplyGpuEmitterTextureToDrawGroup();
    }
}

/// <summary>
/// ImGuiでGPU Emitterに紐づくPostProcess設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPostProcessImGui(PostProcess* postProcess)
{
    ImGui::Checkbox("Use Saved PostProcess", &gpuEmitterUsePostProcess_);
    ImGui::SameLine();
    if (postProcess && ImGui::Button("Capture PostProcess")) {
        CaptureGpuEmitterPostProcessSettings(*postProcess);
        gpuEmitterSettingsMessage_ = "Captured current PostProcess settings";
    }
    ImGui::SameLine();
    if (postProcess && ImGui::Button("Apply PostProcess")) {
        ApplyGpuEmitterPostProcessSettings(*postProcess);
        gpuEmitterSettingsMessage_ = "Applied saved PostProcess settings";
    }
}

/// <summary>
/// ImGuiでGPU Emitterの発生設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterStateImGui()
{
    DrawGpuEmitterPlaybackStateImGui();
    DrawGpuEmitterSpawnStateImGui();
    DrawGpuEmitterScaleLifeStateImGui();
    DrawGpuEmitterPhysicsStateImGui();
    DrawGpuEmitterColorStateImGui();
    NormalizeGpuEmitterStateForRuntime();
}

/// <summary>
/// ImGuiでGPU Emitterの再生フラグを編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPlaybackStateImGui()
{
    ImGui::Checkbox("Auto Emit", &gpuEmitterAutoEmit_);
    ImGui::SameLine();
    ImGui::Checkbox("Update GPU Particles", &gpuParticleUpdateEnabled_);
    ImGui::SameLine();
    ImGui::Checkbox("Draw GPU Particles", &gpuParticleDrawEnabled_);
}

/// <summary>
/// ImGuiでGPU Emitterの発生範囲と発生数を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterSpawnStateImGui()
{
    ImGui::DragFloat3("Emitter Position", &gpuEmitterState_.translate.x, kImGuiFineStep, kImGuiSpawnPositionMin, kImGuiSpawnPositionMax);
    ImGui::DragFloat("Emitter Radius", &gpuEmitterState_.radius, kImGuiFineStep, 0.0f, kImGuiSpawnPositionMax);

    const char* spawnShapeLabels[] = { "Sphere", "Box", "Ring", "Cone" }; // ImGui表示用の発生形状名
    constexpr int spawnShapeCount = 4; // 選択できる発生形状数
    int spawnShapeIndex = static_cast<int>((std::min)(gpuEmitterState_.spawnShape, static_cast<uint32_t>(spawnShapeCount - 1))); // ImGui編集用の発生形状番号
    if (ImGui::Combo("Spawn Shape", &spawnShapeIndex, spawnShapeLabels, spawnShapeCount)) {
        gpuEmitterState_.spawnShape = static_cast<uint32_t>(spawnShapeIndex);
    }

    int gpuEmitCount = static_cast<int>(gpuEmitterState_.count); // ImGui編集用の射出数
    if (ImGui::SliderInt("Emit Count", &gpuEmitCount, 0, static_cast<int>(GetParticleLimit()))) {
        gpuEmitterState_.count = static_cast<uint32_t>((std::max)(gpuEmitCount, 0));
        if (gpuEmitterState_.count == 0) {
            ClearGpuEmitterRuntimeParticleState();
            gpuEmitterManualEmitRequested_ = false;
            gpuEmitterState_.emit = 0;
        }
    }

    ImGui::DragFloat("Frequency", &gpuEmitterState_.frequency, kImGuiFineStep, 0.001f, 10.0f);
}

/// <summary>
/// ImGuiでGPU Emitterのスケールと寿命を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterScaleLifeStateImGui()
{
    ImGui::DragFloat3("Base Scale", &gpuEmitterState_.baseScale.x, kImGuiFineStep, kImGuiScaleMin, kImGuiScaleMax);
    ImGui::DragFloat("Random Scale", &gpuEmitterState_.randomScale, kImGuiFineStep, 0.0f, kImGuiScaleMax);

    ImGui::DragFloat3("Velocity Scale", &gpuEmitterState_.velocityScale.x, kImGuiFineStep, kImGuiPhysicsMin, kImGuiPhysicsMax);
    ImGui::DragFloat("Life Time", &gpuEmitterState_.lifeTime, kImGuiFineStep, kImGuiLifeMin, kImGuiLifeMax);

    bool scaleOverLife = gpuEmitterState_.scaleOverLife != 0; // 寿命に応じてスケールを変えるか
    if (ImGui::Checkbox("Scale Over Life", &scaleOverLife)) {
        gpuEmitterState_.scaleOverLife = scaleOverLife ? 1u : 0u;
    }
    ImGui::DragFloat3("End Scale", &gpuEmitterState_.endScale.x, kImGuiFineStep, 0.0f, kImGuiScaleMax);
}

/// <summary>
/// ImGuiでGPU Emitterの物理挙動を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPhysicsStateImGui()
{
    ImGui::DragFloat3("Gravity", &gpuEmitterState_.gravity.x, kImGuiPhysicsStep, kImGuiPhysicsMin, kImGuiPhysicsMax);
    ImGui::DragFloat("Damping", &gpuEmitterState_.damping, kImGuiFineStep, kImGuiDampingMin, kImGuiDampingMax);
}

/// <summary>
/// ImGuiでGPU Emitterの色変化を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterColorStateImGui()
{
    ImGui::ColorEdit4("GPU Color Min", &gpuEmitterState_.colorMin.x);
    ImGui::ColorEdit4("GPU Color Max", &gpuEmitterState_.colorMax.x);
    bool colorOverLife = gpuEmitterState_.colorOverLife != 0; // 寿命に応じて色を変えるか
    if (ImGui::Checkbox("Color Over Life", &colorOverLife)) {
        gpuEmitterState_.colorOverLife = colorOverLife ? 1u : 0u;
    }
    ImGui::ColorEdit4("End Color", &gpuEmitterState_.endColor.x);
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの保存と読み込みを操作する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileImGui()
{
    DrawGpuEmitterSettingsNameImGui();

    const std::vector<std::string> settingsFiles = GpuEmitterSettingsUtility::CollectSettingsFiles(); // 読み込み候補のJSON一覧
    const std::string saveSettingsPath = GpuEmitterSettingsUtility::BuildSettingsPath(gpuEmitterSettingsName_); // 保存先JSONパス
    const std::string selectedSettingsPath = GpuEmitterSettingsUtility::ResolveSettingsPath(gpuEmitterSettingsName_, settingsFiles); // 読み込み対象JSONパス
    const std::string loadedPresetName = gpuEmitterLoadedSettingsName_.empty() ? "None" : gpuEmitterLoadedSettingsName_; // 表示用のロード済み設定名
    std::string settingsPreview = GpuEmitterSettingsUtility::SanitizeName(gpuEmitterSettingsName_); // コンボ表示用の設定名
    if (settingsPreview.empty()) {
        settingsPreview = "gpu_particle";
    }

    ImGui::Text("Loaded Preset: %s", loadedPresetName.c_str());
    ImGui::Text("Save Path: %s", saveSettingsPath.c_str());
    ImGui::Text("Selected File: %s", selectedSettingsPath.c_str());
    ImGui::Text("Load Files: %zu", settingsFiles.size());

    DrawGpuEmitterSettingsFileComboImGui(settingsFiles, settingsPreview);
    DrawGpuEmitterSettingsFileButtonsImGui(saveSettingsPath, selectedSettingsPath);

    if (!gpuEmitterSettingsMessage_.empty()) {
        ImGui::TextWrapped("%s", gpuEmitterSettingsMessage_.c_str());
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定名を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsNameImGui()
{
    char settingsNameBuffer[64] {}; // 設定名入力用バッファ
    std::snprintf(settingsNameBuffer, sizeof(settingsNameBuffer), "%s", gpuEmitterSettingsName_.c_str());
    if (ImGui::InputText("Settings Name", settingsNameBuffer, sizeof(settingsNameBuffer))) {
        gpuEmitterSettingsName_ = settingsNameBuffer;
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの選択欄を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileComboImGui(const std::vector<std::string>& settingsFiles, const std::string& settingsPreview)
{
    if (ImGui::BeginCombo("Load File", settingsPreview.c_str())) {
        if (settingsFiles.empty()) {
            ImGui::TextDisabled("No json files in resources/effects");
        }
        for (const std::string& filePath : settingsFiles) {
            const std::string stemName = FileUtility::GetStem(filePath); // 選択表示用のファイル名
            const bool isSelected = stemName == settingsPreview; // 現在選択中か
            const std::string selectableLabel = stemName + "##" + filePath; // 表示名とImGui内部IDを分けるラベル
            if (ImGui::Selectable(selectableLabel.c_str(), isSelected)) {
                gpuEmitterSettingsName_ = stemName;
                gpuEmitterSettingsMessage_ = "Selected: " + filePath;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの操作ボタンを表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileButtonsImGui(const std::string& saveSettingsPath, const std::string& selectedSettingsPath)
{
    if (ImGui::Button("Save GPU Settings")) {
        SaveGpuEmitterSettingsFromImGui(saveSettingsPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load GPU Settings")) {
        LoadGpuEmitterSettingsFromImGui(selectedSettingsPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        DeleteGpuEmitterSettingsFromImGui(selectedSettingsPath);
    }
}

/// <summary>
/// GPU Emitter設定を指定パスへ保存して結果メッセージを更新する。
/// </summary>
void ParticleManager::SaveGpuEmitterSettingsFromImGui(const std::string& saveSettingsPath)
{
    const bool willOverwrite = FileUtility::Exists(saveSettingsPath); // 既存ファイルを上書きするか
    if (SaveGpuEmitterSettings(saveSettingsPath)) {
        gpuEmitterLoadedSettingsName_ = FileUtility::GetStem(saveSettingsPath);
        gpuEmitterSettingsMessage_ = std::string(willOverwrite ? "Overwritten: " : "Saved: ") + saveSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Save failed: " + saveSettingsPath;
    }
}

/// <summary>
/// GPU Emitter設定を指定パスから読み込んで結果メッセージを更新する。
/// </summary>
void ParticleManager::LoadGpuEmitterSettingsFromImGui(const std::string& loadSettingsPath)
{
    if (LoadGpuEmitterSettings(loadSettingsPath)) {
        gpuEmitterLoadedSettingsName_ = FileUtility::GetStem(loadSettingsPath);
        gpuEmitterSettingsName_ = gpuEmitterLoadedSettingsName_;
        gpuEmitterSettingsMessage_ = "Loaded: " + loadSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Load failed: " + loadSettingsPath;
    }
}

/// <summary>
/// GPU Emitter設定ファイルを削除して結果メッセージを更新する。
/// </summary>
void ParticleManager::DeleteGpuEmitterSettingsFromImGui(const std::string& selectedSettingsPath)
{
    const bool removed = FileUtility::RemoveFile(selectedSettingsPath); // JSON削除結果
    if (removed) {
        const std::string deletedName = FileUtility::GetStem(selectedSettingsPath); // 削除した設定名
        if (gpuEmitterLoadedSettingsName_ == deletedName) {
            gpuEmitterLoadedSettingsName_.clear();
        }
        gpuEmitterSettingsMessage_ = "Deleted: " + selectedSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Delete failed: " + selectedSettingsPath;
    }
}

/// <summary>
/// ImGuiでGPU Emitterの実行操作を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterControlImGui()
{
    if (ImGui::Button("Reset GPU Particles")) {
        ResetGpuEmitterParticles();
    }
    ImGui::SameLine();
    if (ImGui::Button("Emit Once")) {
        gpuEmitterManualEmitRequested_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Emit Next Frame")) {
        gpuEmitterState_.frequencyTime = gpuEmitterState_.frequency;
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterImGui(PostProcess* postProcess)
{
    if (ImGui::CollapsingHeader("GPU Particle", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawGpuEmitterStatusImGui();
        DrawGpuEmitterEffectImGui();
        DrawGpuEmitterPostProcessImGui(postProcess);
        DrawGpuEmitterStateImGui();
        DrawGpuEmitterSettingsFileImGui();
        DrawGpuEmitterControlImGui();
    }
}
#endif

/// <summary>
/// ImGuiでパーティクル設定を編集する
/// </summary>
void ParticleManager::DrawImGui(PostProcess* postProcess)
{
#ifdef USE_IMGUI
    ImGui::Text("Groups: %zu", particleGroups_.size());

    if (ImGui::CollapsingHeader("Lifetime", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloatRange2(
            "Life Min/Max",
            &lifeMin_,
            &lifeMax_,
            kImGuiFineStep,
            kImGuiLifeMin,
            kImGuiLifeMax);
    }

    if (ImGui::CollapsingHeader("Spawn Random", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(
            "Spawn Pos Min",
            &spawnPosMin_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        ImGui::DragFloat3(
            "Spawn Pos Max",
            &spawnPosMax_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        ImGui::DragFloat3(
            "Scale Min",
            &scaleMin_.x,
            kImGuiFineStep,
            kImGuiScaleMin,
            kImGuiScaleMax);
        ImGui::DragFloat3(
            "Scale Max",
            &scaleMax_.x,
            kImGuiFineStep,
            kImGuiScaleMin,
            kImGuiScaleMax);
    }

    if (ImGui::CollapsingHeader("Velocity / Physics")) {
        ImGui::DragFloat3(
            "Vel Min",
            &velMin_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        ImGui::DragFloat3(
            "Vel Max",
            &velMax_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);

        ImGui::Checkbox("Enable Gravity", &gravityEnabled_);
        ImGui::DragFloat3(
            "Gravity",
            &gravity_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::DragFloat(
            "Damping",
            &damping_,
            kImGuiFineStep,
            kImGuiDampingMin,
            kImGuiDampingMax);
    }

    if (ImGui::CollapsingHeader("Field")) {
        ImGui::Checkbox("Enable Field", &fieldEnabled_);
        ImGui::DragFloat3(
            "Field Accel",
            &fieldAccel_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::DragFloat3(
            "Field Min",
            &fieldMin_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::DragFloat3(
            "Field Max",
            &fieldMax_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
    }

    if (ImGui::CollapsingHeader("Color")) {
        ImGui::ColorEdit4("Color Min", &colMin_.x);
        ImGui::ColorEdit4("Color Max", &colMax_.x);
    }

    DrawGpuEmitterImGui(postProcess);
    if (ImGui::CollapsingHeader("Groups")) {
        for (auto& kv : particleGroups_) {
            if (ImGui::TreeNode(kv.first.c_str())) {
                ImGui::Text("Count = %zu", kv.second.particles.size());
                ImGui::Text("Texture = %s", kv.second.texturePath.c_str());
                if (!kv.second.particles.empty()) {
                    bool hasBounds = false; // 範囲の初期化が済んでいるか
                    Vector3 minimumPosition {}; // グループ内の最小座標
                    Vector3 maximumPosition {}; // グループ内の最大座標
                    const PM_CpuParticle* firstParticle = nullptr; // 先頭パーティクルの参照

                    for (const PM_CpuParticle& particle : kv.second.particles) {
                        const Vector3& position = particle.transform.translate; // 現在のワールド座標
                        if (!hasBounds) {
                            minimumPosition = position;
                            maximumPosition = position;
                            firstParticle = &particle;
                            hasBounds = true;
                            continue;
                        }

                        minimumPosition.x = (std::min)(minimumPosition.x, position.x);
                        minimumPosition.y = (std::min)(minimumPosition.y, position.y);
                        minimumPosition.z = (std::min)(minimumPosition.z, position.z);
                        maximumPosition.x = (std::max)(maximumPosition.x, position.x);
                        maximumPosition.y = (std::max)(maximumPosition.y, position.y);
                        maximumPosition.z = (std::max)(maximumPosition.z, position.z);
                    }

                    const Vector3 centerPosition {
                        (minimumPosition.x + maximumPosition.x) * kBoundsCenterRate,
                        (minimumPosition.y + maximumPosition.y) * kBoundsCenterRate,
                        (minimumPosition.z + maximumPosition.z) * kBoundsCenterRate
                    }; // グループ全体の中心座標

                    ImGui::Text("Center = %.2f, %.2f, %.2f", centerPosition.x, centerPosition.y, centerPosition.z);
                    ImGui::Text("Min = %.2f, %.2f, %.2f", minimumPosition.x, minimumPosition.y, minimumPosition.z);
                    ImGui::Text("Max = %.2f, %.2f, %.2f", maximumPosition.x, maximumPosition.y, maximumPosition.z);
                    if (firstParticle) {
                        const Vector3& firstPosition = firstParticle->transform.translate; // 先頭パーティクルの座標
                        ImGui::Text("First = %.2f, %.2f, %.2f", firstPosition.x, firstPosition.y, firstPosition.z);
                        const Vector3& firstScale = firstParticle->transform.scale; // 先頭パーティクルのスケール
                        const Vector4& firstColor = firstParticle->color; // 先頭パーティクルの色
                        ImGui::Text("Scale = %.2f, %.2f, %.2f", firstScale.x, firstScale.y, firstScale.z);
                        ImGui::Text("Color = %.2f, %.2f, %.2f, %.2f", firstColor.x, firstColor.y, firstColor.z, firstColor.w);
                    }
                }

                ImGui::Checkbox("Use Billboard", &kv.second.useBillboard);
                ImGui::TreePop();
            }
        }
    }
#endif
}
