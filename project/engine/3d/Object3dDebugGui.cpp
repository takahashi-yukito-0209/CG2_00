#include "Object3d.h"

#include "Model.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#include <algorithm>
#endif

using namespace MyEngine;

namespace {
constexpr float kEnvironmentCoefficientMin = 0.0f; // 環境反射係数の最小値
constexpr float kEnvironmentCoefficientMax = 1.0f; // 環境反射係数の最大値
constexpr float kImGuiTransformStep = 0.01f; // Transform調整幅
constexpr float kImGuiScaleMin = 0.001f; // Scale調整の最小値
constexpr float kImGuiScaleMax = 1000.0f; // Scale調整の最大値
}
/// <summary>
/// ImGuiでオブジェクトの状態を表示・編集する
/// </summary>
void Object3d::DrawImGui(int index)
{
#ifdef USE_IMGUI
    // 選択中オブジェクトの識別名を表示
    ImGui::Text("Object %d : %s", index, debugName_.c_str());
    ImGui::DragFloat3(
        "Scale",
        &transform_.scale.x,
        kImGuiTransformStep,
        kImGuiScaleMin,
        kImGuiScaleMax);
    ImGui::DragFloat3("Rotate", &transform_.rotate.x, kImGuiTransformStep);
    ImGui::DragFloat3("Translate", &transform_.translate.x, kImGuiTransformStep);

    // マテリアル編集
    if (materialData_) {
        ImGui::Checkbox("Use Alpha Cutout Sampler", &useAlphaCutoutSampler_);
        materialData_->useAlphaCutoutSampler = useAlphaCutoutSampler_ ? 1 : 0;
        ImGui::Checkbox("Use Alpha Discard", &useAlphaDiscard_);
        materialData_->useAlphaDiscard = useAlphaDiscard_ ? 1 : 0;
        // マテリアル色を編集
        float col[4] = { materialData_->color.x, materialData_->color.y, materialData_->color.z, materialData_->color.w };
        if (ImGui::ColorEdit4("Color", col)) {
            materialData_->color.x = col[0];
            materialData_->color.y = col[1];
            materialData_->color.z = col[2];
            materialData_->color.w = col[3];
        }
        ImGui::SliderFloat(
            "Environment Reflection",
            &materialData_->environmentCoefficient,
            kEnvironmentCoefficientMin,
            kEnvironmentCoefficientMax);
    }

    // アニメーション再生状態を編集
    if (ImGui::CollapsingHeader("Animation")) {
        ImGui::Text("Has Animation : %s", hasAnimation_ ? "true" : "false");
        ImGui::Text("Duration : %.3f", animation_.duration);
        ImGui::Text("Node Animations : %zu", animation_.nodeAnimations.size());
        if (ImGui::Checkbox("Animation Enabled", &animationEnabled_)) {
            if (animationEnabled_ && hasAnimation_) {
                ApplyAnimationAtCurrentTime();
            }
        }
        ImGui::DragFloat("Playback Speed", &animationPlaybackSpeed_, 0.01f, 0.0f, 10.0f);
        if (hasAnimation_ && animation_.duration > 0.0f) {
            if (ImGui::SliderFloat("Animation Time", &animationTime_, 0.0f, animation_.duration)) {
                ApplyAnimationAtCurrentTime();
            }
            if (ImGui::Button("Reset Animation")) {
                animationTime_ = 0.0f;
                ApplyAnimationAtCurrentTime();
            }
        }
    }

    // Skeletonの状態を編集
    if (ImGui::CollapsingHeader("Skeleton")) {
        ImGui::Text("Has Skeleton : %s", hasSkeleton_ ? "true" : "false");
        ImGui::Text("Joint Count : %zu", skeleton_.joints.size());
        if (hasSkeleton_ && !skeleton_.joints.empty()) {
            selectedJointIndex_ = std::clamp(selectedJointIndex_, 0, static_cast<int32_t>(skeleton_.joints.size() - 1));
            const Joint& selectedJoint = skeleton_.joints[selectedJointIndex_]; // 現在選択中のJoint
            if (ImGui::BeginCombo("Joint", selectedJoint.name.c_str())) {
                for (int32_t jointIndex = 0; jointIndex < static_cast<int32_t>(skeleton_.joints.size()); ++jointIndex) {
                    const bool isSelected = selectedJointIndex_ == jointIndex; // 選択中か
                    if (ImGui::Selectable(skeleton_.joints[jointIndex].name.c_str(), isSelected)) {
                        selectedJointIndex_ = jointIndex;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            Joint& joint = skeleton_.joints[selectedJointIndex_]; // 編集対象のJoint
            bool editedJoint = false; // JointのTransformが変更されたか
            editedJoint |= ImGui::DragFloat3("Joint Scale", &joint.transform.scale.x, 0.01f, 0.001f, 100.0f);
            editedJoint |= ImGui::DragFloat4("Joint Rotate Quaternion", &joint.transform.rotate.x, 0.01f, -1.0f, 1.0f);
            editedJoint |= ImGui::DragFloat3("Joint Translate", &joint.transform.translate.x, 0.01f);
            ImGui::Text("Parent : %d", joint.parent ? *joint.parent : -1);

            std::string childrenText; // 子Joint一覧の表示文字列
            for (size_t childIndex = 0; childIndex < joint.children.size(); ++childIndex) {
                if (childIndex > 0) {
                    childrenText += ", ";
                }
                childrenText += std::to_string(joint.children[childIndex]);
            }
            ImGui::Text("Children : %s", childrenText.empty() ? "none" : childrenText.c_str());

            if (editedJoint) {
                Update(skeleton_);
                UpdateSkinningPaletteResources();
            }
        }
    }

    // Skinning描画の状態を編集
    if (ImGui::CollapsingHeader("Skinning")) {
        ImGui::Checkbox("Skinning Enabled", &skinningEnabled_);
        ImGui::Text("Has SkinCluster : %s", hasSkinCluster_ ? "true" : "false");
        ImGui::Text("Can Use Skinning : %s", CanUseSkinning() ? "true" : "false");
        ImGui::Text("Palette Joints : %u", skinningPaletteJointCount_);
        const ModelData& sourceModelData = model_ ? model_->GetModelData() : modelData_; // 表示対象のモデルデータ
        ImGui::Text("Vertex Influences : %zu", sourceModelData.vertexInfluences.size());
        ImGui::Text("Skin Joints : %zu", sourceModelData.skinClusterData.size());
    }

    // Skeletonの状態を視覚的に確認する
    if (ImGui::CollapsingHeader("Skeleton Debug Draw")) {
        ImGui::PushID("SkeletonDebugDraw");
        ImGui::Checkbox("Enabled", &skeletonDebugDrawEnabled_);
        ImGui::DragFloat("Joint Radius", &skeletonDebugJointRadius_, 0.0005f, 0.001f, 0.08f);
        ImGui::DragFloat("Bone Radius", &skeletonDebugBoneRadius_, 0.0005f, 0.0005f, 0.04f);
        ImGui::ColorEdit4("Bone Color", &skeletonDebugBoneColor_.x);
        ImGui::ColorEdit4("Joint Color", &skeletonDebugJointColor_.x);
        ImGui::Text("Debug Vertices : %u", skeletonDebugVertexCounts_[0]);
        ImGui::PopID();
    }
#else
    (void)index;
    (void)materialData_;
    (void)useAlphaCutoutSampler_;
    (void)useAlphaDiscard_;
#endif
}
