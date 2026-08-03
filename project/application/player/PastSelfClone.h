#pragma once

#include "PastSelfRecorder.h"

#include <memory>
#include <string>
#include <vector>

namespace MyEngine {
class ImGuiManager;
class Object3d;
class Object3dCommon;
}

/// <summary>
/// 記録済みプレイヤー状態を分身として再生するクラス
/// </summary>
class PastSelfClone {
public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PastSelfClone();

    /// <summary>
    /// 分身表示用の3Dオブジェクトを初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const std::string& modelFileName);

    /// <summary>
    /// 分身が保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 記録済みフレームの再生を開始する。
    /// </summary>
    bool Start(const std::vector<PastSelfFrame>& sourceFrames);

    /// <summary>
    /// 分身再生を停止する。
    /// </summary>
    void Stop();

    /// <summary>
    /// 分身の再生状態を更新する。
    /// </summary>
    void Update(float deltaTime, const std::vector<StandablePlatform>& standablePlatforms);

    /// <summary>
    /// 現在の分身状態を表示用Object3dへ反映する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 分身を描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// ImGuiで分身状態を表示する。
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 現在の分身状態を取得する。
    /// </summary>
    const PlayerState& GetCurrentState() const { return currentState_; }

    /// <summary>
    /// 分身を上面足場として扱うための情報を取得する。
    /// </summary>
    StandablePlatform GetStandablePlatform() const;

    /// <summary>
    /// 分身が表示対象か取得する。
    /// </summary>
    bool IsVisible() const { return isVisible_; }

private:
    /// <summary>
    /// 指定時刻のプレイヤー状態を取得する。
    /// </summary>
    PlayerState SampleState(float playbackTime) const;

    /// <summary>
    /// 上面足場への着地だけを再生後の分身状態へ反映する。
    /// </summary>
    void ResolveLandingOnPlatforms(float previousFootY, const std::vector<StandablePlatform>& standablePlatforms);

    /// <summary>
    /// 再生状態に応じた分身表示色を反映する。
    /// </summary>
    void ApplyStateMaterialColor();

    std::unique_ptr<MyEngine::Object3d> object3d_; // 分身表示用の3Dオブジェクト
    std::vector<PastSelfFrame> frames_; // 再生に使用する記録フレーム
    PlayerState currentState_; // 現在の分身状態
    Math::Vector4 materialColor_ { 0.35f, 0.8f, 1.0f, 0.45f }; // 再生中の分身表示色
    Math::Vector4 finishedMaterialColor_ { 0.75f, 0.8f, 0.9f, 0.28f }; // 再生終了後の分身表示色
    float playbackTime_ = 0.0f; // 現在の再生時刻
    float playbackSpeed_ = 1.0f; // 再生速度
    bool loopPlayback_ = false; // 終端でループするか
    bool isPlaying_ = false; // 再生中か
    bool isVisible_ = false; // 描画するか
    bool standableEnabled_ = true; // 分身を上面足場として使うか
    bool smoothPlayback_ = true; // フレーム間を滑らかに補間するか
    bool useStateMaterialColor_ = true; // 再生状態に応じて色を変えるか
};