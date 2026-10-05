#include "ParticleManager.h"
#include "GpuEmitterSettingsUtility.h"
#include "ParticleEditorImGuiUtility.h"
#include "ImGuiManager.h"
#include "engine/base/PostProcess.h"
#include "engine/utility/FileUtility.h"
#include <algorithm>
#include <cfloat>

using namespace Math;
using namespace MyEngine;
using namespace MyEngine::ParticleEditorImGuiUtility;

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
    ImGui::TextWrapped("Draw Request: %u / %u", gpuEmitterVisibleCount_, GetParticleLimit());
    ImGui::TextWrapped("Alive Estimate: %u / %u", gpuAliveCountEstimate_, GetParticleLimit());
}

/// <summary>
/// ImGuiでGPU Emitterのeffect情報を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterEffectImGui()
{
    DrawParticleTextInput("Effect Name", "##EffectName", gpuEmitterEffectName_);
    DrawParticleTextInput("Description", "##Description", gpuEmitterDescription_, true);
    DrawParticleTextInput("Texture", "##TexturePath", gpuEmitterTexturePath_);
    if (ImGui::Button("Apply Texture", ImVec2(-FLT_MIN, 0.0f))) {
        ApplyGpuEmitterTextureToDrawGroup();
    }
}

/// <summary>
/// ImGuiでGPU Emitterに紐づくPostProcess設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPostProcessImGui(PostProcess* postProcess)
{
    ImGui::Checkbox("Use Saved PostProcess", &gpuEmitterUsePostProcess_);
    ImGui::BeginDisabled(postProcess == nullptr);
    if (ImGui::Button("Capture PostProcess", ImVec2(-FLT_MIN, 0.0f))) {
        CaptureGpuEmitterPostProcessSettings(*postProcess);
        gpuEmitterSettingsMessage_ = "Captured current PostProcess settings";
    }
    if (ImGui::Button("Apply PostProcess", ImVec2(-FLT_MIN, 0.0f))) {
        ApplyGpuEmitterPostProcessSettings(*postProcess);
        gpuEmitterSettingsMessage_ = "Applied saved PostProcess settings";
    }
    ImGui::EndDisabled();
}

/// <summary>
/// ImGuiでGPU Emitterの発生設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterStateImGui()
{
    if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawGpuEmitterSpawnStateImGui();
    }
    if (ImGui::CollapsingHeader("Size / Lifetime")) {
        DrawGpuEmitterScaleLifeStateImGui();
    }
    if (ImGui::CollapsingHeader("Motion")) {
        DrawGpuEmitterPhysicsStateImGui();
    }
    if (ImGui::CollapsingHeader("Color")) {
        DrawGpuEmitterColorStateImGui();
    }
    NormalizeGpuEmitterStateForRuntime();
}

/// <summary>
/// ImGuiでGPU Emitterの再生フラグを編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPlaybackStateImGui()
{
    ImGui::Checkbox("Auto Emit", &gpuEmitterAutoEmit_);
    ImGui::Checkbox("Update GPU Particles", &gpuParticleUpdateEnabled_);
    ImGui::Checkbox("Draw GPU Particles", &gpuParticleDrawEnabled_);
}

/// <summary>
/// ImGuiでGPU Emitterの発生範囲と発生数を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterSpawnStateImGui()
{
    DrawParticlePropertyLabel("Position");
    ImGui::DragFloat3("##EmitterPosition", &gpuEmitterState_.translate.x, kImGuiFineStep, kImGuiSpawnPositionMin, kImGuiSpawnPositionMax);
    DrawParticlePropertyLabel("Radius");
    ImGui::DragFloat("##EmitterRadius", &gpuEmitterState_.radius, kImGuiFineStep, 0.0f, kImGuiSpawnPositionMax);

    const char* spawnShapeLabels[] = { "Sphere", "Box", "Ring", "Cone" }; // ImGui表示用の発生形状名
    constexpr int spawnShapeCount = 4; // 選択できる発生形状数
    int spawnShapeIndex = static_cast<int>((std::min)(gpuEmitterState_.spawnShape, static_cast<uint32_t>(spawnShapeCount - 1))); // ImGui編集用の発生形状番号
    DrawParticlePropertyLabel("Shape");
    if (ImGui::Combo("##SpawnShape", &spawnShapeIndex, spawnShapeLabels, spawnShapeCount)) {
        gpuEmitterState_.spawnShape = static_cast<uint32_t>(spawnShapeIndex);
    }

    int gpuEmitCount = static_cast<int>(gpuEmitterState_.count); // ImGui編集用の射出数
    DrawParticlePropertyLabel("Particles / Emit");
    if (ImGui::DragInt("##EmitCount", &gpuEmitCount, 1.0f, 0, static_cast<int>(GetParticleLimit()), "%d", ImGuiSliderFlags_AlwaysClamp)) {
        gpuEmitterState_.count = static_cast<uint32_t>((std::max)(gpuEmitCount, 0));
        if (gpuEmitterState_.count == 0) {
            ClearGpuEmitterRuntimeParticleState();
            gpuEmitterManualEmitRequested_ = false;
            gpuEmitterState_.emit = 0;
        }
    }

    DrawParticlePropertyLabel("Emit Interval (s)");
    ImGui::DragFloat("##Frequency", &gpuEmitterState_.frequency, kImGuiFineStep, 0.001f, 10.0f);
}

/// <summary>
/// ImGuiでGPU Emitterのスケールと寿命を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterScaleLifeStateImGui()
{
    DrawParticlePropertyLabel("Base Scale");
    ImGui::DragFloat3("##BaseScale", &gpuEmitterState_.baseScale.x, kImGuiFineStep, kImGuiScaleMin, kImGuiScaleMax);
    DrawParticlePropertyLabel("Random Scale");
    ImGui::DragFloat("##RandomScale", &gpuEmitterState_.randomScale, kImGuiFineStep, 0.0f, kImGuiScaleMax);
    DrawParticlePropertyLabel("Lifetime (s)");
    ImGui::DragFloat("##LifeTime", &gpuEmitterState_.lifeTime, kImGuiFineStep, kImGuiLifeMin, kImGuiLifeMax);

    bool scaleOverLife = gpuEmitterState_.scaleOverLife != 0; // 寿命に応じてスケールを変えるか
    if (ImGui::Checkbox("Scale Over Life", &scaleOverLife)) {
        gpuEmitterState_.scaleOverLife = scaleOverLife ? 1u : 0u;
    }
    ImGui::BeginDisabled(!scaleOverLife);
    DrawParticlePropertyLabel("End Scale");
    ImGui::DragFloat3("##EndScale", &gpuEmitterState_.endScale.x, kImGuiFineStep, 0.0f, kImGuiScaleMax);
    ImGui::EndDisabled();
}

/// <summary>
/// ImGuiでGPU Emitterの物理挙動を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPhysicsStateImGui()
{
    DrawParticlePropertyLabel("Velocity Scale");
    ImGui::DragFloat3("##VelocityScale", &gpuEmitterState_.velocityScale.x, kImGuiFineStep, kImGuiPhysicsMin, kImGuiPhysicsMax);
    DrawParticlePropertyLabel("Gravity");
    ImGui::DragFloat3("##Gravity", &gpuEmitterState_.gravity.x, kImGuiPhysicsStep, kImGuiPhysicsMin, kImGuiPhysicsMax);
    DrawParticlePropertyLabel("Damping");
    ImGui::DragFloat("##Damping", &gpuEmitterState_.damping, kImGuiFineStep, kImGuiDampingMin, kImGuiDampingMax);
}

/// <summary>
/// ImGuiでGPU Emitterの色変化を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterColorStateImGui()
{
    DrawParticlePropertyLabel("Color Min");
    ImGui::ColorEdit4("##ColorMin", &gpuEmitterState_.colorMin.x, ImGuiColorEditFlags_Float);
    DrawParticlePropertyLabel("Color Max");
    ImGui::ColorEdit4("##ColorMax", &gpuEmitterState_.colorMax.x, ImGuiColorEditFlags_Float);
    bool colorOverLife = gpuEmitterState_.colorOverLife != 0; // 寿命に応じて色を変えるか
    if (ImGui::Checkbox("Color Over Life", &colorOverLife)) {
        gpuEmitterState_.colorOverLife = colorOverLife ? 1u : 0u;
    }
    ImGui::BeginDisabled(!colorOverLife);
    DrawParticlePropertyLabel("End Color");
    ImGui::ColorEdit4("##EndColor", &gpuEmitterState_.endColor.x, ImGuiColorEditFlags_Float);
    ImGui::EndDisabled();
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの保存と読み込みを操作する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileImGui()
{
    const std::vector<std::string> settingsFiles = GpuEmitterSettingsUtility::CollectSettingsFiles(); // 読み込み候補のJSON一覧
    if (gpuEmitterSelectedSettingsPath_.empty()) {
        gpuEmitterSelectedSettingsPath_ = GpuEmitterSettingsUtility::ResolveSettingsPath(gpuEmitterSettingsName_, settingsFiles);
    }
    const std::string settingsPreview = FileUtility::GetStem(gpuEmitterSelectedSettingsPath_); // 選択中のプリセット名
    DrawGpuEmitterSettingsFileComboImGui(settingsFiles, settingsPreview);
    ImGui::BeginDisabled(!FileUtility::Exists(gpuEmitterSelectedSettingsPath_));
    if (ImGui::Button("Load Preset", ImVec2(-FLT_MIN, 0.0f))) {
        LoadGpuEmitterSettingsFromImGui(gpuEmitterSelectedSettingsPath_);
    }
    ImGui::EndDisabled();
    DrawGpuEmitterSettingsNameImGui();
    DrawGpuEmitterSettingsFileButtonsImGui(gpuEmitterSelectedSettingsPath_);
    if (ImGui::TreeNode("File Details")) {
        ImGui::TextWrapped("Save: %s", GpuEmitterSettingsUtility::BuildSettingsPath(gpuEmitterSettingsName_).c_str());
        ImGui::TextWrapped("Selected: %s", gpuEmitterSelectedSettingsPath_.c_str());
        ImGui::TreePop();
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定名を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsNameImGui()
{
    DrawParticleTextInput("Save Name", "##SettingsName", gpuEmitterSettingsName_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("File name: A-Z, a-z, 0-9, _ and -");
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの選択欄を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileComboImGui(const std::vector<std::string>& settingsFiles, const std::string& settingsPreview)
{
    DrawParticlePropertyLabel("Preset");
    if (ImGui::BeginCombo("##LoadFile", settingsPreview.c_str())) {
        if (settingsFiles.empty()) {
            ImGui::TextWrapped("No presets");
        }
        for (const std::string& filePath : settingsFiles) {
            const std::string stemName = FileUtility::GetStem(filePath); // 選択表示用のファイル名
            const bool isSelected = filePath == gpuEmitterSelectedSettingsPath_; // 現在選択中か
            const std::string selectableLabel = stemName + "##" + filePath; // 表示名とImGui内部IDを分けるラベル
            if (ImGui::Selectable(selectableLabel.c_str(), isSelected)) {
                gpuEmitterSelectedSettingsPath_ = filePath;
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
void ParticleManager::DrawGpuEmitterSettingsFileButtonsImGui(const std::string& selectedSettingsPath)
{
    // 名前編集と同じフレームの最新値で保存先を決める。
    const std::string currentSavePath = GpuEmitterSettingsUtility::BuildSettingsPath(gpuEmitterSettingsName_); // 最新の入力名に対応する保存先
    const bool validSaveName = !gpuEmitterSettingsName_.empty()
        && gpuEmitterSettingsName_ == GpuEmitterSettingsUtility::SanitizeName(gpuEmitterSettingsName_); // 意図しない名前変換を伴わないか
    ImGui::BeginDisabled(!validSaveName);
    if (ImGui::Button("Save Preset", ImVec2(-FLT_MIN, 0.0f))) {
        if (FileUtility::Exists(currentSavePath)) {
            gpuEmitterPendingSettingsPath_ = currentSavePath;
            ImGui::OpenPopup("Overwrite Preset?");
        } else {
            SaveGpuEmitterSettingsFromImGui(currentSavePath);
        }
    }
    ImGui::EndDisabled();
    if (!validSaveName) {
        ImGui::TextWrapped("Invalid save name");
    }
    ImGui::BeginDisabled(!FileUtility::Exists(selectedSettingsPath));
    if (ImGui::Button("Delete Preset", ImVec2(-FLT_MIN, 0.0f))) {
        gpuEmitterPendingSettingsPath_ = selectedSettingsPath;
        ImGui::OpenPopup("Delete Preset?");
    }
    ImGui::EndDisabled();

    ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Overwrite Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f);
        ImGui::Text("Overwrite: %s", FileUtility::GetStem(gpuEmitterPendingSettingsPath_).c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Overwrite", ImVec2(-FLT_MIN, 0.0f))) {
            SaveGpuEmitterSettingsFromImGui(gpuEmitterPendingSettingsPath_);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel", ImVec2(-FLT_MIN, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Delete Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f);
        ImGui::Text("Delete: %s", FileUtility::GetStem(gpuEmitterPendingSettingsPath_).c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Delete", ImVec2(-FLT_MIN, 0.0f))) {
            DeleteGpuEmitterSettingsFromImGui(gpuEmitterPendingSettingsPath_);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel", ImVec2(-FLT_MIN, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/// <summary>
/// GPU Emitter設定を指定パスへ保存して結果メッセージを更新する。
/// </summary>
void ParticleManager::SaveGpuEmitterSettingsFromImGui(const std::string& saveSettingsPath)
{
    const bool willOverwrite = FileUtility::Exists(saveSettingsPath); // 既存ファイルを上書きするか
    std::string saveError; // 保存失敗の詳細
    if (SaveGpuEmitterSettings(saveSettingsPath, &saveError)) {
        gpuEmitterLoadedSettingsName_ = FileUtility::GetStem(saveSettingsPath);
        gpuEmitterSelectedSettingsPath_ = saveSettingsPath;
        gpuEmitterSettingsMessage_ = std::string(willOverwrite ? "Overwritten: " : "Saved: ") + saveSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Save failed: " + saveSettingsPath + " / " + saveError;
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
        gpuEmitterSelectedSettingsPath_ = loadSettingsPath;
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
        gpuEmitterSelectedSettingsPath_.clear();
    } else {
        gpuEmitterSettingsMessage_ = "Delete failed: " + selectedSettingsPath;
    }
}

/// <summary>
/// ImGuiでGPU Emitterの実行操作を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterControlImGui()
{
    ImGui::BeginDisabled(!gpuParticleReady_ || gpuEmitterState_.count == 0);
    if (ImGui::Button("Emit Once", ImVec2(-FLT_MIN, 0.0f))) {
        gpuEmitterManualEmitRequested_ = true;
    }
    if (ImGui::Button("Restart Preview", ImVec2(-FLT_MIN, 0.0f))) {
        ResetGpuEmitterParticles();
        gpuEmitterManualEmitRequested_ = true;
    }
    ImGui::EndDisabled();
    if (ImGui::Button("Clear Particles", ImVec2(-FLT_MIN, 0.0f))) {
        ResetGpuEmitterParticles();
        gpuEmitterManualEmitRequested_ = false;
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterImGui(PostProcess* postProcess)
{
    ImGui::TextWrapped("%s", gpuEmitterEffectName_.c_str());
    DrawGpuEmitterControlImGui();
    DrawGpuEmitterPlaybackStateImGui();
    DrawGpuEmitterStateImGui();
    if (ImGui::CollapsingHeader("Effect / Texture")) {
        DrawGpuEmitterEffectImGui();
    }
    if (ImGui::CollapsingHeader("Presets")) {
        const std::string currentPreset = gpuEmitterLoadedSettingsName_.empty() ? "None" : gpuEmitterLoadedSettingsName_; // 現在の設定名
        ImGui::TextWrapped("Current: %s", currentPreset.c_str());
        DrawGpuEmitterSettingsFileImGui();
    }
    if (ImGui::CollapsingHeader("PostProcess")) {
        DrawGpuEmitterPostProcessImGui(postProcess);
    }
    if (ImGui::CollapsingHeader("GPU Status")) {
        DrawGpuEmitterStatusImGui();
    }
    if (!gpuEmitterSettingsMessage_.empty()) {
        ImGui::TextWrapped("%s", gpuEmitterSettingsMessage_.c_str());
    }
}
#endif

/// <summary>
/// ImGuiでパーティクル設定を編集する
/// </summary>
void ParticleManager::DrawImGui(PostProcess* postProcess)
{
#ifdef USE_IMGUI
    if (ImGui::BeginTabBar("ParticleEditorTabs")) {
        if (ImGui::BeginTabItem("GPU")) {
            ImGui::PushID("GpuParticleEditor");
            DrawGpuEmitterImGui(postProcess);
            ImGui::PopID();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CPU")) {
            ImGui::PushID("CpuParticleEditor");
            DrawCpuParticleImGui();
            ImGui::PopID();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
#else
    (void)postProcess;
#endif
}

/// <summary>
/// ImGuiでCPUパーティクルの共通設定とグループを編集する。
/// </summary>
void ParticleManager::DrawCpuParticleImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Groups: %zu", particleGroups_.size());

    if (ImGui::CollapsingHeader("Lifetime")) {
        DrawParticlePropertyLabel("Life Min / Max (s)");
        ImGui::DragFloatRange2(
            "##LifeMinMax",
            &lifeMin_,
            &lifeMax_,
            kImGuiFineStep,
            kImGuiLifeMin,
            kImGuiLifeMax);
    }

    if (ImGui::CollapsingHeader("Spawn Random")) {
        DrawParticlePropertyLabel("Spawn Position Min");
        ImGui::DragFloat3(
            "##SpawnPosMin",
            &spawnPosMin_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        DrawParticlePropertyLabel("Spawn Position Max");
        ImGui::DragFloat3(
            "##SpawnPosMax",
            &spawnPosMax_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        DrawParticlePropertyLabel("Scale Min");
        ImGui::DragFloat3(
            "##ScaleMin",
            &scaleMin_.x,
            kImGuiFineStep,
            kImGuiScaleMin,
            kImGuiScaleMax);
        DrawParticlePropertyLabel("Scale Max");
        ImGui::DragFloat3(
            "##ScaleMax",
            &scaleMax_.x,
            kImGuiFineStep,
            kImGuiScaleMin,
            kImGuiScaleMax);
    }

    if (ImGui::CollapsingHeader("Velocity / Physics")) {
        DrawParticlePropertyLabel("Velocity Min");
        ImGui::DragFloat3(
            "##VelMin",
            &velMin_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        DrawParticlePropertyLabel("Velocity Max");
        ImGui::DragFloat3(
            "##VelMax",
            &velMax_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);

        ImGui::Checkbox("Enable Gravity", &gravityEnabled_);
        ImGui::BeginDisabled(!gravityEnabled_);
        DrawParticlePropertyLabel("Gravity");
        ImGui::DragFloat3(
            "##Gravity",
            &gravity_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::EndDisabled();
        DrawParticlePropertyLabel("Damping");
        ImGui::DragFloat(
            "##Damping",
            &damping_,
            kImGuiFineStep,
            kImGuiDampingMin,
            kImGuiDampingMax);
    }

    if (ImGui::CollapsingHeader("Field")) {
        ImGui::Checkbox("Enable Field", &fieldEnabled_);
        ImGui::BeginDisabled(!fieldEnabled_);
        DrawParticlePropertyLabel("Field Acceleration");
        ImGui::DragFloat3(
            "##FieldAccel",
            &fieldAccel_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        DrawParticlePropertyLabel("Field Min");
        ImGui::DragFloat3(
            "##FieldMin",
            &fieldMin_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        DrawParticlePropertyLabel("Field Max");
        ImGui::DragFloat3(
            "##FieldMax",
            &fieldMax_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Color")) {
        DrawParticlePropertyLabel("Color Min");
        ImGui::ColorEdit4("##ColorMin", &colMin_.x, ImGuiColorEditFlags_Float);
        DrawParticlePropertyLabel("Color Max");
        ImGui::ColorEdit4("##ColorMax", &colMax_.x, ImGuiColorEditFlags_Float);
    }

    if (ImGui::CollapsingHeader("Groups")) {
        for (auto& kv : particleGroups_) {
            if (ImGui::TreeNode(kv.first.c_str())) {
                ImGui::Text("Count = %zu", kv.second.particles.size());
                ImGui::TextWrapped("Texture = %s", kv.second.texturePath.c_str());
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

                    ImGui::TextWrapped("Center = %.2f, %.2f, %.2f", centerPosition.x, centerPosition.y, centerPosition.z);
                    ImGui::TextWrapped("Min = %.2f, %.2f, %.2f", minimumPosition.x, minimumPosition.y, minimumPosition.z);
                    ImGui::TextWrapped("Max = %.2f, %.2f, %.2f", maximumPosition.x, maximumPosition.y, maximumPosition.z);
                    if (firstParticle) {
                        const Vector3& firstPosition = firstParticle->transform.translate; // 先頭パーティクルの座標
                        ImGui::TextWrapped("First = %.2f, %.2f, %.2f", firstPosition.x, firstPosition.y, firstPosition.z);
                        const Vector3& firstScale = firstParticle->transform.scale; // 先頭パーティクルのスケール
                        const Vector4& firstColor = firstParticle->color; // 先頭パーティクルの色
                        ImGui::TextWrapped("Scale = %.2f, %.2f, %.2f", firstScale.x, firstScale.y, firstScale.z);
                        ImGui::TextWrapped("Color = %.2f, %.2f, %.2f, %.2f", firstColor.x, firstColor.y, firstColor.z, firstColor.w);
                    }
                }

                ImGui::Checkbox("Use Billboard", &kv.second.useBillboard);
                ImGui::TreePop();
            }
        }
    }
#endif
}
