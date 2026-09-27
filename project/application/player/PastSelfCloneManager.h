#pragma once

#include "PastSelfClone.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MyEngine {
class ImGuiManager;
class Object3dCommon;
}

/// <summary>
/// 複数の分身を所有し、一括して再生・更新・描画するクラス
/// </summary>
class PastSelfCloneManager {
public:
    /// <summary>
    /// 分身生成に使用する描画環境とモデルを設定する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const std::string& modelFileName);

    /// <summary>
    /// すべての分身と保持中の描画環境を解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 記録済みフレームから新しい分身を追加する。
    /// </summary>
    bool AddClone(const std::vector<PastSelfFrame>& sourceFrames);

    /// <summary>
    /// 保存済み分身をすべて削除する。
    /// </summary>
    void Clear();

    /// <summary>
    /// 保存済み分身を先頭から同時に再生する。
    /// </summary>
    bool StartAll();

    /// <summary>
    /// すべての分身を停止して非表示にする。
    /// </summary>
    void StopAll();

    /// <summary>
    /// すべての分身を表示したまま停止する。
    /// </summary>
    void PauseAll();

    /// <summary>
    /// すべての分身の再生状態を更新する。
    /// </summary>
    void Update(float deltaTime, const std::vector<StandablePlatform>& standablePlatforms);

    /// <summary>
    /// すべての可視分身を表示用オブジェクトへ反映する。
    /// </summary>
    void UpdateObjects(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// すべての可視分身を描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// ImGuiで保存済み分身の状態を表示する。
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 可視中の分身状態一覧を取得する。
    /// </summary>
    std::vector<PlayerState> GetVisibleStates() const;

    /// <summary>
    /// プレイヤーが乗れる可視分身の足場一覧を取得する。
    /// </summary>
    std::vector<StandablePlatform> GetStandablePlatforms() const;

    /// <summary>
    /// 保存済み分身数を取得する。
    /// </summary>
    size_t GetCloneCount() const { return clones_.size(); }

    /// <summary>
    /// 可視中の分身数を取得する。
    /// </summary>
    size_t GetVisibleCount() const;

    /// <summary>
    /// 再生中の分身数を取得する。
    /// </summary>
    size_t GetPlayingCount() const;

private:
    MyEngine::Object3dCommon* object3dCommon_ = nullptr; // 分身オブジェクト生成に使用する共通描画環境
    MyEngine::ImGuiManager* imguiManager_ = nullptr; // 分身オブジェクトへ渡すImGui管理クラス
    std::string modelFileName_; // 分身表示に使用するモデルファイル名
    std::vector<std::unique_ptr<PastSelfClone>> clones_; // 保存済みの分身一覧
};
