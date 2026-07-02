#pragma once
#include "../../engine/base/IScene.h"
#include "../../engine/base/PostProcess.h"
#include "../effects/TemporalRiftEffect.h"
#include "../effects/TimeReversalEffect.h"
#include "../effects/TimeStopEffect.h"

#include <cstdint>
#include <memory>
#include <vector>

// 前方宣言
namespace MyEngine {
class Sprite;
class Object3d;
class SpriteCommon;
class Object3dCommon;
class Camera;
class SkyBox;
}

#include "../../engine/particle/ParticleEmitter.h"

/// <summary>
/// ゲームプレイ中のシーンを管理するクラス
/// </summary>
class PlayScene : public MyEngine::IScene {
public: // メンバ関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    PlayScene();

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PlayScene();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(const MyEngine::SceneContext& ctx) override;

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
    /// ImGuiを描画する
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// シーン開始時の処理
    /// </summary>
    void OnEnter() override;

    /// <summary>
    /// シーン終了時の処理
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// 描画対象の種類を設定する
    /// </summary>
    void SetSelectedDrawType(int t) override;

    /// <summary>
    /// Scene View用のオフスクリーン描画だけにするか設定する
    /// </summary>
    void SetSceneViewOnly(bool enabled) override;

    /// <summary>
    /// Scene Viewへ表示するSRV番号を取得する
    /// </summary>
    uint32_t GetSceneViewSrvIndex() const override;

    /// <summary>
    /// シーンが使用しているポストプロセスを取得する
    /// </summary>
    MyEngine::PostProcess* GetPostProcess() override;

    /// <summary>
    /// 3Dオブジェクトのポインタ一覧を取得する
    /// </summary>
    void FillObject3dPointers(std::vector<MyEngine::Object3d*>* out);

    /// <summary>
    /// スプライトのポインタ一覧を取得する
    /// </summary>
    void FillSpritePointers(std::vector<MyEngine::Sprite*>* out);

private:
    /// <summary>
    /// エディターから選択できるエフェクト種別
    /// </summary>
    enum class EffectType {
        DimensionalShatter,
        TimeReversal,
        TimeStop,
    };

    /// <summary>
    /// 時空破砕エフェクトを開始する
    /// </summary>
    void StartTemporalRiftEffect();

    /// <summary>
    /// 時間逆行エフェクトを開始する
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
    /// 時間逆行エフェクトの状態を更新する
    /// </summary>
    void UpdateTimeReversalEffect(float deltaTime);

    /// <summary>
    /// 時間逆行用スプライトを更新する
    /// </summary>
    void UpdateTimeReversalSprites();

    /// <summary>
    /// 時間逆行対象のTransform履歴を更新する
    /// </summary>
    void UpdateTimeReversalTransformHistory();

    /// <summary>
    /// 時間逆行用スプライトを描画する
    /// </summary>
    void DrawTimeReversalParticles();

    /// <summary>
    /// ImGuiで選択中のエフェクトを開始する
    /// </summary>
    void StartSelectedEffect();

    /// <summary>
    /// いずれかのエフェクトが再生中か確認する
    /// </summary>
    bool IsAnyEffectPlaying() const;

    /// <summary>
    /// 時空破砕エフェクトの状態を更新する
    /// </summary>
    void UpdateTemporalRiftEffect(float deltaTime);

    /// <summary>
    /// 時空破砕の発生位置を画面UV座標へ変換する
    /// </summary>
    Math::Vector2 CalculateTemporalRiftScreenUv() const;

    /// <summary>
    /// 指定したワールド座標を画面UV座標へ変換する
    /// </summary>
    Math::Vector2 CalculateWorldScreenUv(const Math::Vector3& worldPosition) const;

    /// <summary>
    /// 時間ずれ対象のTransform履歴を更新する
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
    /// ヒットストップとカメラシェイクを更新する
    /// </summary>
    void UpdateImpactResponse(float deltaTime);

    /// <summary>
    /// カメラシェイクを終了してカメラ位置を復元する
    /// </summary>
    void StopCameraShake();

    /// <summary>
    /// パーティクル描画用オブジェクトを初期化する
    /// </summary>
    void InitializeParticleObjects();

    /// <summary>
    /// パーティクル管理とエミッターを初期化する
    /// </summary>
    void InitializeParticleEffects();

    /// <summary>
    /// 時間演出用スプライトを初期化する
    /// </summary>
    void InitializeTemporalEffectSprites();

    /// <summary>
    /// ポストプロセス用レンダーターゲットを初期化する
    /// </summary>
    void InitializePostProcessTargets();

    /// <summary>
    /// 3D空間とパーティクルを描画する
    /// </summary>
    void DrawWorldAndParticles();

    /// <summary>
    /// ポストプロセスの影響を受けないスプライトを描画する
    /// </summary>
    void DrawSprites();

private: // メンバ変数
    MyEngine::SceneContext ctx_;
    std::vector<std::unique_ptr<MyEngine::Sprite>> sprites_;
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects3d_;
    std::unique_ptr<MyEngine::Object3d> particlePlane_;
    std::unique_ptr<MyEngine::Object3d> particleRing_;
    std::unique_ptr<MyEngine::Object3d> particleCylinder_;
    ParticleEmitter pmEmitter_;
    ParticleEmitter ringEmitter_;
    ParticleEmitter cylinderEmitter_;
    std::unique_ptr<MyEngine::SkyBox> skybox_;
    std::vector<std::unique_ptr<MyEngine::Sprite>> temporalAfterimageSprites_; // Transform履歴を表示する残像
    std::vector<std::unique_ptr<MyEngine::Sprite>> timeReversalSprites_; // 時間逆行用パーティクルの表示スプライト
    std::vector<std::unique_ptr<MyEngine::Sprite>> timeReversalAfterimageSprites_; // 巻き戻し軌跡を表示する残像スプライト
    std::unique_ptr<MyEngine::Sprite> timeReversalConvergenceSprite_; // 収束時のフラッシュ表示スプライト
    MyEngine::PostProcess postProcess_; // 時間演出に使用するポストプロセス
    int sceneRenderTargetHandle_ = -1; // シーン描画用レンダーターゲット
    uint32_t sceneRenderTargetSrvIndex_ = UINT32_MAX; // シーン描画結果のSRV
    uint32_t sceneDepthSrvIndex_ = UINT32_MAX; // シーン描画深度のSRV
    bool sceneViewOnly_ = false; // シーンをScene View用RTだけに描画するか
    int postProcessIntermediateHandle_ = -1; // ポストエフェクト連結用の中間レンダーターゲット
    uint32_t postProcessIntermediateSrvIndex_ = UINT32_MAX; // 中間描画結果のSRV
    int finalRenderTargetHandle_ = -1; // Scene Viewへ表示する最終描画用レンダーターゲット
    uint32_t finalRenderTargetSrvIndex_ = UINT32_MAX; // Scene Viewへ表示する最終描画結果のSRV
    uint32_t dissolveMaskSrvIndex_ = UINT32_MAX; // Dissolveで使用するノイズマスクSRV
    TemporalRiftEffect temporalRiftEffect_; // 時空破砕エフェクト
    EffectType selectedEffectType_ = EffectType::DimensionalShatter; // ImGuiで選択中のエフェクト
    TimeReversalEffect timeReversalEffect_; // 時間逆行エフェクト
    TimeStopEffect timeStopEffect_; // 時間停止エフェクト
};