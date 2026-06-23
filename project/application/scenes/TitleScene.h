#pragma once
#include "../../engine/base/IScene.h"
#include "../../engine/base/PostProcess.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <vector>

// 前方宣言
namespace MyEngine {
class Sprite;
class Object3d;
class SpriteCommon;
class Object3dCommon;
class ParticleManager;
class TextureManager;
class Camera;
class SkyBox;
}

#include "../../engine/particle/ParticleEmitter.h"

using namespace MyEngine;

/// <summary>
/// プレイシーンのクラス。IScene インターフェースを実装して、ゲームのプレイ中のシーンを表す。
/// </summary>
class TitleScene : public IScene {
public: // メンバ関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    TitleScene();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~TitleScene();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(const SceneContext& ctx) override;

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update(float dt) override;

    /// <summary>
    /// 描画処理
    /// </summary>
    void Draw() override;

    /// <summary>
    /// 　シーンに入るときの処理
    /// </summary>
    void OnEnter() override;

    /// <summary>
    /// シーンから出る時の処理
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// 描画モードの更新を受け取る
    /// </summary>
    void SetSelectedDrawType(int t) override;

    /// <summary>
    /// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
    /// </summary>
    void FillObject3dPointers(std::vector<Object3d*>* out) override;

    /// <summary>
    /// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
    /// </summary>
    void FillSpritePointers(std::vector<Sprite*>* out) override;

    /// <summary>
    /// シーンの名前を取得
    /// </summary>
    std::string GetName() const override { return "Title"; }

    /// <summary>
    /// プレイシーンが使用しているポストプロセスを取得する
    /// </summary>
    PostProcess* GetPostProcess() override { return &postProcess_; }

    /// <summary>
    /// 時空破砕エフェクトの調整UIを描画する
    /// </summary>
    void DrawImGui() override;

private:
    /// <summary>
    /// ImGuiから選択できるエフェクト種別
    /// </summary>
    enum class EffectType {
        DimensionalShatter,
        TimeReversal,
        TimeStop,
    };

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
    /// 時間逆流エフェクトの進行状態
    /// </summary>
    enum class TimeReversalPhase {
        Idle,
        Expanding,
        Paused,
        Rewinding,
        Converging,
    };

    /// <summary>
    /// 時間逆流専用パーティクル
    /// </summary>

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
        float distortionStrength = 0.045f; // 開始と終了に使用する歪み強度
        float distortionRadius = 0.65f; // 歪みの影響半径
        float distortionWaveCount = 5.0f; // 歪みの波数
        int startRingCount = 3; // 停止開始時に生成するリング数
        int releaseRingCount = 4; // 時間再開時に生成するリング数
        int releaseFragmentCount = 16; // 時間再開時に生成する破片数
        Math::Vector3 effectPosition { 0.0f, 1.0f, 0.0f }; // エフェクトの発生位置
    };
    struct TimeReversalParticle {
        Math::Vector3 position; // 現在のワールド座標
        Math::Vector3 velocity; // 拡散時の移動速度
        Math::Vector3 rewindStartPosition; // 巻き戻し開始時のワールド座標
        float elapsedTime = 0.0f; // 生成からの経過時間
        float lifeTime = 1.0f; // 拡散を続ける時間
    };

    /// <summary>
    /// 時間逆流エフェクトの調整値
    /// </summary>
    struct TimeReversalSettings {
        int particleCount = 32; // 生成する粒子数
        float minSpeed = 1.5f; // 最小拡散速度
        float maxSpeed = 4.0f; // 最大拡散速度
        float expansionDuration = 0.8f; // 通常拡散を行う時間
        float pauseDuration = 0.35f; // 拡散後に空中停止する時間
        float rewindDuration = 0.65f; // 発生地点へ巻き戻す時間
        int rewindAfterimageCount = 3; // 巻き戻し中に表示する残像数
        float rewindAfterimageSpacing = 0.18f; // 残像同士の軌道間隔
        float rewindAfterimageAlpha = 0.35f; // 最も濃い残像の透明度
        float distortionStrength = -0.035f; // 巻き戻し中の画面歪み強度
        float distortionRadius = 0.45f; // 時間逆流の画面歪み半径
        float distortionWaveCount = 4.0f; // 時間逆流の画面歪み波数
        float convergenceDuration = 0.22f; // 収束演出を表示する時間
        float convergenceFlashSize = 180.0f; // 収束時のフラッシュ最大サイズ
        float convergenceFlashAlpha = 0.9f; // 収束時のフラッシュ透明度
        int convergenceRingCount = 2; // 収束時に生成するリング数
        float transformHistoryDuration = 2.0f; // 3D対象のTransform履歴保持時間
        float particleSize = 24.0f; // 粒子スプライトの大きさ
        Math::Vector4 particleColor { 0.45f, 0.85f, 1.0f, 0.9f }; // 粒子の色
        Math::Vector3 effectPosition { 0.0f, 1.0f, 0.0f }; // エフェクトの発生位置
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
        float burstBlurStrength = 0.050f; // 破砕開始時のブラー幅
        int blurSampleCount = 16; // ラジアルブラーのサンプル数
        float distortionRadius = 0.35f; // 画面歪みの影響半径
        float compressDistortionStrength = -0.025f; // 圧縮時の内向き歪み強度
        float burstDistortionStrength = 0.045f; // 破砕時の外向き歪み強度
        float distortionWaveCount = 3.0f; // 画面歪みの波数
        int afterimageCount = 5; // 表示する残像数
        int afterimageFrameInterval = 3; // 残像同士の履歴間隔
        float afterimageSize = 90.0f; // 残像スプライトの大きさ
        float afterimageAlpha = 0.35f; // 最も新しい残像の透明度
        float temporalDisplacement = 0.8f; // 対象へ加える時間ずれ移動量
        Math::Vector3 afterimageColor { 0.35f, 0.65f, 1.0f }; // 残像の色
        float hitStopDuration = 0.06f; // 破砕時に演出を停止する時間
        float cameraShakeDuration = 0.22f; // カメラを揺らす時間
        float cameraShakeStrength = 0.12f; // カメラシェイクの最大移動量
        float cameraShakeFrequency = 35.0f; // カメラシェイクの振動周波数
        int crackCount = 7; // 生成する亀裂数
        float crackLength = 1.20f; // 亀裂の基準長
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
        float fragmentMinSpeed = 2.5f; // 放射破片の最低速度
        float fragmentMaxSpeed = 6.0f; // 放射破片の最高速度
        float fragmentLifeTime = 0.55f; // 放射破片の表示時間

    };

    /// <summary>
    /// 時空破砕エフェクトを開始する
    /// </summary>
    void StartTemporalRiftEffect();

    /// <summary>
    /// 時間逆流エフェクトを開始する
    /// </summary>
    void StartTimeReversalEffect();

    /// <summary>
    /// 時間停止エフェクトを開始する
    /// </summary>
    void StartTimeStopEffect();

    /// <summary>
    /// 時間停止エフェクトの状態を更新する
    /// </summary>
    void UpdateTimeStopEffect(float deltaTime);

    /// <summary>
    /// 時間停止中か確認する
    /// </summary>
    bool IsTimeStopped() const;

    /// <summary>
    /// 時間逆流専用パーティクルを更新する
    /// </summary>
    void UpdateTimeReversalEffect(float deltaTime);

    /// <summary>
    /// 時間逆流専用パーティクルのスプライトを更新する
    /// </summary>
    void UpdateTimeReversalSprites();

    /// <summary>
    /// 時間巻き戻し対象のTransform履歴を更新する
    /// </summary>
    void UpdateTimeReversalTransformHistory();

    /// <summary>
    /// 保存したTransform履歴を対象オブジェクトへ逆順に適用する
    /// </summary>
    void ApplyTimeReversalTransform();

    /// <summary>
    /// 時間巻き戻し完了時の収束演出を開始する
    /// </summary>
    void StartTimeReversalConvergence();

    /// <summary>
    /// 時間逆流専用パーティクルを描画する
    /// </summary>
    void DrawTimeReversalParticles();

    /// <summary>
    /// ImGuiで選択中のエフェクトを開始する
    /// </summary>
    void StartSelectedEffect();

    /// <summary>
    /// 選択中のエフェクトが実装済みか確認する
    /// </summary>
    bool IsSelectedEffectReady() const;

    /// <summary>
    /// いずれかのエフェクトが再生中か確認する
    /// </summary>
    bool IsAnyEffectPlaying() const;

    /// <summary>
    /// 時空破砕エフェクトの進行状態を更新する
    /// </summary>
    void UpdateTemporalRiftEffect(float deltaTime);

    /// <summary>
    /// 時空破砕のワールド座標を画面UV座標へ変換する
    /// </summary>
    Math::Vector2 CalculateTemporalRiftScreenUv() const;

    /// <summary>
    /// 指定したワールド座標を画面UV座標へ変換する
    /// </summary>
    Math::Vector2 CalculateWorldScreenUv(const Math::Vector3& worldPosition) const;

    /// <summary>
    /// 時間ずれ対象のTransformと履歴を更新する
    /// </summary>
    void UpdateTemporalAfterimages();

    /// <summary>
    /// Transform履歴から残像スプライトを更新する
    /// </summary>
    void UpdateAfterimageSprites();

    /// <summary>
    /// Transform履歴による残像を描画する
    /// </summary>
    void DrawTemporalAfterimages();

    /// <summary>
    /// 破砕時のヒットストップとカメラシェイクを開始する
    /// </summary>
    void StartImpactResponse();

    /// <summary>
    /// ヒットストップとカメラシェイクを更新する
    /// </summary>
    void UpdateImpactResponse(float deltaTime);

    /// <summary>
    /// カメラシェイクを終了してカメラ位置を復元する
    /// </summary>
    void StopCameraShake();

    /// <summary>
    /// 空間亀裂を複数生成する
    /// </summary>
    void EmitSpaceCracks();

    /// <summary>
    /// 空間破砕時の衝撃波と破片を生成する
    /// </summary>
    void EmitRiftBurst();

    /// <summary>
    /// 3Dオブジェクトとパーティクルを描画する
    /// </summary>
    void DrawWorldAndParticles();

    /// <summary>
    /// ポストプロセスの影響を受けないスプライトを描画する
    /// </summary>
    void DrawSprites();

private: // メンバ変数
    SceneContext ctx_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::vector<std::unique_ptr<Object3d>> objects3d_;
    std::unique_ptr<Object3d> particlePlane_;
    std::unique_ptr<Object3d> particleRing_;
    std::unique_ptr<Object3d> particleCylinder_;
    ParticleEmitter pmEmitter_;
    ParticleEmitter ringEmitter_;
    ParticleEmitter cylinderEmitter_;
    std::unique_ptr<SkyBox> skybox_;
    std::vector<std::unique_ptr<Sprite>> temporalAfterimageSprites_; // Transform履歴を表示する残像
    std::vector<std::unique_ptr<Sprite>> timeReversalSprites_; // 時間逆流専用パーティクルの表示スプライト
    std::vector<std::unique_ptr<Sprite>> timeReversalAfterimageSprites_; // 巻き戻し軌道を表示する残像スプライト
    std::unique_ptr<Sprite> timeReversalConvergenceSprite_; // 収束時のフラッシュ表示スプライト
    PostProcess postProcess_; // 時空演出に使用するポストプロセス
    int sceneRenderTargetHandle_ = -1; // シーン描画用レンダーターゲット
    uint32_t sceneRenderTargetSrvIndex_ = UINT32_MAX; // シーン描画結果のSRV
    int postProcessIntermediateHandle_ = -1; // ポストエフェクト連結用の中間レンダーターゲット
    uint32_t postProcessIntermediateSrvIndex_ = UINT32_MAX; // 中間描画結果のSRV
    TemporalRiftPhase temporalRiftPhase_ = TemporalRiftPhase::Idle; // 現在の演出状態
    EffectType selectedEffectType_ = EffectType::DimensionalShatter; // ImGuiで選択中のエフェクト
    TimeReversalPhase timeReversalPhase_ = TimeReversalPhase::Idle; // 時間逆流の現在状態
    float timeReversalPhaseTime_ = 0.0f; // 時間逆流の現在状態での経過時間
    TimeReversalSettings timeReversalSettings_; // 時間逆流の調整値
    TimeStopPhase timeStopPhase_ = TimeStopPhase::Idle; // 現在の時間停止状態
    float timeStopPhaseTime_ = 0.0f; // 現在フェーズでの経過時間
    TimeStopSettings timeStopSettings_; // 時間停止の調整値
    PostEffectType timeStopPreviousPostEffect_ = PostEffectType::Copy; // 再生前のポストエフェクト 
   std::vector<TimeReversalParticle> timeReversalParticles_; // 時間逆流専用パーティクル
    std::deque<Math::Transform> timeReversalTransformHistory_; // 時間巻き戻し対象のTransform履歴
    std::mt19937 timeReversalRandom_ { std::random_device {}() }; // 粒子生成に使用する乱数
    float temporalRiftPhaseTime_ = 0.0f; // 現在の演出状態での経過時間
    Math::Vector3 temporalRiftPosition_ { 0.0f, 1.0f, 0.0f }; // 時空破砕の発生位置
    TemporalRiftSettings temporalRiftSettings_; // 時空破砕の調整値
    int temporalCracksEmitted_ = 0; // 現在までに生成した亀裂数
    Math::Vector2 temporalRiftScreenUv_ { 0.5f, 0.5f }; // 時空破砕の画面UV座標
    std::deque<Math::Transform> temporalTransformHistory_; // 時間ずれ対象のTransform履歴
    Math::Transform temporalTargetBaseTransform_ {}; // 演出開始前の対象Transform
    bool hasTemporalTargetBaseTransform_ = false; // 復元用Transformを保持しているか
    float hitStopRemainingTime_ = 0.0f; // ヒットストップの残り時間
    float cameraShakeRemainingTime_ = 0.0f; // カメラシェイクの残り時間
    float cameraShakeElapsedTime_ = 0.0f; // カメラシェイク開始からの経過時間
    Math::Vector3 cameraShakeBasePosition_ {}; // シェイク開始前のカメラ位置
    bool isCameraShakeActive_ = false; // カメラシェイク中か
};
