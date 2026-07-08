#pragma once

#include "../../engine/base/PostProcess.h"

/// <summary>
/// 時間停止エフェクトの進行状態
/// </summary>
enum class TimeStopPhase {
    Idle,
    Entering,
    Stopped,
    Releasing,
};

/// <summary>
/// 時間停止エフェクトの調整値
/// </summary>
struct TimeStopSettings {
    float enterDuration = 0.20f; // 時間停止へ移行する時間
    float stopDuration = 1.50f; // 時間を停止する時間
    float releaseDuration = 0.30f; // 時間再開の演出時間
    float distortionStrength = 0.045f; // 画面歪みの強さ
    float distortionRadius = 0.65f; // 画面歪みの影響範囲
    float distortionWaveCount = 5.0f; // 画面歪みの波数
    int startRingCount = 3; // 停止開始時に発生するリング数
    int releaseRingCount = 4; // 時間再開時に発生するリング数
    int releaseFragmentCount = 16; // 時間再開時に発生する破片数
    Math::Vector3 effectPosition { 0.0f, 1.0f, 0.0f }; // エフェクト発生位置
};

/// <summary>
/// 時間停止エフェクトを管理するクラス
/// </summary>
class TimeStopEffect {
public:
    /// <summary>
    /// エフェクトの進行状態を初期状態へ戻す
    /// </summary>
    void ResetState();

    /// <summary>
    /// 調整値を初期値へ戻す
    /// </summary>
    void ResetSettings();

    /// <summary>
    /// 時間停止エフェクトを開始する
    /// </summary>
    void Start(MyEngine::PostProcess& postProcess, const Math::Vector2& effectCenter);

    /// <summary>
    /// 時間停止エフェクトの状態を更新する
    /// </summary>
    void Update(float deltaTime, MyEngine::PostProcess& postProcess);

    /// <summary>
    /// ImGuiで時間停止エフェクトの状態と調整値を表示する
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 時間停止エフェクトが再生中か確認する
    /// </summary>
    bool IsPlaying() const;

    /// <summary>
    /// 時間停止中か確認する
    /// </summary>
    bool IsStopped() const;

    /// <summary>
    /// エフェクト発生位置を取得する
    /// </summary>
    const Math::Vector3& GetEffectPosition() const;

private:
    /// <summary>
    /// 現在のフェーズ名を取得する
    /// </summary>
    const char* GetPhaseName() const;

    /// <summary>
    /// 時間停止開始時の歪みポストエフェクトを設定する。
    /// </summary>
    void ConfigureDistortionPostProcess(MyEngine::PostProcess& postProcess, const Math::Vector2& effectCenter);

    /// <summary>
    /// 開始フェーズのポストエフェクト状態を反映する。
    /// </summary>
    void ApplyEnteringPostProcess(MyEngine::PostProcess& postProcess, float progress);

    /// <summary>
    /// 停止フェーズのポストエフェクト状態を反映する。
    /// </summary>
    void ApplyStoppedPostProcess(MyEngine::PostProcess& postProcess, bool resetDistortionStrength);

    /// <summary>
    /// 再開フェーズのポストエフェクト状態を反映する。
    /// </summary>
    void ApplyReleasingPostProcess(MyEngine::PostProcess& postProcess, float progress);

    TimeStopPhase phase_ = TimeStopPhase::Idle; // 現在の時間停止状態
    float phaseTime_ = 0.0f; // 現在フェーズでの経過時間
    TimeStopSettings settings_; // 時間停止の調整値
    MyEngine::PostEffectType previousPostEffect_ = MyEngine::PostEffectType::Copy; // 再生前のポストエフェクト
};
