#pragma once
#include "../../engine/base/IScene.h"
#include "../../engine/base/PostProcess.h"
#include "../../engine/base/RenderTarget.h"
#include "../../engine/level/LevelData.h"
#include "../../engine/utility/CollisionSystem.h"
#include "../effects/TemporalRiftEffect.h"
#include "../effects/TimeReversalEffect.h"
#include "../effects/TimeStopEffect.h"
#include "../gimmicks/StageGimmicks.h"
#include "../player/PastSelfCloneManager.h"
#include "../player/PastSelfRecorder.h"
#include "../player/Player.h"
#include "StageRuleSettings.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
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
    /// シーン名を取得する
    /// </summary>
    std::string GetName() const override { return "Play"; }
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
    /// Scene ViewのGizmoで3DオブジェクトのTransformが編集されたときに呼び出す。
    /// </summary>
    void NotifyObjectTransformEdited(size_t objectIndex) override;

    /// <summary>
    /// Scene View画像上へLevel Editor用の編集表示を重ねて描画する。
    /// </summary>
    void DrawSceneViewOverlay(const Math::Matrix4x4& viewProjectionMatrix, float imageMinX, float imageMinY, float imageWidth, float imageHeight) override;

    /// <summary>
    /// Level EditorとGizmoで共有する選択中3Dオブジェクト番号を取得する。
    /// </summary>
    int GetSelectedSceneObjectIndex() const;

    /// <summary>
    /// Level EditorからGizmo対象の3Dオブジェクトを選択する。
    /// </summary>
    void SelectSceneObjectForEditor(size_t objectIndex);
    /// <summary>
    /// スプライトのポインタ一覧を取得する
    /// </summary>
    void FillSpritePointers(std::vector<MyEngine::Sprite*>* out);

    /// <summary>
    /// パーティクルエミッターのポインタ一覧を取得する
    /// </summary>
    void FillParticleEmitterPointers(std::vector<::ParticleEmitter*>* out) override;

private:
    /// <summary>
    /// 分身チュートリアル用のステージブロック。
    /// </summary>
    struct PastSelfTutorialStageBlock {
        std::string name; // ステージエディターで識別する名前
        std::unique_ptr<MyEngine::Object3d> object; // 表示用のステージブロック
        SolidCollider collider; // 地形として全面衝突する情報
        Math::Vector4 baseColor { 1.0f, 1.0f, 1.0f, 1.0f }; // 通常時の表示色
        uint32_t routeMask = 0; // このブロックを使用するチュートリアルルート
        bool oneCloneGoalPlatform = false; // 1体ルートの到達床として色を切り替えるか
        bool goalMarker = false; // ゴール表示用のブロックか
    };

    /// <summary>
    /// ステージ編集対象の種類。
    /// </summary>
    enum class PastSelfTutorialEditorObjectType {
        StageBlock, // 固定ステージブロック
        BoxSwitch, // 通常スイッチ
        Door, // 通常扉
        TimedSwitch, // 時間差スイッチ
        TimedDoor, // 時間差扉
        ToggleSwitch, // トグルスイッチ
        ToggleGate, // トグル連動ゲート
        ToggleElevator, // トグル連動昇降足場
        WeightSwitch, // 重さスイッチ
        GoalBridge, // ゴール前の橋
        OneWayGate, // 一方通行ゲート
    };

    /// <summary>
    /// 実ステージ上の編集対象を参照する情報。
    /// </summary>
    struct PastSelfTutorialEditorObject {
        MyEngine::Object3d* object = nullptr; // ギズモとImGuiで編集する表示オブジェクト
        const char* label = "Stage Object"; // ステージ編集一覧へ表示する名前
        PastSelfTutorialEditorObjectType type = PastSelfTutorialEditorObjectType::StageBlock; // 編集対象の種類
        size_t stageBlockIndex = 0; // StageBlockの場合に参照するブロック番号
    };

    /// <summary>
    /// JSONから読み込んだギミック配置情報。
    /// </summary>
    struct PastSelfTutorialGimmickLayout {
        std::string id; // 保存対象のギミックを識別するID
        Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 保存された表示スケール
        Math::Vector3 rotate { 0.0f, 0.0f, 0.0f }; // 保存された表示回転
        Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // 保存された表示座標
        Math::Vector3 upperTranslate { 0.0f, 0.0f, 0.0f }; // 昇降足場の上端座標
        bool hasUpperTranslate = false; // 上端座標を持つ昇降足場情報か
    };

    /// <summary>
    /// 分身チュートリアルで使用する攻略ルート種別。
    /// </summary>
    enum class PastSelfTutorialRoute {
        OneCloneBasics, // 分身1体でトグル昇降床を攻略するルート
        TwoCloneCooperation, // 分身2体で緑と青のスイッチを同時起動するルート
        FinalChallenge, // すべてのギミックを確認する総合ルート
    };

    /// <summary>
    /// ポストプロセス描画で使用する状態
    /// </summary>
    struct PostProcessDrawContext {
        uint32_t sourceSrvIndex = UINT32_MAX; // 最終描画で入力として使うSRV番号
        MyEngine::PostEffectType finalEffectType = MyEngine::PostEffectType::Copy; // 最終的に適用するポストエフェクト
        bool useGaussianFilter = false; // Gaussian Filterの2pass描画を行うか
        bool useFinalRenderTarget = false; // Scene View用RTへ最終結果を描画するか
    };
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
    /// エフェクト選択と再生操作用のImGuiを描画する。
    /// </summary>
    void DrawEffectControllerImGui();

    /// <summary>
    /// ImGuiでシーン内3Dオブジェクトの生成と削除を行う
    /// </summary>
    void DrawSceneObjectEditImGui();

    /// <summary>
    /// 実ステージで使用中のオブジェクト編集ImGuiを描画する。
    /// </summary>
    void DrawPastSelfTutorialStageEditorImGui();

    /// <summary>
    /// ImGuiでレベルJSONの読み込み状態を表示する。
    /// </summary>
    void DrawLevelDataImGui();

    /// <summary>
    /// LevelData編集ImGuiの一時状態を保持する。
    /// </summary>
    struct LevelEditorImGuiState;

    /// <summary>
    /// LevelData編集ImGuiからレベル再読み込みを実行する。
    /// </summary>
    bool ExecuteLevelEditorReload(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiからレベル保存を実行する。
    /// </summary>
    void ExecuteLevelEditorSave();

    /// <summary>
    /// LevelData編集ImGuiから保存後再読み込みを実行する。
    /// </summary>
    void ExecuteLevelEditorSaveAndReload(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiの読み込み操作を描画する。
    /// </summary>
    void DrawLevelLoadSectionImGui(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiの保存操作を描画する。
    /// </summary>
    void DrawLevelSaveSectionImGui(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiのPrefab操作を描画する。
    /// </summary>
    void DrawLevelPrefabSectionImGui(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiのシーン反映操作を描画する。
    /// </summary>
    void DrawLevelSceneApplySectionImGui(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiの状態表示を描画する。
    /// </summary>
    void DrawLevelStatusSectionImGui();

    /// <summary>
    /// LevelData編集ImGuiの確認ポップアップを描画する。
    /// </summary>
    void DrawLevelConfirmPopupsImGui(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiの階層オブジェクト編集を描画する。
    /// </summary>
    void DrawLevelObjectsSectionImGui(LevelEditorImGuiState& state);

    /// <summary>
    /// LevelData編集ImGuiの遅延履歴と自動保存を処理する。
    /// </summary>
    void FlushLevelEditorDeferredActions(LevelEditorImGuiState& state);

    /// <summary>
    /// ImGuiで衝突判定の状態を表示する。
    /// </summary>
    void DrawCollisionDebugImGui();

    /// <summary>
    /// ImGuiでシーン内スプライトの生成と削除を行う。
    /// </summary>
    void DrawSceneSpriteEditImGui();

    /// <summary>
    /// 選択中エフェクトの詳細ImGuiを描画する。
    /// </summary>
    void DrawSelectedEffectImGui();

    /// <summary>
    /// ImGuiで選択中のエフェクトを開始する
    /// </summary>
    void StartSelectedEffect();

    /// <summary>
    /// いずれかのエフェクトが再生中か確認する
    /// </summary>
    bool IsAnyEffectPlaying() const;

    /// <summary>
    /// エフェクト開始入力を処理する。
    /// </summary>
    void HandleEffectStartInput();

    /// <summary>
    /// ポストエフェクト切り替え入力を処理する。
    /// </summary>
    void HandlePostProcessShortcutInput();


    /// <summary>
    /// 分身チュートリアル用オブジェクトを初期化する。
    /// </summary>
    void InitializePastSelfTutorial();

    /// <summary>
    /// 分身チュートリアル用ステージを初期化する。
    /// </summary>
    void InitializePastSelfTutorialStage();

    /// <summary>
    /// 選択中ルートで使用するステージ編集対象一覧を構築する。
    /// </summary>
    void BuildPastSelfTutorialEditorObjects(std::vector<PastSelfTutorialEditorObject>* outObjects);

    /// <summary>
    /// ステージ編集対象のTransform変更をゲーム判定へ反映する。
    /// </summary>
    void ApplyPastSelfTutorialEditorTransform(const PastSelfTutorialEditorObject& editorObject);

    /// <summary>
    /// 読み込んだギミック配置を実ステージへ反映する。
    /// </summary>
    void ApplyPastSelfTutorialGimmickLayouts();

    /// <summary>
    /// 現在の攻略状況を収めるゲーム用カメラ位置へ戻す。
    /// </summary>
    void ResetPastSelfTutorialCameraFrame();

    /// <summary>
    /// 実ステージブロックとギミック配置をJSONから再読み込みする。
    /// </summary>
    bool ReloadPastSelfTutorialStage();

    /// <summary>
    /// 現在の実ステージブロックとギミック配置をJSONへ保存する。
    /// </summary>
    bool SavePastSelfTutorialStage();

    /// <summary>
    /// 選択中ルート用の新しいステージブロックを追加する。
    /// </summary>
    size_t CreatePastSelfTutorialStageBlock();

    /// <summary>
    /// 指定したステージブロックを複製する。
    /// </summary>
    size_t DuplicatePastSelfTutorialStageBlock(size_t blockIndex);

    /// <summary>
    /// 指定したステージブロックを削除する。
    /// </summary>
    bool DeletePastSelfTutorialStageBlock(size_t blockIndex);

    /// <summary>
    /// 指定情報から実ステージブロックを構築して末尾へ追加する。
    /// </summary>
    size_t AppendPastSelfTutorialStageBlock(const std::string& name, const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& translate, const Math::Vector4& color, bool collidable, bool oneCloneGoalPlatform, bool goalMarker, uint32_t routeMask);

    /// <summary>
    /// 分身チュートリアル用状態を更新する。
    /// </summary>
    void UpdatePastSelfTutorial(float deltaTime);

    /// <summary>
    /// 分身チュートリアル用の分身ギミックを初期化する。
    /// </summary>
    void InitializePastSelfTutorialMechanics();

    /// <summary>
    /// 分身チュートリアル用の分身ギミックを更新する。
    /// </summary>
    void UpdatePastSelfTutorialMechanics(float deltaTime);

    /// <summary>
    /// 分身チュートリアル用状態を初期状態へ戻す。
    /// </summary>
    void ResetPastSelfTutorialState();

    /// <summary>
    /// 記録済み分身を残したまま再生開始用の状態へ戻す。
    /// </summary>
    void ResetPastSelfTutorialReplayState(bool registerPrepareAction = true);

    /// <summary>
    /// 最後に保存した分身を削除して再生準備状態へ戻す。
    /// </summary>
    void UndoLastPastSelfTutorialClone();

    /// <summary>
    /// 新しい分身用のプレイヤー記録を開始する。
    /// </summary>
    void StartPastSelfTutorialRecording();

    /// <summary>
    /// 現在のステージルールで新しい分身記録を開始できるか判定する。
    /// </summary>
    bool CanStartPastSelfTutorialRecording() const;

    /// <summary>
    /// 現在の記録を停止し、新しい分身として保存する。
    /// </summary>
    void FinishPastSelfTutorialRecording();

    /// <summary>
    /// 分身チュートリアル用の分身ギミック表示を更新する。
    /// </summary>
    void UpdatePastSelfTutorialMechanicObjects(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 分身記録の開始・終了地点マーカーを更新する。
    /// </summary>
    void UpdatePastSelfTutorialCloneRecordMarkers(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 分身記録の開始・終了地点マーカーを描画する。
    /// </summary>
    void DrawPastSelfTutorialCloneRecordMarkers();

    /// <summary>
    /// 分身チュートリアル用の分身ギミックを描画する。
    /// </summary>
    void DrawPastSelfTutorialMechanics();

    /// <summary>
    /// 分身チュートリアル用ステージを更新する。
    /// </summary>
    void UpdatePastSelfTutorialStage(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 選択中のチュートリアルルートで指定されたステージブロックを使用するか判定する。
    /// </summary>
    bool IsPastSelfTutorialStageBlockEnabled(size_t blockIndex) const;

    /// <summary>
    /// 選択中のチュートリアルルートに応じたカメラ注視対象を取得する。
    /// </summary>
    Math::Vector3 GetPastSelfTutorialRouteCameraTarget(const PlayerState& playerState) const;

    /// <summary>
    /// 分身チュートリアル用ステージを描画する。
    /// </summary>
    void DrawPastSelfTutorialStage();

    /// <summary>
    /// 分身チュートリアル用の全面コライダーを追加する。
    /// </summary>
    void AppendPastSelfTutorialSolidColliders(std::vector<SolidCollider>* colliders) const;

    /// <summary>
    /// 分身チュートリアル用ゴール判定を更新する。
    /// </summary>
    void UpdatePastSelfTutorialGoal();

    /// <summary>
    /// 分身チュートリアル用ゴール表示を現在状態に合わせる。
    /// </summary>
    void ApplyPastSelfTutorialGoalVisual();

    /// <summary>
    /// 分身チュートリアル用オブジェクトを描画する。
    /// </summary>
    void DrawPastSelfTutorial();

    /// <summary>
    /// ImGuiで分身チュートリアル用の状態を表示する。
    /// </summary>
    void DrawPastSelfTutorialImGui();

    /// <summary>
    /// 分身チュートリアルルートの選択UIを表示する。
    /// </summary>
    void DrawPastSelfTutorialRouteSelector();

    /// <summary>
    /// 選択されたチュートリアルルートに対応するルールを適用する。
    /// </summary>
    void SelectPastSelfTutorialRoute(PastSelfTutorialRoute route);

    /// <summary>
    /// クリア済みのルートから次のチュートリアルルートへ進む。
    /// </summary>
    void AdvancePastSelfTutorialRoute();

    /// <summary>
    /// 現在選択中のチュートリアルルート名を取得する。
    /// </summary>
    const char* GetPastSelfTutorialRouteLabel() const;

    /// <summary>
    /// 現在選択中のチュートリアルルートが完了しているか判定する。
    /// </summary>
    bool IsPastSelfTutorialSelectedRouteComplete() const;

    /// <summary>
    /// 選択中のチュートリアルルートのクリア状態を確定する。
    /// </summary>
    void FinalizePastSelfTutorialSelectedRoute();

    /// <summary>
    /// プレイヤー操作に必要な主要状態を固定表示する。
    /// </summary>
    void DrawPastSelfTutorialFixedStatusHud();

    /// <summary>
    /// 現在の攻略状態から次に行う確認手順を取得する。
    /// </summary>
    const char* GetPastSelfTutorialNextActionText() const;

    /// <summary>
    /// 新たに達成した検証項目をHUD通知へ登録する。
    /// </summary>
    void RegisterPastSelfTutorialCheckCompleted(const char* checkText);

    /// <summary>
    /// 分身チュートリアル用の検証状態をPlayerタブ内に表示する。
    /// </summary>
    void DrawPastSelfTutorialStatusHud();

    /// <summary>
    /// キー入力で選択されたポストエフェクトを適用する。
    /// </summary>
    void ApplyPostProcessShortcut(MyEngine::PostEffectType effectType);

    /// <summary>
    /// 時間演出とポストプロセスの状態を更新する。
    /// </summary>
    void UpdateTemporalEffects(float deltaTime);

    /// <summary>
    /// 再生中エフェクトに対応するポストエフェクト中心を計算する。
    /// </summary>
    Math::Vector2 CalculatePostEffectCenter() const;

    /// <summary>
    /// ポストエフェクトの中心座標を更新する。
    /// </summary>
    void UpdatePostEffectCenters();

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
    /// ポストプロセス用リソースを解放する。
    /// </summary>
    void FinalizePostProcessTargets();

    /// <summary>
    /// シーンで登録したParticleManagerの状態をクリアする。
    /// </summary>
    void ClearSceneParticles();

    /// <summary>
    /// シーンが保持している表示用オブジェクトを解放する。
    /// </summary>
    void ReleaseSceneObjects();

    /// <summary>
    /// パーティクル描画用オブジェクトを解放する。
    /// </summary>
    void ReleaseParticleObjects();

    /// <summary>
    /// SkyBoxを解放する。
    /// </summary>
    void ReleaseSkyBox();

    /// <summary>
    /// パーティクルエミッターとParticleManagerを更新する。
    /// </summary>
    void UpdateParticleSystems(float deltaTime);

    /// <summary>
    /// シーン内の3Dオブジェクトを更新する。
    /// </summary>
    void UpdateSceneObjects(float deltaTime);
    /// <summary>
    /// シーン内3Dオブジェクトの衝突判定を更新する。
    /// </summary>
    void UpdateSceneCollisions();

    /// <summary>
    /// パーティクル描画用オブジェクトを初期化する
    /// </summary>
    void InitializeParticleObjects();

    /// <summary>
    /// パーティクル管理とエミッターを初期化する
    /// </summary>
    void InitializeParticleEffects();

    /// <summary>
    /// ParticleManagerに使用するグループと描画オブジェクトを登録する。
    /// </summary>
    void InitializeParticleManager();

    /// <summary>
    /// ヒット演出用エミッターを初期化する。
    /// </summary>
    void InitializeHitParticleEmitter();

    /// <summary>
    /// リング演出用エミッターを初期化する。
    /// </summary>
    void InitializeRingParticleEmitter();

    /// <summary>
    /// 円柱演出用エミッターを初期化する。
    /// </summary>
    void InitializeCylinderParticleEmitter();

    /// <summary>
    /// パーティクルエミッターを初期化する。
    /// </summary>
    void InitializeParticleEmitters();

    /// <summary>
    /// 時間演出用スプライトを初期化する
    /// </summary>
    void InitializeTemporalEffectSprites();

    /// <summary>
    /// 時空破砕で使用する残像スプライトを必要になった時点で作成する
    /// </summary>
    void EnsureTemporalRiftSprites();

    /// <summary>
    /// 時間逆行で使用するスプライトを必要になった時点で作成する
    /// </summary>
    void EnsureTimeReversalSprites();

    /// <summary>
    /// ポストプロセス用レンダーターゲットを初期化する
    /// </summary>
    void InitializePostProcessTargets();

    /// <summary>
    /// シーンで使用するテクスチャを読み込む。
    /// </summary>
    void LoadSceneTextures();

    /// <summary>
    /// 環境マップ用のSkyBoxを初期化する。
    /// </summary>
    void InitializeSkyBox();

    /// <summary>
    /// 3Dオブジェクトの初期設定を適用する。
    /// </summary>
    void ApplySceneObjectInitialSettings(MyEngine::Object3d& object3d, const std::string& modelFileName);

    /// <summary>
    /// 指定したモデルファイル名からシーン用3Dオブジェクトを生成する
    /// </summary>
    void CreateSceneObject(const std::string& modelFileName);

    /// <summary>
    /// レベルデータ内のオブジェクト一覧からシーン用3Dオブジェクトを生成する。
    /// </summary>
    void CreateSceneObjectsFromLevelData(const std::vector<MyEngine::LevelObjectData>& objectDataList);

    /// <summary>
    /// レベルデータ内の1オブジェクトからシーン用3Dオブジェクトを生成する。
    /// </summary>
    void CreateSceneObjectFromLevelData(const MyEngine::LevelObjectData& objectData);

    /// <summary>
    /// 既存のシーン用3DオブジェクトへレベルデータのTransformとColliderを反映する。
    /// </summary>
    bool ApplyLevelDataToExistingSceneObjects(const std::vector<MyEngine::LevelObjectData>& objectDataList, size_t& objectIndex);

    /// <summary>
    /// 現在のシーン用3DオブジェクトのTransformをレベルデータへ書き戻す。
    /// </summary>
    bool SyncSceneObjectsToLevelData();

    /// <summary>
    /// 現在のレベルデータで参照しているモデルを事前読み込みする。
    /// </summary>
    bool PreloadLevelModels();

    /// <summary>
    /// 指定したシーン用3DオブジェクトをLevelDataのルートへ追加する。
    /// </summary>
    bool AppendSceneObjectToLevelData(size_t objectIndex);

    /// <summary>
    /// 指定したシーン用3Dオブジェクトに対応するLevelData内MESHを削除する。
    /// </summary>
    bool RemoveSceneObjectFromLevelData(size_t objectIndex);

    /// <summary>
    /// 既存のシーン用3Dオブジェクトをレベルデータ階層へ順番に書き戻す。
    /// </summary>
    bool SyncSceneObjectsToLevelDataRecursive(std::vector<MyEngine::LevelObjectData>& objectDataList, size_t& objectIndex, const Math::Transform& parentTransform);

    /// <summary>
    /// 指定した番号のシーン用3Dオブジェクトを削除する
    /// </summary>
    void DeleteSceneObject(size_t objectIndex);

    /// <summary>
    /// 指定したテクスチャ名からシーン用スプライトを生成する。
    /// </summary>
    void CreateSceneSprite(const std::string& textureName);

    /// <summary>
    /// 指定した番号のシーン用スプライトを削除する。
    /// </summary>
    void DeleteSceneSprite(size_t spriteIndex);

    /// <summary>
    /// シーンで使用する3Dオブジェクトを初期化する。
    /// </summary>
    void InitializeSceneObjects();

    /// <summary>
    /// 現在の3DオブジェクトをクリアしてレベルJSONから作り直す。
    /// </summary>
    bool ReloadLevelSceneObjects();

    /// <summary>
    /// 現在保持しているレベルデータをシーン用3Dオブジェクトへ反映する。
    /// </summary>
    bool ApplyLevelDataToScene(bool applyCameraStart = false);

    /// <summary>
    /// 現在保持しているレベルデータの集計情報を更新する。
    /// </summary>
    void RefreshLevelDataSummary();

    /// <summary>
    /// 現在のレベルデータをJSONスナップショットとして保存する。
    /// </summary>
    bool SaveLevelSnapshot();

    /// <summary>
    /// レベルJSONの読み込み状態を記録する。
    /// </summary>
    void SetLevelLoadStatus(bool succeeded, const std::string& message);

    /// <summary>
    /// レベルJSONの保存状態を記録する。
    /// </summary>
    void SetLevelSaveStatus(bool succeeded, const std::string& message);

    /// <summary>
    /// LevelDataが未保存状態になったことを記録する。
    /// </summary>
    void MarkLevelDataDirty(const std::string& message, bool appliedToScene);

    /// <summary>
    /// MESH順の番号からLevelData内のオブジェクトを取得する。
    /// </summary>
    MyEngine::LevelObjectData* FindLevelMeshObjectByIndex(size_t objectIndex);

    /// <summary>
    /// MESH順の番号からLevelData内のオブジェクトを再帰的に取得する。
    /// </summary>
    MyEngine::LevelObjectData* FindLevelMeshObjectByIndexRecursive(std::vector<MyEngine::LevelObjectData>& objectDataList, size_t targetMeshIndex, size_t& currentMeshIndex);

    /// <summary>
    /// LevelDataの選択コライダーを対応するObject3dへ反映する。
    /// </summary>
    void ApplyLevelColliderEditToSceneObject(size_t objectIndex, const MyEngine::LevelObjectData& objectData);


    /// <summary>
    /// ポストプロセス描画が利用できるか判定する
    /// </summary>
    bool CanUsePostProcess() const;

    /// <summary>
    /// シーン描画結果をポストプロセス入力用RTへ描画する
    /// </summary>
    void DrawSceneToPostProcessTarget();

    /// <summary>
    /// 時間演出用のポストプロセス連鎖を適用する
    /// </summary>
    void ApplyTemporalPostProcessChain(uint32_t& postProcessSourceSrvIndex, MyEngine::PostEffectType& finalEffectType);

    /// <summary>
    /// Gaussian Filterの2pass描画が利用できるか判定する
    /// </summary>
    bool CanUseGaussianFilter(MyEngine::PostEffectType finalEffectType) const;

    /// <summary>
    /// Scene View表示用の最終RTが利用できるか判定する
    /// </summary>
    bool CanUseFinalRenderTarget() const;

    /// <summary>
    /// Gaussian Filterの1pass目を中間RTへ描画する
    /// </summary>
    void ApplyGaussianFirstPass(uint32_t& postProcessSourceSrvIndex);

    /// <summary>
    /// 最終ポストプロセス描画を実行する
    /// </summary>
    void DrawFinalPostProcessPass(uint32_t postProcessSourceSrvIndex, MyEngine::PostEffectType finalEffectType, bool useGaussianFilter);
    /// <summary>
    /// 最終描画に必要なポストプロセス状態を作成する
    /// </summary>
    PostProcessDrawContext BuildPostProcessDrawContext();

    /// <summary>
    /// 最終描画前に必要なポストプロセスの前段パスを適用する。
    /// </summary>
    void ApplyPostProcessPrePasses(PostProcessDrawContext& drawContext);

    /// <summary>
    /// Scene View用RTが必要な場合だけ描画先を切り替える
    /// </summary>
    void BeginSceneViewRenderTargetIfNeeded(bool useFinalRenderTarget);

    /// <summary>
    /// Scene View用RTへ描画していた場合だけ描画先を戻す
    /// </summary>
    void EndSceneViewRenderTargetIfNeeded(bool useFinalRenderTarget);

    /// <summary>
    /// 現在の描画先へポストプロセス結果とスプライトを描画する
    /// </summary>
    void DrawPostProcessOutputToCurrentTarget(const PostProcessDrawContext& drawContext);

    /// <summary>
    /// 作成済みのポストプロセス状態に従って最終結果を描画する
    /// </summary>
    void DrawPostProcessResult(const PostProcessDrawContext& drawContext);

    /// <summary>
    /// ポストプロセス付きでシーンを描画する
    /// </summary>
    bool DrawPostProcessedScene();

    /// <summary>
    /// シーン内の3D要素を描画する
    /// </summary>
    void DrawSceneContent();
    /// <summary>
    /// 登録済みの3Dオブジェクトを描画する。
    /// </summary>
    void DrawSceneObjects();

    /// <summary>
    /// 所有中の3Dオブジェクトから参照用ビューを作り直す。
    /// </summary>
    void RebuildObjectPointerView();

    /// <summary>
    /// 所有中のスプライトから参照用ビューを作り直す。
    /// </summary>
    void RebuildSpritePointerView();

    /// <summary>
    /// 所有中のパーティクルエミッターから参照用ビューを作り直す。
    /// </summary>
    void RebuildParticleEmitterPointerView();

    /// <summary>
    /// 次に生成する3Dオブジェクトへ割り当てるIDを取得する。
    /// </summary>
    uint32_t IssueObjectId();

    /// <summary>
    /// 次に生成するスプライトへ割り当てるIDを取得する。
    /// </summary>
    uint32_t IssueSpriteId();

    /// <summary>
    /// 次に生成するパーティクルエミッターへ割り当てるIDを取得する。
    /// </summary>
    uint32_t IssueParticleEmitterId();

    /// <summary>
    /// 3D空間とパーティクルを描画する
    /// </summary>
    void DrawWorldAndParticles();

    /// <summary>
    /// 蓄積した 3D デバッグラインを現在の描画先へ描画する。
    /// </summary>
    void DrawDebugLines3D();

    /// <summary>
    /// ポストプロセスの影響を受けないスプライトを描画する
    /// </summary>
    void DrawSprites();

    /// <summary>
    /// 蓄積した 2D デバッグラインを現在の描画先へ描画する。
    /// </summary>
    void DrawDebugLines2D();

private: // メンバー変数
    static constexpr bool kUsePostEffectPreviewScene = true; // ポストエフェクト確認用に関係ない演出描画を止める

    MyEngine::SceneContext ctx_;
    MyEngine::LevelData levelData_; // Blenderから読み込んだレベルデータ
    std::string levelDataFileName_; // 読み込み対象のレベルJSONファイル名
    std::string levelSaveFileName_; // 読み書き共通化後の保存先確認用レベルJSONファイル名
    bool levelLoadSucceeded_ = false; // 直近のレベルJSON読み込みが成功したか
    std::string levelLoadMessage_; // 直近のレベルJSON読み込み状態メッセージ
    bool levelSaveSucceeded_ = false; // 直近のレベルJSON保存が成功したか
    std::string levelSaveMessage_; // 直近のレベルJSON保存状態メッセージ
    bool levelDirty_ = false; // LevelDataに未保存の編集があるか
    bool levelAppliedToScene_ = false; // 現在のLevelDataがシーンへ反映済みか
    size_t levelTotalObjectCount_ = 0; // レベルJSONに含まれる総オブジェクト数
    size_t levelMeshObjectCount_ = 0; // レベルJSONから生成対象になったMesh数
    size_t levelColliderObjectCount_ = 0; // レベルJSONに含まれる有効コライダー数
    size_t levelDisabledObjectCount_ = 0; // レベルJSONで無効化されているオブジェクト数
    size_t levelSpawnPointCount_ = 0; // レベルJSONに含まれるスポーン地点数
    size_t levelEventTriggerCount_ = 0; // レベルJSONに含まれるイベントトリガー数
    size_t levelCameraStartCount_ = 0; // レベルJSONに含まれる開始カメラ数
    std::vector<std::unique_ptr<MyEngine::Sprite>> sprites_;
    std::vector<MyEngine::Sprite*> spritePointerView_; // ImGuiなど外部参照用のスプライト一覧
    uint32_t nextSpriteId_ = 1; // 次に生成するスプライトへ割り当てるID
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects3d_;
    std::vector<MyEngine::Object3d*> objectPointerView_; // ImGuiなど外部参照用の3Dオブジェクト一覧
    uint32_t nextObjectId_ = 1; // 次に生成する3Dオブジェクトへ割り当てるID
    MyEngine::CollisionSystem collisionSystem_; // シーン内3Dオブジェクトの衝突判定管理
    size_t lastCollisionPairCount_ = 0; // 直近フレームで衝突していたペア数
    Player player_; // 分身チュートリアルを操作するプレイヤー
    PastSelfTutorialRoute pastSelfTutorialRoute_ = PastSelfTutorialRoute::FinalChallenge; // 現在選択中のチュートリアルルート
    StageRuleSettings pastSelfTutorialStageRules_; // 分身チュートリアルステージへ適用する調整可能なルール
    PastSelfRecorder pastSelfRecorder_; // 分身用のプレイヤー状態記録
    PastSelfCloneManager pastSelfCloneManager_; // 記録済み状態を再生するチュートリアル用分身の管理クラス
    std::vector<PastSelfTutorialStageBlock> pastSelfTutorialStageBlocks_; // 分身チュートリアル用のステージブロック一覧
    std::vector<PastSelfTutorialGimmickLayout> pastSelfTutorialGimmickLayouts_; // JSONから読み込んだギミック配置一覧
    std::string pastSelfTutorialStageFileName_; // エディターで指定する実ステージJSON名
    std::string pastSelfTutorialStageFilePath_; // 実ステージJSONの保存先
    std::string pastSelfTutorialStageFileMessage_; // 直近の読み書き結果
    bool pastSelfTutorialStageFileSucceeded_ = false; // 直近の読み書きに成功したか
    bool pastSelfTutorialAutoCameraFollow_ = true; // プレイヤーと攻略対象へゲーム用カメラを自動追従させるか
    BoxSwitchGimmick pastSelfTutorialSwitch_; // 分身専用スイッチギミック
    LinkedDoorGimmick pastSelfTutorialDoor_; // スイッチ連動扉ギミック
    TimedSwitchGimmick pastSelfTutorialTimedSwitch_; // 時間差スイッチギミック
    LinkedDoorGimmick pastSelfTutorialTimedDoor_; // 時間差スイッチ連動扉ギミック
    ToggleSwitchGimmick pastSelfTutorialToggleSwitch_; // 1体用ルートのトグルスイッチギミック
    LinkedDoorGimmick pastSelfTutorialToggleGate_; // トグルスイッチに連動する1体用ルートのゲート
    MovingPlatformGimmick pastSelfTutorialToggleElevator_; // トグルスイッチに連動する1体用ルートの昇降足場
    WeightSwitchGimmick pastSelfTutorialWeightSwitch_; // 重さスイッチギミック
    LinkedBridgeGimmick pastSelfTutorialGoalBridge_; // 重さスイッチに連動するゴール前の橋ギミック
    OneWayGateGimmick pastSelfTutorialOneWayGate_; // 一方通行ゲートギミック
    BoxGoalGimmick pastSelfTutorialGoal_; // ゴール判定ギミック
    std::unique_ptr<MyEngine::Object3d> pastSelfTutorialCloneStartMarkerObject_; // 分身開始地点を示す案内マーカー
    std::unique_ptr<MyEngine::Object3d> pastSelfTutorialCloneEndMarkerObject_; // 分身終了地点を示す案内マーカー
    bool pastSelfTutorialSwitchActive_ = false; // スイッチが押されているか
    bool pastSelfTutorialDoorOpen_ = false; // 扉が開いているか
    bool pastSelfTutorialGoalReached_ = false; // ゴールに到達したか
    bool pastSelfTutorialDoorUnlockedByClone_ = false; // 分身入力で通常扉が開放済みか
    bool pastSelfTutorialPlayerOnSwitch_ = false; // プレイヤーが分身専用スイッチ上にいるか
    bool pastSelfTutorialCloneOnSwitch_ = false; // 分身が分身専用スイッチ上にいるか
    bool pastSelfTutorialDoorBlockedBeforeClone_ = false; // 分身なしで閉じた扉に阻まれたか
    bool pastSelfTutorialClonePlatformUsed_ = false; // プレイヤーが分身を足場として利用したか
    bool pastSelfTutorialDoorOpenedByClone_ = false; // 分身がスイッチを押して扉を開けたか
    bool pastSelfTutorialTimedSwitchActive_ = false; // 時間差スイッチが起動中か
    bool pastSelfTutorialTimedSwitchCloneOn_ = false; // 分身が時間差スイッチ上にいるか
    bool pastSelfTutorialTimedDoorOpen_ = false; // 時間差扉が開いているか
    bool pastSelfTutorialToggleSwitchActive_ = false; // トグルスイッチがONか
    bool pastSelfTutorialToggleSwitchCloneOn_ = false; // 分身がトグルスイッチ上にいるか
    bool pastSelfTutorialToggleGateOpen_ = false; // トグル連動ゲートが開いているか
    bool pastSelfTutorialToggleElevatorActive_ = false; // トグル連動昇降足場が稼働中か
    bool pastSelfTutorialOneCloneToggleActivated_ = false; // 1体用ルートで分身がトグルスイッチを起動したか
    bool pastSelfTutorialOneCloneElevatorRidden_ = false; // 1体用ルートでプレイヤーが昇降足場へ乗ったか
    bool pastSelfTutorialOneCloneBasicsComplete_ = false; // 1体用トグル昇降床ルートを完了したか
    bool pastSelfTutorialTwoCloneReplayPrepared_ = false; // 2体用ルートで保存分身を残して再生準備したか
    bool pastSelfTutorialTwoCloneSwitchesActivated_ = false; // 2体用ルートで別々の分身が緑と青のスイッチを同時起動したか
    bool pastSelfTutorialTwoCloneCooperationComplete_ = false; // 2体用連携ルートで青扉を通過したか
    bool pastSelfTutorialRouteClearFinalized_ = false; // 選択中のチュートリアルルートのクリア結果を確定済みか
    bool pastSelfTutorialOneCloneRouteCleared_ = false; // 1体用ルートを通算でクリア済みか
    bool pastSelfTutorialTwoCloneRouteCleared_ = false; // 2体用ルートを通算でクリア済みか
    bool pastSelfTutorialFinalChallengeCleared_ = false; // 最終課題を通算でクリア済みか
    bool pastSelfTutorialWeightSwitchActive_ = false; // 重さスイッチが起動中か
    bool pastSelfTutorialWeightPlayerOn_ = false; // プレイヤーが重さスイッチ上にいるか
    bool pastSelfTutorialWeightCloneOn_ = false; // 分身が重さスイッチ上にいるか
    bool pastSelfTutorialGoalBridgeUnlocked_ = false; // 重さスイッチ入力でゴール前の橋を解放済みか
    bool pastSelfTutorialGoalBridgeDeployed_ = false; // ゴール前の橋が展開されているか
    bool pastSelfTutorialOneWayGateBlocking_ = false; // 一方通行ゲートが戻りを塞いでいるか
    bool pastSelfTutorialTimedDoorOpened_ = false; // 時間差扉を開けたか
    bool pastSelfTutorialDualCloneSwitchesActivated_ = false; // 離れた2つのスイッチを複数分身で同時起動したか
    bool pastSelfTutorialWeightSwitchActivated_ = false; // 重さスイッチを起動したか
    bool pastSelfTutorialOneWayGateUsed_ = false; // 一方通行ゲートを通過したか
    bool pastSelfTutorialResetShown_ = false; // リセット状態から検証を開始したことをHUDで示すか
    bool pastSelfTutorialRecordStarted_ = false; // 分身記録を開始したことをHUDで示すか
    bool pastSelfTutorialRecordStopped_ = false; // 分身記録を停止したことをHUDで示すか
    bool pastSelfTutorialPrepareUsed_ = false; // 記録を残したPrepare操作を使ったことをHUDで示すか
    bool pastSelfTutorialReplayStarted_ = false; // 記録済み分身の再生を開始したことをHUDで示すか
    bool pastSelfTutorialRecordingPendingCommit_ = false; // 現在の記録を分身として保存する必要があるか
    bool pastSelfTutorialShowVerificationDetails_ = false; // 検証用の詳細HUDを表示するか
    float pastSelfTutorialElapsedTime_ = 0.0f; // 現在の挑戦開始からの経過時間
    float pastSelfTutorialClearTime_ = 0.0f; // クリア時点の経過時間
    float pastSelfTutorialLastRecordDuration_ = 0.0f; // 最後に確定した分身記録時間
    float pastSelfTutorialPrepareFeedbackSeconds_ = 0.0f; // Prepare成功表示を残す秒数
    std::string pastSelfTutorialRecentCheckText_; // 直近に達成した検証項目の表示文
    float pastSelfTutorialRecentCheckSeconds_ = 0.0f; // 達成通知を表示する残り時間
    Math::Vector3 pastSelfTutorialCameraFocus_ {}; // 追従補間後のチュートリアル用カメラ注視点
    float pastSelfTutorialCameraDistance_ = 0.0f; // 追従補間後のチュートリアル用カメラ距離
    uint32_t pastSelfTutorialRecordTakeCount_ = 0; // 現在の挑戦で開始した分身記録回数
    std::unique_ptr<MyEngine::Object3d> particlePlane_;
    std::unique_ptr<MyEngine::Object3d> particleRing_;
    std::unique_ptr<MyEngine::Object3d> particleCylinder_;
    ParticleEmitter pmEmitter_;
    ParticleEmitter ringEmitter_;
    ParticleEmitter cylinderEmitter_;
    std::vector<ParticleEmitter*> particleEmitterPointerView_; // ImGuiなど外部参照用のパーティクルエミッター一覧
    uint32_t nextParticleEmitterId_ = 1; // 次に生成するパーティクルエミッターへ割り当てるID
    std::unique_ptr<MyEngine::SkyBox> skybox_;
    std::vector<std::unique_ptr<MyEngine::Sprite>> temporalAfterimageSprites_; // Transform履歴を表示する残像スプライト
    std::vector<std::unique_ptr<MyEngine::Sprite>> timeReversalSprites_; // 時間逆行用パーティクルの表示スプライト
    std::vector<std::unique_ptr<MyEngine::Sprite>> timeReversalAfterimageSprites_; // 巻き戻り軌跡を表示する残像スプライト
    std::unique_ptr<MyEngine::Sprite> timeReversalConvergenceSprite_; // 収束時のフラッシュ表示スプライト
    MyEngine::PostProcess postProcess_; // 時間演出に使用するポストプロセス
    MyEngine::RenderTarget sceneRenderTarget_; // シーン描画結果を保持するRT
    bool sceneViewOnly_ = false; // シーンをScene View用RTだけに描画するか
    MyEngine::RenderTarget postProcessIntermediateTarget_; // ポストプロセス連鎖用の中間RT
    MyEngine::RenderTarget finalRenderTarget_; // Scene Viewへ渡す最終描画結果RT
    uint32_t dissolveMaskSrvIndex_ = UINT32_MAX; // Dissolveで使用するノイズマスクSRV
    TemporalRiftEffect temporalRiftEffect_; // 時空破砕エフェクト
    EffectType selectedEffectType_ = EffectType::DimensionalShatter; // ImGuiで選択中のエフェクト
    TimeReversalEffect timeReversalEffect_; // 時間逆行エフェクト
    TimeStopEffect timeStopEffect_; // 時間停止エフェクト
};
