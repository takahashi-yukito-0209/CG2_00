#pragma once

#include "../../engine/base/PostProcess.h"

#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace MyEngine {
class Camera;
class Object3d;
class Sprite;
class SpriteCommon;
}

/// <summary>
/// 時空破砕エフェクトの進行状態
/// </summary>
enum class TemporalRiftPhase {
    Idle,
    Compress,
    Freeze,
    Crack,
    Burst,
    Recover,
};

/// <summary>
/// 時空破砕エフェクトの調整値
/// </summary>
struct TemporalRiftSettings {
    float compressDuration = 0.18f; // 空間圧縮フェーズの時間
    float freezeDuration = 0.10f; // 時間停止フェーズの時間
    float crackDuration = 0.24f; // 亀裂展開フェーズの時間
    float burstDuration = 0.16f; // 破砕フェーズの時間
    float recoverDuration = 0.22f; // 復帰フェーズの時間
    float compressBlurStart = 0.005f; // 圧縮開始時のブラー幅
    float compressBlurEnd = 0.030f; // 圧縮終了時のブラー幅
    float burstBlurStrength = 0.050f; // 破砕時のブラー幅
    int blurSampleCount = 16; // ラジアルブラーのサンプル数
    float distortionRadius = 0.35f; // 画面歪みの影響範囲
    float compressDistortionStrength = -0.025f; // 圧縮時の内向き歪み強度
    float burstDistortionStrength = 0.045f; // 破砕時の外向き歪み強度
    float distortionWaveCount = 3.0f; // 画面歪みの波数
    int afterimageCount = 5; // 表示する残像数
    int afterimageFrameInterval = 3; // 残像同士の履歴間隔
    float afterimageSize = 90.0f; // 残像スプライトの大きさ
    float afterimageAlpha = 0.35f; // 残像の透明度
    float temporalDisplacement = 0.8f; // 対象へ加える時間ずれ移動量
    Math::Vector3 afterimageColor { 0.35f, 0.65f, 1.0f }; // 残像の色
    float hitStopDuration = 0.06f; // 破砕時に演出を停止する時間
    float cameraShakeDuration = 0.22f; // カメラを揺らす時間
    float cameraShakeStrength = 0.12f; // カメラシェイクの最大移動量
    float cameraShakeFrequency = 35.0f; // カメラシェイクの振動周波数
    int crackCount = 7; // 発生する亀裂数
    float crackLength = 1.20f; // 亀裂の基準長さ
    float crackLengthVariation = 0.35f; // 亀裂ごとの長さ変化
    float crackWidth = 0.055f; // 亀裂の基準幅
    float crackWidthVariation = 0.025f; // 亀裂ごとの幅変化
    float crackLifeTime = 0.65f; // 亀裂の表示時間
    Math::Vector4 crackColor { 0.55f, 0.80f, 1.0f, 1.0f }; // 亀裂の色
    int ringCount = 2; // 破砕時のリング数
    int fragmentCount = 12; // 破砕時の破片数
    Math::Vector4 innerRingColor { 0.35f, 0.85f, 1.0f, 0.85f }; // 内側リングの色
    Math::Vector4 outerRingColor { 0.65f, 0.35f, 1.0f, 0.70f }; // 外側リングの色
    Math::Vector4 fragmentColor { 0.55f, 0.85f, 1.0f, 1.0f }; // 放射破片の色
    float fragmentMinSpeed = 2.5f; // 放射破片の最小速度
    float fragmentMaxSpeed = 6.0f; // 放射破片の最大速度
    float fragmentLifeTime = 0.55f; // 放射破片の表示時間
};

/// <summary>
/// 時空破砕エフェクトを管理するクラス
/// </summary>
class TemporalRiftEffect {
public:
    using ScreenUvResolver = std::function<Math::Vector2(const Math::Vector3&)>;

    /// <summary>
    /// エフェクトの進行状態を初期状態へ戻す
    /// </summary>
    void ResetState(MyEngine::Camera* camera);

    /// <summary>
    /// 調整値を初期値へ戻す
    /// </summary>
    void ResetSettings();

    /// <summary>
    /// 時空破砕エフェクトを開始する
    /// </summary>
    void Start(MyEngine::PostProcess& postProcess, std::vector<std::unique_ptr<MyEngine::Object3d>>& objects3d, const Math::Vector2& screenUv);

    /// <summary>
    /// 時空破砕エフェクトの状態を更新する
    /// </summary>
    void Update(float deltaTime, MyEngine::PostProcess& postProcess, MyEngine::Camera* camera);

    /// <summary>
    /// 時間ずれ対象のTransform履歴を更新する
    /// </summary>
    void UpdateAfterimages(std::vector<std::unique_ptr<MyEngine::Object3d>>& objects3d);

    /// <summary>
    /// Transform履歴から残像スプライトを更新する
    /// </summary>
    void UpdateAfterimageSprites(std::vector<std::unique_ptr<MyEngine::Sprite>>& afterimageSprites, const ScreenUvResolver& screenUvResolver);

    /// <summary>
    /// Transform履歴による残像を描画する
    /// </summary>
    void DrawAfterimages(MyEngine::SpriteCommon* spriteCommon, std::vector<std::unique_ptr<MyEngine::Sprite>>& afterimageSprites);

    /// <summary>
    /// ヒットストップとカメラシェイクを更新する
    /// </summary>
    void UpdateImpact(float deltaTime, MyEngine::Camera* camera);

    /// <summary>
    /// カメラシェイクを終了してカメラ位置を復元する
    /// </summary>
    void StopCameraShake(MyEngine::Camera* camera);

    /// <summary>
    /// ImGuiで時空破砕エフェクトの状態と調整値を表示する
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 時空破砕エフェクトが再生中か確認する
    /// </summary>
    bool IsPlaying() const;

    /// <summary>
    /// 歪みとブラーを連結するフェーズか確認する
    /// </summary>
    bool IsPostProcessChainPhase() const;

    /// <summary>
    /// ヒットストップの残り時間を取得する
    /// </summary>
    float GetHitStopRemainingTime() const;

    /// <summary>
    /// エフェクト発生位置を取得する
    /// </summary>
    const Math::Vector3& GetEffectPosition() const;

    /// <summary>
    /// 画面UV座標を取得する
    /// </summary>
    const Math::Vector2& GetScreenUv() const;

    /// <summary>
    /// 画面UV座標を設定する
    /// </summary>
    void SetScreenUv(const Math::Vector2& screenUv);

private:
    /// <summary>
    /// 現在のフェーズ名を取得する
    /// </summary>
    const char* GetPhaseName() const;

    /// <summary>
    /// 破砕時のヒットストップとカメラシェイクを開始する
    /// </summary>
    void StartImpact(MyEngine::Camera* camera);

    /// <summary>
    /// 空間亀裂を進行度に合わせて段階的に発生する
    /// </summary>
    void EmitSpaceCracks();

    /// <summary>
    /// 空間破砕時の多重リングと放射破片を発生する
    /// </summary>
    void EmitRiftBurst();

    TemporalRiftPhase phase_ = TemporalRiftPhase::Idle; // 現在の演出状態
    float phaseTime_ = 0.0f; // 現在フェーズでの経過時間
    TemporalRiftSettings settings_; // 時空破砕の調整値
    MyEngine::PostEffectType previousPostEffect_ = MyEngine::PostEffectType::Copy; // 再生前のポストエフェクト
    Math::Vector3 effectPosition_ { 0.0f, 1.0f, 0.0f }; // 時空破砕の発生位置
    int cracksEmitted_ = 0; // 現在までに発生した亀裂数
    Math::Vector2 screenUv_ { 0.5f, 0.5f }; // 時空破砕の画面UV座標
    std::deque<Math::Transform> transformHistory_; // 時間ずれ対象のTransform履歴
    Math::Transform targetBaseTransform_ {}; // 演出開始前の対象Transform
    bool hasTargetBaseTransform_ = false; // 復元用Transformを保持しているか
    float hitStopRemainingTime_ = 0.0f; // ヒットストップの残り時間
    float cameraShakeRemainingTime_ = 0.0f; // カメラシェイクの残り時間
    float cameraShakeElapsedTime_ = 0.0f; // カメラシェイク開始からの経過時間
    Math::Vector3 cameraShakeBasePosition_ {}; // シェイク開始前のカメラ位置
    bool isCameraShakeActive_ = false; // カメラシェイク中か
};