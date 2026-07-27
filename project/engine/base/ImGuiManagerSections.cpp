#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace MyEngine {
namespace {
/// <summary>
/// パス文字列から表示用のファイル名だけを取得する。
/// </summary>
std::string GetDisplayFileName(const std::string& path)
{
    const size_t slashPosition = path.find_last_of("/\\"); // 最後の区切り文字位置
    if (slashPosition == std::string::npos) {
        return path;
    }
    return path.substr(slashPosition + 1);
}

/// <summary>
/// 3DオブジェクトのImGui表示名を作成する。
/// </summary>
std::string BuildObjectDisplayLabel(const Object3d* object, int objectIndex)
{
    std::string label = "Object " + std::to_string(objectIndex); // indexベースの表示名
    if (object) {
        label += " [ID " + std::to_string(object->GetObjectId()) + "]";
        const std::string displayName = GetDisplayFileName(object->GetDebugName()); // モデル由来の表示名
        if (!displayName.empty()) {
            label += " : " + displayName;
        }
    }
    return label;
}

/// <summary>
/// スプライトのImGui表示名を作成する。
/// </summary>
std::string BuildSpriteDisplayLabel(const Sprite* sprite, int spriteIndex)
{
    std::string label = "Sprite " + std::to_string(spriteIndex); // indexベースの表示名
    if (sprite) {
        label += " [ID " + std::to_string(sprite->GetSpriteId()) + "]";
        const std::string displayName = GetDisplayFileName(sprite->GetTextureFilePath()); // テクスチャ由来の表示名
        if (!displayName.empty()) {
            label += " : " + displayName;
        }
    }
    return label;
}

/// <summary>
/// パーティクルエミッターのImGui表示名を作成する。
/// </summary>
std::string BuildParticleEmitterDisplayLabel(const ParticleEmitter* emitter, int emitterIndex)
{
    std::string label = "Emitter " + std::to_string(emitterIndex); // indexベースの表示名
    if (emitter) {
        label += " [ID " + std::to_string(emitter->GetEmitterId()) + "]";
        if (!emitter->groupName.empty()) {
            label += " : " + emitter->groupName;
        }
    }
    return label;
}
} // namespace

/// <summary>
/// シーン情報のImGuiを描画する
/// </summary>
void ImGuiManager::DrawSceneSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ctx.currentSceneName) {
            ImGui::Text("Current Scene: %s", ctx.currentSceneName);
        }

        if (ctx.requestSceneChange) {
            const char* sceneNames[] = {
                "Title",
                "Play",
            }; // ImGuiから切り替え可能なシーン名
            constexpr int sceneCount = static_cast<int>(sizeof(sceneNames) / sizeof(sceneNames[0])); // シーン数
            int selectedSceneIndex = 0; // 現在選択されているシーン番号
            for (int sceneIndex = 0; sceneIndex < sceneCount; ++sceneIndex) {
                if (ctx.currentSceneName && std::strcmp(ctx.currentSceneName, sceneNames[sceneIndex]) == 0) {
                    selectedSceneIndex = sceneIndex;
                    break;
                }
            }
            if (ImGui::Combo("Scene##SceneSelector", &selectedSceneIndex, sceneNames, sceneCount)) {
                ctx.requestSceneChange(sceneNames[selectedSceneIndex]);
            }
        }
        ImGui::Separator();
        const char* targetLabels[] = {
            "Object3D",
            "Sprite2D",
            "Emitter",
            "GPU Emitter"
        }; // 選択可能なギズモ対象
        gizmoTargetMode_ = (std::clamp)(gizmoTargetMode_, 0, static_cast<int>(IM_ARRAYSIZE(targetLabels)) - 1);
        ImGui::Combo("Gizmo Target", &gizmoTargetMode_, targetLabels, IM_ARRAYSIZE(targetLabels));

        if (gizmoTargetMode_ == 3) {
            const char* gpuOperationLabels[] = {
                "Move",
                "Scale"
            }; // GPU Emitterで選択可能なギズモ操作
            int gpuOperationIndex = gizmoOperationMode_ == 2 ? 1 : 0; // GPU Emitter用の操作番号
            if (ImGui::Combo("Gizmo Operation", &gpuOperationIndex, gpuOperationLabels, IM_ARRAYSIZE(gpuOperationLabels))) {
                gizmoOperationMode_ = gpuOperationIndex == 0 ? 0 : 2;
            }
            if (gizmoOperationMode_ != 2) {
                gizmoOperationMode_ = 0;
            }
        } else {
            const char* operationLabels[] = {
                "Move",
                "Rotate",
                "Scale"
            }; // 選択可能なギズモ操作
            gizmoOperationMode_ = (std::clamp)(gizmoOperationMode_, 0, static_cast<int>(IM_ARRAYSIZE(operationLabels)) - 1);
            ImGui::Combo("Gizmo Operation", &gizmoOperationMode_, operationLabels, IM_ARRAYSIZE(operationLabels));
        }

        if (gizmoTargetMode_ == 0 || gizmoTargetMode_ == 2) {
            const char* spaceLabels[] = {
                "Global",
                "Local"
            }; // 選択可能なギズモ座標空間
            gizmoTransformSpaceMode_ = (std::clamp)(gizmoTransformSpaceMode_, 0, static_cast<int>(IM_ARRAYSIZE(spaceLabels)) - 1);
            ImGui::Combo("Gizmo Space", &gizmoTransformSpaceMode_, spaceLabels, IM_ARRAYSIZE(spaceLabels));
        }

        DirectXCommon* directXCommon = DirectXCommon::GetInstance(); // フレーム同期設定の反映先
        if (directXCommon) {
            const char* frameSyncLabels[] = {
                "VSync",
                "Fixed 60",
                "Unlimited"
            }; // 選択可能なフレーム同期モード
            int frameSyncIndex = static_cast<int>(directXCommon->GetFrameSyncMode()); // ImGui編集用の同期モード番号
            frameSyncIndex = (std::clamp)(frameSyncIndex, 0, static_cast<int>(IM_ARRAYSIZE(frameSyncLabels)) - 1);
            if (ImGui::Combo("Frame Sync", &frameSyncIndex, frameSyncLabels, IM_ARRAYSIZE(frameSyncLabels))) {
                directXCommon->SetFrameSyncMode(static_cast<DirectXCommon::FrameSyncMode>(frameSyncIndex));
            }
        }

        const float frameRate = ImGui::GetIO().Framerate; // 現在のImGui計測FPS
        const float frameTimeMs = frameRate > 0.0f ? 1000.0f / frameRate : 0.0f; // 1フレームあたりの表示時間
        ImGui::Text("FPS: %.1f (%.3f ms)", frameRate, frameTimeMs);
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// パーティクル関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawParticleSection(Context& ctx)
{
#ifdef USE_IMGUI
    const bool hasCpuEmitters = ctx.particleEmitters && !ctx.particleEmitters->empty(); // CPUエミッター一覧があるか
    if (!hasCpuEmitters && !ctx.particleEmitter && !ctx.particleManager) {
        return;
    }

    if (ImGui::CollapsingHeader("Particle")) {
        if (hasCpuEmitters) {
            const int emitterCount = static_cast<int>(ctx.particleEmitters->size()); // 編集可能なCPUエミッター数
            selectedEmitterIndex_ = (std::clamp)(selectedEmitterIndex_, 0, emitterCount - 1);
            ParticleEmitter* previewEmitter = (*ctx.particleEmitters)[selectedEmitterIndex_]; // コンボの現在選択エミッター
            std::string preview = BuildParticleEmitterDisplayLabel(previewEmitter, selectedEmitterIndex_); // コンボの現在表示名

            if (ImGui::BeginCombo("CPU Emitter Target", preview.c_str())) {
                for (int emitterIndex = 0; emitterIndex < emitterCount; ++emitterIndex) {
                    ParticleEmitter* emitter = (*ctx.particleEmitters)[emitterIndex]; // 表示名を作るエミッター
                    std::string label = BuildParticleEmitterDisplayLabel(emitter, emitterIndex); // 選択候補の表示名
                    const bool isSelected = selectedEmitterIndex_ == emitterIndex; // 現在選択中か
                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        selectedEmitterIndex_ = emitterIndex;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ParticleEmitter* selectedEmitter = (*ctx.particleEmitters)[selectedEmitterIndex_]; // 編集対象のCPUエミッター
            if (selectedEmitter) {
                ImGui::PushID(selectedEmitter);
                selectedEmitter->DrawImGui();
                ImGui::PopID();
            }
        } else if (ctx.particleEmitter) {
            ImGui::PushID(ctx.particleEmitter);
            ctx.particleEmitter->DrawImGui();
            ImGui::PopID();
        }

        ImGui::Separator();

        if (ctx.particleManager) {
            ctx.particleManager->DrawImGui(ctx.postProcess);
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// 3Dオブジェクト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawObjectSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.objects3d || ctx.objects3d->empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Objects")) {
        const int objectCount = static_cast<int>(ctx.objects3d->size()); // 表示可能な3Dオブジェクト数
        selectedObjectIndex_ = (std::clamp)(selectedObjectIndex_, 0, objectCount - 1);

        Object3d* previewObject = (*ctx.objects3d)[selectedObjectIndex_]; // コンボで現在選択している3Dオブジェクト
        std::string preview = BuildObjectDisplayLabel(previewObject, selectedObjectIndex_); // コンボの現在表示名

        if (ImGui::BeginCombo("Target", preview.c_str())) {
            for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
                Object3d* object = (*ctx.objects3d)[objectIndex]; // 表示名を取得する3Dオブジェクト
                std::string label = BuildObjectDisplayLabel(object, objectIndex); // 選択候補の表示名

                const bool isSelected = selectedObjectIndex_ == objectIndex; // 現在選択中か
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedObjectIndex_ = objectIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Object3d* selectedObject = (*ctx.objects3d)[selectedObjectIndex_]; // 編集対象の3Dオブジェクト
        if (selectedObject) {
            ImGui::Separator();
            selectedObject->DrawImGui(selectedObjectIndex_);
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// スプライト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawSpriteSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.sprites || ctx.sprites->empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Sprites")) {
        const int spriteCount = static_cast<int>(ctx.sprites->size()); // 表示可能なスプライト数
        selectedSpriteIndex_ = (std::clamp)(selectedSpriteIndex_, 0, spriteCount - 1);

        Sprite* previewSprite = (*ctx.sprites)[selectedSpriteIndex_]; // コンボで現在選択しているスプライト
        std::string preview = BuildSpriteDisplayLabel(previewSprite, selectedSpriteIndex_); // コンボの現在表示名

        if (ImGui::BeginCombo("Target", preview.c_str())) {
            for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
                Sprite* sprite = (*ctx.sprites)[spriteIndex]; // 表示名を取得するスプライト
                std::string label = BuildSpriteDisplayLabel(sprite, spriteIndex); // 選択候補の表示名

                const bool isSelected = selectedSpriteIndex_ == spriteIndex; // 現在選択中か
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedSpriteIndex_ = spriteIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Sprite* selectedSprite = (*ctx.sprites)[selectedSpriteIndex_]; // 編集対象のスプライト
        if (selectedSprite) {
            ImGui::Separator();
            selectedSprite->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// 共通設定関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCommonSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Common")) {
        if (ctx.spriteCommon || ctx.object3dCommon) {
            if (ImGui::CollapsingHeader("Blend Mode")) {
                const char* blendNames[] = {
                    "None",
                    "Alpha",
                    "Add",
                    "Subtract",
                    "Multiply",
                    "Screen"
                }; // 選択可能なブレンドモード名

                if (ctx.object3dCommon) {
                    int objectBlendIndex = static_cast<int>(ctx.object3dCommon->GetBlendMode()); // 3Dオブジェクトのブレンドモード
                    if (ImGui::Combo("1. Object3D Blend", &objectBlendIndex, blendNames, IM_ARRAYSIZE(blendNames))) {
                        ctx.object3dCommon->SetBlendMode(static_cast<BlendMode>(objectBlendIndex));
                    }
                }

                if (ctx.spriteCommon) {
                    int spriteBlendIndex = static_cast<int>(ctx.spriteCommon->GetBlendMode()); // スプライトのブレンドモード
                    if (ImGui::Combo("2. Sprite Blend", &spriteBlendIndex, blendNames, IM_ARRAYSIZE(blendNames))) {
                        ctx.spriteCommon->SetBlendMode(static_cast<BlendMode>(spriteBlendIndex));
                    }
                }
            }
        }

        if (ctx.object3dCommon) {
            ctx.object3dCommon->DrawImGui();
        }

        if (ctx.spriteCommon) {
            ctx.spriteCommon->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// カメラ関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCameraWindow(Context& ctx)
{
#ifdef USE_IMGUI
    ImGui::Begin("Camera");

    if (ctx.object3dCommon) {
        ctx.object3dCommon->DrawCameraImGui();
    }

    ImGui::End();
#else
    (void)ctx;
#endif
}

} // namespace MyEngine
#endif