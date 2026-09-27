#include "PastSelfCloneManager.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

#include <algorithm>
#include <array>

using namespace MyEngine;

namespace {
constexpr std::array<Math::Vector4, 4> kClonePlayingColors = { {
    { 1.0f, 0.05f, 0.95f, 0.78f },
    { 1.0f, 0.35f, 0.05f, 0.82f },
    { 0.65f, 0.2f, 1.0f, 0.8f },
    { 1.0f, 0.5f, 0.72f, 0.8f },
} }; // 保存順に割り当てる再生中の分身色
constexpr std::array<Math::Vector4, 4> kCloneFinishedColors = { {
    { 0.9f, 0.45f, 1.0f, 0.5f },
    { 1.0f, 0.62f, 0.28f, 0.52f },
    { 0.78f, 0.5f, 1.0f, 0.5f },
    { 1.0f, 0.7f, 0.84f, 0.5f },
} }; // 保存順に割り当てる再生終了後の分身色
}

/// <summary>
/// 分身生成に使用する描画環境とモデルを設定する。
/// </summary>
void PastSelfCloneManager::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager, const std::string& modelFileName)
{
    Clear();
    object3dCommon_ = object3dCommon;
    imguiManager_ = imguiManager;
    modelFileName_ = modelFileName;
}

/// <summary>
/// すべての分身と保持中の描画環境を解放する。
/// </summary>
void PastSelfCloneManager::Finalize()
{
    Clear();
    object3dCommon_ = nullptr;
    imguiManager_ = nullptr;
    modelFileName_.clear();
}

/// <summary>
/// 記録済みフレームから新しい分身を追加する。
/// </summary>
bool PastSelfCloneManager::AddClone(const std::vector<PastSelfFrame>& sourceFrames)
{
    if (!object3dCommon_ || modelFileName_.empty() || sourceFrames.size() < 2) {
        return false;
    }

    auto clone = std::make_unique<PastSelfClone>(); // 新しく保存する分身
    clone->Initialize(object3dCommon_, imguiManager_, modelFileName_);
    const size_t colorIndex = clones_.size() % kClonePlayingColors.size(); // 保存順から選択した分身色番号
    clone->SetMaterialColors(kClonePlayingColors[colorIndex], kCloneFinishedColors[colorIndex]);
    if (!clone->Load(sourceFrames)) {
        clone->Finalize();
        return false;
    }

    clones_.push_back(std::move(clone));
    return true;
}

/// <summary>
/// 保存済み分身をすべて削除する。
/// </summary>
void PastSelfCloneManager::Clear()
{
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone) {
            clone->Finalize();
        }
    }
    clones_.clear();
}

/// <summary>
/// 保存済み分身を先頭から同時に再生する。
/// </summary>
bool PastSelfCloneManager::StartAll()
{
    bool started = false; // 1体以上の再生を開始できたか
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone && clone->Start()) {
            started = true;
        }
    }
    return started;
}

/// <summary>
/// すべての分身を停止して非表示にする。
/// </summary>
void PastSelfCloneManager::StopAll()
{
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone) {
            clone->Stop();
        }
    }
}

/// <summary>
/// すべての分身を表示したまま停止する。
/// </summary>
void PastSelfCloneManager::PauseAll()
{
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone) {
            clone->Pause();
        }
    }
}

/// <summary>
/// すべての分身の再生状態を更新する。
/// </summary>
void PastSelfCloneManager::Update(float deltaTime, const std::vector<StandablePlatform>& standablePlatforms)
{
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone) {
            clone->Update(deltaTime, standablePlatforms);
        }
    }
}

/// <summary>
/// すべての可視分身を表示用オブジェクトへ反映する。
/// </summary>
void PastSelfCloneManager::UpdateObjects(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone) {
            clone->UpdateObject(viewMatrix, projectionMatrix);
        }
    }
}

/// <summary>
/// すべての可視分身を描画する。
/// </summary>
void PastSelfCloneManager::Draw()
{
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone) {
            clone->Draw();
        }
    }
}

/// <summary>
/// ImGuiで保存済み分身の状態を表示する。
/// </summary>
void PastSelfCloneManager::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Stored: %zu  Visible: %zu  Playing: %zu", GetCloneCount(), GetVisibleCount(), GetPlayingCount());
    for (size_t cloneIndex = 0; cloneIndex < clones_.size(); ++cloneIndex) {
        const std::unique_ptr<PastSelfClone>& clone = clones_[cloneIndex]; // 状態を表示する分身
        if (!clone) {
            continue;
        }

        ImGui::PushID(static_cast<int>(cloneIndex));
        if (ImGui::TreeNode("Clone", "Clone %zu", cloneIndex + 1)) {
            clone->DrawImGui();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
#endif
}

/// <summary>
/// 可視中の分身状態一覧を取得する。
/// </summary>
std::vector<PlayerState> PastSelfCloneManager::GetVisibleStates() const
{
    std::vector<PlayerState> states; // ギミック判定とカメラ計算に使用する分身状態一覧
    states.reserve(GetVisibleCount());
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (clone && clone->IsVisible()) {
            states.push_back(clone->GetCurrentState());
        }
    }
    return states;
}

/// <summary>
/// プレイヤーが乗れる可視分身の足場一覧を取得する。
/// </summary>
std::vector<StandablePlatform> PastSelfCloneManager::GetStandablePlatforms() const
{
    std::vector<StandablePlatform> platforms; // プレイヤー用の分身足場一覧
    platforms.reserve(GetVisibleCount());
    for (const std::unique_ptr<PastSelfClone>& clone : clones_) {
        if (!clone) {
            continue;
        }

        const StandablePlatform platform = clone->GetStandablePlatform(); // 現在の分身から作成した足場
        if (platform.enabled) {
            platforms.push_back(platform);
        }
    }
    return platforms;
}

/// <summary>
/// 可視中の分身数を取得する。
/// </summary>
size_t PastSelfCloneManager::GetVisibleCount() const
{
    return static_cast<size_t>(std::count_if(clones_.begin(), clones_.end(), [](const std::unique_ptr<PastSelfClone>& clone) {
        return clone && clone->IsVisible();
    }));
}

/// <summary>
/// 再生中の分身数を取得する。
/// </summary>
size_t PastSelfCloneManager::GetPlayingCount() const
{
    return static_cast<size_t>(std::count_if(clones_.begin(), clones_.end(), [](const std::unique_ptr<PastSelfClone>& clone) {
        return clone && clone->IsPlaying();
    }));
}
