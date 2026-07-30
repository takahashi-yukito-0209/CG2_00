#pragma once
#include "../../engine/base/IScene.h"
#include "../../engine/base/PostProcess.h"
#include "../../engine/base/RenderTarget.h"
#include "../../engine/level/LevelData.h"
#include "../../engine/utility/CollisionSystem.h"
#include "../effects/TemporalRiftEffect.h"
#include "../effects/TimeReversalEffect.h"
#include "../effects/TimeStopEffect.h"

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
    /// ImGuiでレベルJSONの読み込み状態を表示する。
    /// </summary>
    void DrawLevelDataImGui();

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
    /// 評価確認用のアニメーション操作入力を処理する。
    /// </summary>
    void HandleEvaluationAnimationInput();

    /// <summary>
    /// 評価確認用のSkinningモデル移動入力を処理する。
    /// </summary>
    void HandleSkinningModelControlInput(float deltaTime);

    /// <summary>
    /// 評価確認用に操作するSkinningモデルを取得する。
    /// </summary>
    MyEngine::Object3d* FindEvaluationSkinningControlObject() const;

    /// <summary>
    /// シーン内アニメーションの再生有効状態をまとめて設定する。
    /// </summary>
    void SetSceneAnimationEnabled(bool enabled);

    /// <summary>
    /// シーン内アニメーションの再生有効状態を切り替える。
    /// </summary>
    void ToggleSceneAnimationEnabled();

    /// <summary>
    /// シーン内アニメーションの再生速度をまとめて変更する。
    /// </summary>
    void AdjustSceneAnimationPlaybackSpeed(float speedDelta);

    /// <summary>
    /// シーン内アニメーションを先頭へ戻す。
    /// </summary>
    void ResetSceneAnimations();

    /// <summary>
    /// 評価確認用操作のImGuiを描画する。
    /// </summary>
    void DrawEvaluationControlImGui();

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
    bool ApplyLevelDataToScene();

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
    std::string levelSaveFileName_; // 書き出し対象のレベルJSONファイル名
    bool levelLoadSucceeded_ = false; // 直近のレベルJSON読み込みが成功したか
    std::string levelLoadMessage_; // 直近のレベルJSON読み込み状態メッセージ
    bool levelSaveSucceeded_ = false; // 直近のレベルJSON保存が成功したか
    std::string levelSaveMessage_; // 直近のレベルJSON保存状態メッセージ
    bool levelDirty_ = false; // LevelDataに未保存の編集があるか
    bool levelAppliedToScene_ = false; // 現在のLevelDataがシーンへ反映済みか
    size_t levelTotalObjectCount_ = 0; // レベルJSONに含まれる総オブジェクト数
    size_t levelMeshObjectCount_ = 0; // レベルJSONから生成対象になったMesh数
    size_t levelColliderObjectCount_ = 0; // レベルJSONに含まれる有効コライダー数
    std::vector<std::unique_ptr<MyEngine::Sprite>> sprites_;
    std::vector<MyEngine::Sprite*> spritePointerView_; // ImGuiなど外部参照用のスプライト一覧
    uint32_t nextSpriteId_ = 1; // 次に生成するスプライトへ割り当てるID
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects3d_;
    std::vector<MyEngine::Object3d*> objectPointerView_; // ImGuiなど外部参照用の3Dオブジェクト一覧
    uint32_t nextObjectId_ = 1; // 次に生成する3Dオブジェクトへ割り当てるID
    MyEngine::CollisionSystem collisionSystem_; // シーン内3Dオブジェクトの衝突判定管理
    size_t lastCollisionPairCount_ = 0; // 直近フレームで衝突していたペア数
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
