#include "PlayScene.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/PrimitiveFactory.h"
#include "../../engine/particle/ParticleManager.h"

#include <memory>
#include <vector>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr float kParticleRingOuterRadius = 1.0f; // パーティクルリングの外径
constexpr float kParticleRingInnerRadius = 0.2f; // パーティクルリングの内径
constexpr float kParticleCylinderTopRadius = 1.0f; // パーティクル円柱の上面半径
constexpr float kParticleCylinderBottomRadius = 1.0f; // パーティクル円柱の下面半径
constexpr float kParticleCylinderHeight = 1.0f; // パーティクル円柱の高さ
constexpr uint32_t kParticleCylinderDivide = 32; // パーティクル円柱の分割数
constexpr bool kUseAlphaCutoutSampler = true; // アルファ抜き用サンプラーを使用する
constexpr bool kNoAlphaCutoutSampler = false; // アルファ抜き用サンプラーを使用しない
constexpr Vector3 kEmitterDefaultPosition = { 0.0f, 0.0f, 0.0f }; // エミッターの初期位置
constexpr int kHitEmitterParticleCount = 8; // ヒット演出で発生させる粒子数
constexpr int kSingleEffectEmitterCount = 1; // 単発演出で発生させる粒子数
constexpr float kEmitterDefaultFrequency = 0.25f; // エフェクト確認時に短寿命粒子が途切れにくい初期発生間隔
constexpr float kStoppedDeltaTime = 0.0f; // ヒットストップや時間停止中に使用する停止時間
constexpr float kHitStopFinishedThreshold = 0.0f; // ヒットストップが終了したとみなす残り時間
constexpr const char* kCircleTextureName = "circle.png"; // 円形パーティクルに使用するテクスチャ名
constexpr const char* kCircleFlashTextureName = "circle2.png"; // 発光系スプライトに使用するテクスチャ名
constexpr const char* kGradationLineTextureName = "gradationLine.png"; // リングと円柱に使用するテクスチャ名
constexpr const char* kUvCheckerTextureName = "uvChecker.png"; // 確認用UVテクスチャ名
constexpr const char* kMonsterBallTextureName = "monsterBall.png"; // 確認用ボールテクスチャ名
constexpr const char* kCircleParticleGroupName = "Circle"; // 円形パーティクルグループ名
constexpr const char* kCheckerParticleGroupName = "Checker"; // チェッカーパーティクルグループ名
constexpr const char* kBallParticleGroupName = "Ball"; // ボールパーティクルグループ名
constexpr const char* kHitParticleGroupName = "Hit"; // ヒット演出パーティクルグループ名
constexpr const char* kRingParticleGroupName = "Ring"; // リング演出パーティクルグループ名
constexpr const char* kCylinderParticleGroupName = "Cylinder"; // 円柱演出パーティクルグループ名

/// <summary>
/// ヒットストップが残っているか判定する
/// </summary>
bool HasActiveHitStop(float hitStopRemainingTime)
{
    return hitStopRemainingTime > kHitStopFinishedThreshold;
}

/// <summary>
/// パーティクルエミッターで使用する演出種別
/// </summary>
enum class ParticleEmitterEffectType {
    Hit,
    Ring,
    Cylinder,
};

/// <summary>
/// パーティクルエミッターの共通設定を適用する
/// </summary>
void ConfigureParticleEmitter(
    ParticleEmitter& emitter,
    const char* groupName,
    int particleCount,
    ParticleEmitterEffectType effectType)
{
    emitter.groupName = groupName;
    emitter.transform.translate = kEmitterDefaultPosition;
    emitter.count = particleCount;
    emitter.frequency = kEmitterDefaultFrequency;
    emitter.useHitEffect = false;
    emitter.useRingEffect = false;
    emitter.useCylinderEffect = false;

    switch (effectType) {
    case ParticleEmitterEffectType::Hit:
        emitter.useHitEffect = true;
        break;
    case ParticleEmitterEffectType::Ring:
        emitter.useRingEffect = true;
        break;
    case ParticleEmitterEffectType::Cylinder:
        emitter.useCylinderEffect = true;
        break;
    }

    emitter.Emit();
}

/// <summary>
/// パーティクル描画用の3Dオブジェクトを作成する
/// </summary>
std::unique_ptr<Object3d> CreateParticleDrawObject(
    const SceneContext& ctx,
    const std::vector<Object3d::VertexData>& meshVertices,
    const char* textureName,
    bool useAlphaCutoutSampler)
{
    auto object3d = std::make_unique<Object3d>(); // 作成するパーティクル描画用オブジェクト
    object3d->Initialize(ctx.object3dCommon, ctx.imguiManager);
    object3d->SetMesh(meshVertices);
    object3d->SetTexture(textureName);
    object3d->SetEnableLighting(false);
    object3d->SetUseAlphaCutoutSampler(useAlphaCutoutSampler);
    return object3d;
}
} // namespace

/// <summary>
/// パーティクル描画用オブジェクトを初期化する
/// </summary>
void PlayScene::InitializeParticleObjects()
{
    const std::vector<Object3d::VertexData> planeMesh = PrimitiveFactory::CreatePlane(); // 平面パーティクル用メッシュ
    particlePlane_ = CreateParticleDrawObject(
        ctx_,
        planeMesh,
        kCircleTextureName,
        kNoAlphaCutoutSampler);

    const std::vector<Object3d::VertexData> ringMesh = PrimitiveFactory::CreateRing(
        kParticleRingOuterRadius,
        kParticleRingInnerRadius); // リングパーティクル用メッシュ
    particleRing_ = CreateParticleDrawObject(
        ctx_,
        ringMesh,
        kGradationLineTextureName,
        kUseAlphaCutoutSampler);

    const std::vector<Object3d::VertexData> cylinderMesh = PrimitiveFactory::CreateCylinder(
        kParticleCylinderTopRadius,
        kParticleCylinderBottomRadius,
        kParticleCylinderHeight,
        kParticleCylinderDivide); // 円柱パーティクル用メッシュ
    particleCylinder_ = CreateParticleDrawObject(
        ctx_,
        cylinderMesh,
        kGradationLineTextureName,
        kUseAlphaCutoutSampler);
}

/// <summary>
/// ParticleManagerに使用するグループと描画オブジェクトを登録する。
/// </summary>
void PlayScene::InitializeParticleManager()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル全体を管理するインスタンス
    if (!particleManager) {
        return;
    }

    particleManager->ClearSceneParticles();
    particleManager->SetParticlePlane(particlePlane_.get());
    particleManager->CreateParticleGroup(kCircleParticleGroupName, kCircleTextureName);
    particleManager->CreateParticleGroup(kCheckerParticleGroupName, kUvCheckerTextureName);
    particleManager->CreateParticleGroup(kBallParticleGroupName, kMonsterBallTextureName);
    particleManager->CreateParticleGroup(kHitParticleGroupName, kCircleFlashTextureName);
    particleManager->CreateParticleGroup(kRingParticleGroupName, kGradationLineTextureName);
    particleManager->CreateParticleGroup(kCylinderParticleGroupName, kGradationLineTextureName);
    particleManager->SetParticleObject(kHitParticleGroupName, particlePlane_.get());
    particleManager->SetParticleObject(kRingParticleGroupName, particleRing_.get());
    particleManager->SetParticleObject(kCylinderParticleGroupName, particleCylinder_.get());
    particleManager->SetGroupBillboard(kCylinderParticleGroupName, false);
}

/// <summary>
/// ヒット演出用エミッターを初期化する。
/// </summary>
void PlayScene::InitializeHitParticleEmitter()
{
    ConfigureParticleEmitter(
        pmEmitter_,
        kHitParticleGroupName,
        kHitEmitterParticleCount,
        ParticleEmitterEffectType::Hit);
}

/// <summary>
/// リング演出用エミッターを初期化する。
/// </summary>
void PlayScene::InitializeRingParticleEmitter()
{
    ConfigureParticleEmitter(
        ringEmitter_,
        kRingParticleGroupName,
        kSingleEffectEmitterCount,
        ParticleEmitterEffectType::Ring);
}

/// <summary>
/// 円柱演出用エミッターを初期化する。
/// </summary>
void PlayScene::InitializeCylinderParticleEmitter()
{
    ConfigureParticleEmitter(
        cylinderEmitter_,
        kCylinderParticleGroupName,
        kSingleEffectEmitterCount,
        ParticleEmitterEffectType::Cylinder);
}

/// <summary>
/// パーティクルエミッターを初期化する。
/// </summary>
void PlayScene::InitializeParticleEmitters()
{
    nextParticleEmitterId_ = 1;
    pmEmitter_.SetEmitterId(IssueParticleEmitterId());
    ringEmitter_.SetEmitterId(IssueParticleEmitterId());
    cylinderEmitter_.SetEmitterId(IssueParticleEmitterId());

    InitializeHitParticleEmitter();
    InitializeRingParticleEmitter();
    InitializeCylinderParticleEmitter();
    RebuildParticleEmitterPointerView();
}

/// <summary>
/// パーティクル管理とエミッターを初期化する。
/// </summary>
void PlayScene::InitializeParticleEffects()
{
    InitializeParticleManager();
    InitializeParticleEmitters();
}

/// <summary>
/// シーンで登録したParticleManagerの状態をクリアする。
/// </summary>
void PlayScene::ClearSceneParticles()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // シーン登録状態をクリアするパーティクル管理
    if (particleManager) {
        particleManager->ClearSceneParticles();
    }
}

/// <summary>
/// パーティクル描画用オブジェクトを解放する。
/// </summary>
void PlayScene::ReleaseParticleObjects()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル描画オブジェクトの参照元
    if (particleManager) {
        particleManager->SetParticlePlane(nullptr);
    }

    particlePlane_.reset();
    particleRing_.reset();
    particleCylinder_.reset();
}
/// <summary>
/// パーティクルエミッターとParticleManagerを更新する。
/// </summary>
void PlayScene::UpdateParticleSystems(float deltaTime)
{
    const bool hasActiveHitStop = HasActiveHitStop(temporalRiftEffect_.GetHitStopRemainingTime()); // ヒットストップ中か
    const float particleDeltaTime = (hasActiveHitStop || IsTimeStopped()) ? kStoppedDeltaTime : deltaTime; // ヒットストップと時間停止を反映したパーティクル時間
    pmEmitter_.Update(particleDeltaTime);
    ringEmitter_.Update(particleDeltaTime);
    cylinderEmitter_.Update(particleDeltaTime);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル全体を更新する管理クラス
    if (particleManager) {
        particleManager->Update(particleDeltaTime);
    }
}
