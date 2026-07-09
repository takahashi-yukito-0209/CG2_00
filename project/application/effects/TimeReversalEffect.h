#pragma once

#include "../../engine/base/PostProcess.h"

#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace MyEngine {
class Object3d;
class Sprite;
class SpriteCommon;
}

/// <summary>
/// 時間逆行エフェクトの進行状態
/// </summary>
enum class TimeReversalPhase {
    Idle,
    Expanding,
    Paused,
    Rewinding,
    Converging,
};

/// <summary>
/// 時間逆行用パーティクル
/// </summary>
struct TimeReversalParticle {
    Math::Vector3 position; // 現在のワールド座標
    Math::Vector3 velocity; // 拡散時の移動速度
    Math::Vector3 rewindStartPosition; // 巻き戻し開始時のワールド座標
    float elapsedTime = 0.0f; // 発生からの経過時間
    float lifeTime = 1.0f; // 拡散を続ける時間
};

/// <summary>
/// 時間逆行エフェクトの調整値
/// </summary>
struct TimeReversalSettings {
    int particleCount = 32; // 発生する粒子数
    float minSpeed = 1.5f; // 最小拡散速度
    float maxSpeed = 4.0f; // 最大拡散速度
    float expansionDuration = 0.8f; // 通常拡散を行う時間
    float pauseDuration = 0.35f; // 拡散後に空中停止する時間
    float rewindDuration = 0.65f; // 発生地点へ巻き戻る時間
    int rewindAfterimageCount = 3; // 巻き戻し中に表示する残像数
    float rewindAfterimageSpacing = 0.18f; // 残像同士の軌跡間隔
    float rewindAfterimageAlpha = 0.35f; // 最も濃い残像の透明度
    float distortionStrength = -0.035f; // 巻き戻し中の画面歪み強度
    float distortionRadius = 0.45f; // 画面歪みの影響範囲
    float distortionWaveCount = 4.0f; // 画面歪みの波数
    float convergenceDuration = 0.22f; // 収束演出を表示する時間
    float convergenceFlashSize = 180.0f; // 収束時のフラッシュ最大サイズ
    float convergenceFlashAlpha = 0.9f; // 収束時のフラッシュ透明度
    int convergenceRingCount = 2; // 収束時に発生するリング数
    float transformHistoryDuration = 2.0f; // 3D対象のTransform履歴保持時間
    float particleSize = 24.0f; // 粒子スプライトの大きさ
    Math::Vector4 particleColor { 0.45f, 0.85f, 1.0f, 0.9f }; // 粒子の色
    Math::Vector3 effectPosition { 0.0f, 1.0f, 0.0f }; // エフェクト発生位置
};

/// <summary>
/// 時間逆行エフェクトを管理するクラス
/// </summary>
class TimeReversalEffect {
public:
    using ScreenUvResolver = std::function<Math::Vector2(const Math::Vector3&)>;

    /// <summary>
    /// エフェクトの進行状態を初期状態へ戻す
    /// </summary>
    void ResetState();

    /// <summary>
    /// 調整値を初期値へ戻す
    /// </summary>
    void ResetSettings();

    /// <summary>
    /// 時間逆行エフェクトを開始する
    /// </summary>
    void Start(MyEngine::PostProcess& postProcess, size_t maximumSpriteCount);

    /// <summary>
    /// 時間逆行エフェクトの状態を更新する
    /// </summary>
    void Update(float deltaTime, MyEngine::PostProcess& postProcess, std::vector<std::unique_ptr<MyEngine::Object3d>>& objects3d);

    /// <summary>
    /// 時間逆行用スプライトを更新する
    /// </summary>
    void UpdateSprites(
        std::vector<std::unique_ptr<MyEngine::Sprite>>& particleSprites,
        std::vector<std::unique_ptr<MyEngine::Sprite>>& afterimageSprites,
        MyEngine::Sprite* convergenceSprite,
        const ScreenUvResolver& screenUvResolver);

    /// <summary>
    /// 巻き戻し対象のTransform履歴を更新する
    /// </summary>
    void UpdateTransformHistory(std::vector<std::unique_ptr<MyEngine::Object3d>>& objects3d, bool canRecordHistory);

    /// <summary>
    /// 時間逆行用スプライトを描画する
    /// </summary>
    void DrawParticles(
        MyEngine::SpriteCommon* spriteCommon,
        std::vector<std::unique_ptr<MyEngine::Sprite>>& particleSprites,
        std::vector<std::unique_ptr<MyEngine::Sprite>>& afterimageSprites,
        MyEngine::Sprite* convergenceSprite);

    /// <summary>
    /// ImGuiで時間逆行エフェクトの状態と調整値を表示する
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 時間逆行エフェクトが再生中か確認する
    /// </summary>
    bool IsPlaying() const;

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
    /// Transform履歴を対象オブジェクトへ適用する
    /// </summary>
    void ApplyTransform(std::vector<std::unique_ptr<MyEngine::Object3d>>& objects3d);

    /// <summary>
    /// 収束フェーズを開始する
    /// </summary>
    void StartConvergence();

    /// <summary>
    /// 時間逆行開始時のポストエフェクト状態を設定する。
    /// </summary>
    void ConfigureInitialPostProcess(MyEngine::PostProcess& postProcess);

    /// <summary>
    /// 巻き戻しフェーズのポストエフェクト状態を反映する。
    /// </summary>
    void ApplyRewindingPostProcess(MyEngine::PostProcess& postProcess, float rewindRate);

    /// <summary>
    /// 収束フェーズのポストエフェクト状態を反映する。
    /// </summary>
    void ApplyConvergingPostProcess(MyEngine::PostProcess& postProcess, float convergenceRate);

    /// <summary>
    /// 再生前のポストエフェクト状態へ戻す。
    /// </summary>
    void RestorePreviousPostProcess(MyEngine::PostProcess& postProcess);

    TimeReversalPhase phase_ = TimeReversalPhase::Idle; // 現在の時間逆行状態
    float phaseTime_ = 0.0f; // 現在フェーズでの経過時間
    TimeReversalSettings settings_; // 時間逆行の調整値
    MyEngine::PostEffectType previousPostEffect_ = MyEngine::PostEffectType::Copy; // 再生前のポストエフェクト
    std::vector<TimeReversalParticle> particles_; // 時間逆行用パーティクル
    std::deque<Math::Transform> transformHistory_; // 巻き戻し対象のTransform履歴
    std::mt19937 random_ { std::random_device {}() }; // 粒子生成に使用する乱数
};
