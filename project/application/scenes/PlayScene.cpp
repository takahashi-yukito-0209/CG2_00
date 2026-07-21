#include "PlayScene.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/PrimitiveFactory.h"
#include "../../engine/3d/SkyBox.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/base/SrvManager.h"
#include "../../engine/base/WinApp.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/utility/mathUtility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr Vector2 kCenteredSpriteAnchor = { 0.5f, 0.5f }; // 中心基準で表示するスプライトのアンカー
constexpr Vector2 kTemporalSpriteDefaultSize = { 1.0f, 1.0f }; // 時間演出スプライトの初期サイズ
constexpr Vector4 kHiddenSpriteColor = { 0.0f, 0.0f, 0.0f, 0.0f }; // 非表示状態にするスプライト色
constexpr int kMaximumTemporalAfterimageCount = 8; // 時空破砕で保持できる最大残像数
constexpr int kMaximumTimeReversalParticleCount = 128; // 時間逆流で保持できる最大粒子数
constexpr int kMaximumTimeReversalAfterimageCount = 3; // 1粒子ごとに保持する最大残像数
constexpr std::array<float, 4> kSceneRenderTargetClearColor = { 0.53f, 0.71f, 0.82f, 1.0f }; // シーン描画RTのクリア色
constexpr std::array<float, 4> kTransparentRenderTargetClearColor = { 0.0f, 0.0f, 0.0f, 1.0f }; // 中間RTと最終RTのクリア色
constexpr bool kUseDepthBuffer = true; // RenderTargetに深度バッファを作成する
constexpr bool kNoDepthBuffer = false; // RenderTargetに深度バッファを作成しない
constexpr bool kCreateDepthSrv = true; // 深度SRVを作成する
constexpr bool kNoDepthSrv = false; // 深度SRVを作成しない
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
constexpr float kEmitterDefaultFrequency = 1.0f; // エミッターの初期発生間隔
constexpr float kStoppedDeltaTime = 0.0f; // ヒットストップや時間停止中に使用する停止時間
constexpr float kHitStopFinishedThreshold = 0.0f; // ヒットストップが終了したとみなす残り時間
constexpr uint32_t kDemoSpriteCount = 5; // 作成する確認用スプライト数
constexpr uint32_t kDemoSpriteTextureSwitchInterval = 2; // 同じ確認用テクスチャを連続で使う枚数
constexpr float kCubeEnvironmentCoefficient = 0.85f; // cubeに適用する環境マップ反射率
constexpr Vector3 kCubeInitialTranslate = { 3.0f, 0.0f, 0.0f }; // cubeの初期配置
constexpr size_t kTerrainObjectIndex = 5; // terrainモデルの登録番号
constexpr Vector3 kTerrainInitialScale = { 5.0f, 5.0f, 5.0f }; // terrainモデルの初期スケール
constexpr Vector3 kSkinningPreviewScale = { 3.0f, 3.0f, 3.0f }; // Skinning確認モデルの初期スケール
constexpr Vector3 kSkinningPreviewTranslate = { 0.0f, 0.0f, 0.0f }; // Skinning確認モデルの初期位置
constexpr bool kLoadEnvironmentMapOnStartup = false; // 遷移直後に環境マップを読み込むか
constexpr const char* kFenceModelKeyword = "fence"; // アルファ抜き設定を適用するモデル判定キーワード
constexpr const char* kCubeModelKeywordLower = "cube"; // cubeモデル判定用の小文字キーワード
constexpr const char* kAnimatedCubeModelFileName = "AnimatedCube/AnimatedCube.gltf";
constexpr const char* kSimpleSkinModelFileName = "simpleSkin/simpleSkin.gltf"; // Skinning確認用simpleSkinモデル
constexpr const char* kHumanSneakWalkModelFileName = "human/sneakWalk.gltf"; // Skinning確認用sneakWalkモデル
constexpr const char* kHumanWalkModelFileName = "human/walk.gltf"; // Skinning確認用walkモデル
constexpr const char* kCubeModelKeywordUpper = "Cube"; // cubeモデル判定用の大文字キーワード

struct SceneModelLoadDesc {
    const char* fileName; // モデルファイル名
    bool loadOnStartup; // PlayScene遷移直後に読み込むか
};

constexpr std::array<SceneModelLoadDesc, 10> kSceneModelLoadDescs = {
    SceneModelLoadDesc { "plane/plane.gltf", true },
    SceneModelLoadDesc { "bunny/bunny.obj", false },
    SceneModelLoadDesc { "teapot/teapot.obj", false },
    SceneModelLoadDesc { "fence/fence.obj", true },
    SceneModelLoadDesc { "sphere/sphere.gltf", true },
    SceneModelLoadDesc { "terrain/terrain.obj", false },
    SceneModelLoadDesc { kAnimatedCubeModelFileName, false },
    SceneModelLoadDesc { kSimpleSkinModelFileName, false },
    SceneModelLoadDesc { kHumanSneakWalkModelFileName, false },
    SceneModelLoadDesc { kHumanWalkModelFileName, false },
}; // シーンで扱うモデルと起動時ロード設定
constexpr const char* kEnvironmentMapTextureName = "rostock_laage_airport_4k.dds"; // 環境マップ用DDS名
constexpr const char* kCircleTextureName = "circle.png"; // 円形パーティクルに使用するテクスチャ名
constexpr const char* kCircleFlashTextureName = "circle2.png"; // 発光系スプライトに使用するテクスチャ名
constexpr const char* kGradationLineTextureName = "gradationLine.png"; // リングと円柱に使用するテクスチャ名
constexpr const char* kUvCheckerTextureName = "uvChecker.png"; // 確認用UVテクスチャ名
constexpr const char* kMonsterBallTextureName = "monsterBall.png"; // 確認用ボールテクスチャ名
constexpr std::array<const char*, 2> kDemoSpriteTextureNames = {
    kUvCheckerTextureName,
    kMonsterBallTextureName,
}; // 確認用スプライトに使用するテクスチャ名

constexpr const char* kDissolveMaskTextureName = "noise0.png"; // Dissolveに使用するノイズマスク名
constexpr std::array<const char*, 5> kSceneTextureNames = {
    kUvCheckerTextureName,
    kMonsterBallTextureName,
    kCircleTextureName,
    kGradationLineTextureName,
    kEnvironmentMapTextureName,
}; // シーン初期化時に読み込むテクスチャ名

constexpr const char* kCircleParticleGroupName = "Circle"; // 円形パーティクルグループ名
constexpr const char* kCheckerParticleGroupName = "Checker"; // チェッカーパーティクルグループ名
constexpr const char* kBallParticleGroupName = "Ball"; // ボールパーティクルグループ名
constexpr const char* kHitParticleGroupName = "Hit"; // ヒット演出パーティクルグループ名
constexpr const char* kRingParticleGroupName = "Ring"; // リング演出パーティクルグループ名
constexpr const char* kCylinderParticleGroupName = "Cylinder"; // 円柱演出パーティクルグループ名

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
/// ポストプロセス用のRenderTarget設定を作成する
/// </summary>
RenderTargetDesc CreatePostProcessRenderTargetDesc(
    DXGI_FORMAT format,
    bool useDepth,
    bool createDepthSrv,
    const std::array<float, 4>& clearColor)
{
    RenderTargetDesc desc {}; // 作成するRenderTarget設定
    desc.width = WinApp::kWindowWidth;
    desc.height = WinApp::kWindowHeight;
    desc.format = format;
    desc.useDepth = useDepth;
    desc.createColorSrv = true;
    desc.createDepthSrv = createDepthSrv;
    desc.resizeWithWindow = true;
    desc.clearColor = clearColor;
    return desc;
}

/// <summary>
/// 非表示状態の時間演出スプライトを作成する
/// </summary>
std::unique_ptr<Sprite> CreateHiddenTemporalSprite(const SceneContext& ctx, const std::string& textureName)
{
    auto sprite = std::make_unique<Sprite>(); // 作成する時間演出用スプライト
    sprite->Initialize(ctx.spriteCommon, textureName, ctx.imguiManager);
    sprite->SetAnchorPoint(kCenteredSpriteAnchor);
    sprite->SetSize(kTemporalSpriteDefaultSize);
    sprite->SetColor(kHiddenSpriteColor);
    sprite->Update();
    return sprite;
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

/// <summary>
/// 確認用スプライト番号から使用するテクスチャ名を取得する
/// </summary>
const char* GetDemoSpriteTextureName(uint32_t spriteIndex)
{
    const size_t textureIndex = (std::min)(
        static_cast<size_t>(spriteIndex / kDemoSpriteTextureSwitchInterval),
        kDemoSpriteTextureNames.size() - 1); // 使用する確認用テクスチャ番号
    return kDemoSpriteTextureNames[textureIndex];
}

/// <summary>
/// ヒットストップが残っているか判定する
/// </summary>
bool HasActiveHitStop(float hitStopRemainingTime)
{
    return hitStopRemainingTime > kHitStopFinishedThreshold;
}

/// <summary>
/// 指定したモデルファイル名に判定キーワードが含まれるか調べる
/// </summary>
bool ContainsModelKeyword(const std::string& modelFileName, const char* keyword)
{
    return modelFileName.find(keyword) != std::string::npos;
}

/// <summary>
/// アルファ抜き設定が必要なモデルか判定する
/// </summary>
bool IsFenceModelFile(const std::string& modelFileName)
{
    return ContainsModelKeyword(modelFileName, kFenceModelKeyword);
}

/// <summary>
/// 環境マップ確認用のcubeモデルか判定する
/// </summary>
bool IsCubeModelFile(const std::string& modelFileName)
{
    return ContainsModelKeyword(modelFileName, kCubeModelKeywordLower)
        || ContainsModelKeyword(modelFileName, kCubeModelKeywordUpper);
}

/// <summary>
/// 初期化時にアニメーションも設定するモデルか判定する
/// </summary>
bool IsAnimationModelFile(const std::string& modelFileName)
{
    return modelFileName == kAnimatedCubeModelFileName
        || modelFileName == kHumanSneakWalkModelFileName
        || modelFileName == kHumanWalkModelFileName;
}

/// <summary>
/// Skinning確認用に表示サイズを調整するモデルか判定する
/// </summary>
bool IsSkinningPreviewModelFile(const std::string& modelFileName)
{
    return modelFileName == kSimpleSkinModelFileName
        || modelFileName == kHumanSneakWalkModelFileName
        || modelFileName == kHumanWalkModelFileName;
}

/// <summary>
/// 指定したテクスチャをPlayScene遷移直後に読み込むか判定する。
/// </summary>
bool ShouldLoadTextureOnStartup(const char* textureName)
{
    if (!textureName) {
        return false;
    }

    if (!kLoadEnvironmentMapOnStartup && std::string(textureName) == kEnvironmentMapTextureName) {
        return false;
    }

    return true;
}
}

/// <summary>
/// コンストラクタ
/// </summary>
PlayScene::PlayScene() { }

/// <summary>
/// デストラクタ
/// </summary>
PlayScene::~PlayScene() { }

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

    particleManager->Initialize(ctx_.directXCommon, ctx_.object3dCommon, ctx_.srvManager, ctx_.textureManager, ctx_.imguiManager);
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
    InitializeHitParticleEmitter();
    InitializeRingParticleEmitter();
    InitializeCylinderParticleEmitter();
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
/// 時間演出用スプライトを初期化する
/// </summary>
void PlayScene::InitializeTemporalEffectSprites()
{
    temporalAfterimageSprites_.reserve(kMaximumTemporalAfterimageCount);
    timeReversalSprites_.reserve(kMaximumTimeReversalParticleCount);
    timeReversalAfterimageSprites_.reserve(kMaximumTimeReversalParticleCount * kMaximumTimeReversalAfterimageCount);
}

/// <summary>
/// 時空破砕で使用する残像スプライトを必要になった時点で作成する。
/// </summary>
void PlayScene::EnsureTemporalRiftSprites()
{
    while (temporalAfterimageSprites_.size() < kMaximumTemporalAfterimageCount) {
        auto afterimageSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時空破砕の残像表示に使うスプライト
        temporalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }
}

/// <summary>
/// 時間逆行で使用するスプライトを必要になった時点で作成する。
/// </summary>
void PlayScene::EnsureTimeReversalSprites()
{
    while (timeReversalSprites_.size() < kMaximumTimeReversalParticleCount) {
        auto particleSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時間逆行の粒子表示に使うスプライト
        timeReversalSprites_.push_back(std::move(particleSprite));
    }

    const size_t maximumRewindAfterimageSpriteCount = static_cast<size_t>(kMaximumTimeReversalParticleCount * kMaximumTimeReversalAfterimageCount); // 時間逆行の軌跡表示に必要な残像数
    while (timeReversalAfterimageSprites_.size() < maximumRewindAfterimageSpriteCount) {
        auto afterimageSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時間逆行の軌跡表示に使うスプライト
        timeReversalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }

    if (!timeReversalConvergenceSprite_) {
        timeReversalConvergenceSprite_ = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName);
    }
}

/// <summary>
/// ポストプロセス用レンダーターゲットを初期化する
/// </summary>
void PlayScene::InitializePostProcessTargets()
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 初期化に使用するDirectX基盤
    if (!directXCommon) {
        return;
    }

    const DXGI_FORMAT renderTargetFormat = directXCommon->GetSwapChainFormat(); // 各RTで使用するカラーフォーマット
    const RenderTargetDesc sceneRenderTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kUseDepthBuffer,
        kCreateDepthSrv,
        kSceneRenderTargetClearColor); // シーン描画用RT設定
    sceneRenderTarget_.Initialize(directXCommon, sceneRenderTargetDesc);

    const RenderTargetDesc intermediateTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kNoDepthBuffer,
        kNoDepthSrv,
        kTransparentRenderTargetClearColor); // ポストプロセス中間RT設定
    postProcessIntermediateTarget_.Initialize(directXCommon, intermediateTargetDesc);

    const RenderTargetDesc finalRenderTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kNoDepthBuffer,
        kNoDepthSrv,
        kTransparentRenderTargetClearColor); // Scene View表示用RT設定
    finalRenderTarget_.Initialize(directXCommon, finalRenderTargetDesc);

    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture(kDissolveMaskTextureName);
        dissolveMaskSrvIndex_ = ctx_.textureManager->GetSrvIndex(kDissolveMaskTextureName);
    }

    postProcess_.Initialize(directXCommon);
    postProcess_.SetEffectType(PostEffectType::Copy);
}

/// <summary>
/// 確認用スプライトを初期化する。
/// </summary>
void PlayScene::InitializeDemoSprites()
{
    for (uint32_t spriteIndex = 0; spriteIndex < kDemoSpriteCount; ++spriteIndex) {
        auto sprite = std::make_unique<Sprite>(); // 作成中の確認用スプライト
        const char* textureName = GetDemoSpriteTextureName(spriteIndex); // 使用する確認用テクスチャ名
        sprite->Initialize(ctx_.spriteCommon, textureName, ctx_.imguiManager);
        sprites_.push_back(std::move(sprite));
    }
}

/// <summary>
/// 3Dオブジェクトの初期設定を適用する。
/// </summary>
void PlayScene::ApplySceneObjectInitialSettings(Object3d& object3d, const std::string& modelFileName)
{
    const bool isFenceModel = IsFenceModelFile(modelFileName); // アルファ抜き用サンプラーが必要なモデルか
    if (isFenceModel) {
        object3d.SetUseAlphaCutoutSampler(true);
    }

    const bool isCubeModel = IsCubeModelFile(modelFileName); // 環境マップ確認用モデルか
    if (isCubeModel) {
        object3d.SetEnvironmentCoefficient(kCubeEnvironmentCoefficient);
        object3d.SetTranslate(kCubeInitialTranslate);
    }

    const bool isSkinningPreviewModel = IsSkinningPreviewModelFile(modelFileName); // Skinning確認用モデルか
    if (isSkinningPreviewModel) {
        object3d.SetScale(kSkinningPreviewScale);
        object3d.SetTranslate(kSkinningPreviewTranslate);
    }
}
/// <summary>
/// シーンで使用する3Dオブジェクトを初期化する。
/// </summary>
void PlayScene::InitializeSceneObjects()
{
    objects3d_.reserve(kSceneModelLoadDescs.size());
    for (const SceneModelLoadDesc& modelDesc : kSceneModelLoadDescs) {
        const char* modelFileName = modelDesc.fileName; // 初期化対象のモデルファイル名
        if (!modelDesc.loadOnStartup) {
            objects3d_.push_back(nullptr);
            continue;
        }

        auto object3d = std::make_unique<Object3d>(); // 作成中の3Dオブジェクト
        object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        object3d->SetModel(modelFileName);
        if (IsAnimationModelFile(modelFileName)) {
            object3d->SetAnimation(modelFileName);
        }
        ApplySceneObjectInitialSettings(*object3d, modelFileName);
        objects3d_.push_back(std::move(object3d));
    }
    if (objects3d_.size() > kTerrainObjectIndex && objects3d_[kTerrainObjectIndex]) {
        objects3d_[kTerrainObjectIndex]->SetScale(kTerrainInitialScale);
    }
}

/// <summary>
/// シーンで使用するテクスチャを読み込む。
/// </summary>
void PlayScene::LoadSceneTextures()
{
    if (!ctx_.textureManager) {
        return;
    }

    for (const char* textureName : kSceneTextureNames) {
        if (!ShouldLoadTextureOnStartup(textureName)) {
            continue;
        }
        ctx_.textureManager->LoadTexture(textureName);
    }
}

/// <summary>
/// 環境マップ用のSkyBoxを初期化する。
/// </summary>
void PlayScene::InitializeSkyBox()
{
    if (!kLoadEnvironmentMapOnStartup) {
        return;
    }

    if (!ctx_.textureManager || !ctx_.srvManager || !ctx_.directXCommon) {
        return;
    }
    const uint32_t environmentMapSrvIndex = ctx_.textureManager->GetSrvIndex(kEnvironmentMapTextureName); // 環境マップのSRV番号
    if (environmentMapSrvIndex == UINT32_MAX) {
        return;
    }

    skybox_ = std::make_unique<SkyBox>();
    skybox_->Initialize(ctx_.directXCommon, ctx_.srvManager, environmentMapSrvIndex);
    if (ctx_.object3dCommon) {
        ctx_.object3dCommon->SetEnvironmentMapSrvIndex(environmentMapSrvIndex);
    }
}

void PlayScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;

    LoadSceneTextures();
    InitializeSkyBox();
    InitializeDemoSprites();
    InitializeSceneObjects();
    InitializeParticleObjects();
    InitializeParticleEffects();
    InitializeTemporalEffectSprites();
    InitializePostProcessTargets();
}

/// <summary>
/// ポストプロセス用リソースを解放する。
/// </summary>
void PlayScene::FinalizePostProcessTargets()
{
    sceneRenderTarget_.Finalize();
    postProcessIntermediateTarget_.Finalize();
    finalRenderTarget_.Finalize();
    dissolveMaskSrvIndex_ = UINT32_MAX;
    postProcess_.Finalize();
}

/// <summary>
/// ParticleManagerを解放する。
/// </summary>
void PlayScene::FinalizeParticleManager()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 解放対象のパーティクル管理
    if (particleManager) {
        particleManager->Finalize();
    }
}

/// <summary>
/// シーンが保持している表示用オブジェクトを解放する。
/// </summary>
void PlayScene::ReleaseSceneObjects()
{
    sprites_.clear();
    objects3d_.clear();
    temporalAfterimageSprites_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
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
/// SkyBoxを解放する。
/// </summary>
void PlayScene::ReleaseSkyBox()
{
    if (skybox_) {
        skybox_->Finalize();
        skybox_.reset();
    }
}

/// <summary>
/// 終了処理を行う。
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    StopCameraShake();

    FinalizePostProcessTargets();
    FinalizeParticleManager();
    ReleaseSceneObjects();
    timeReversalEffect_.ResetState();
    ReleaseParticleObjects();
    ReleaseSkyBox();
    temporalRiftEffect_.ResetState(ctx_.camera);
    timeStopEffect_.ResetState();
    ctx_ = {};
}

/// <summary>
/// エフェクト開始入力を処理する。
/// </summary>
void PlayScene::HandleEffectStartInput()
{
    InputManager* inputManager = InputManager::GetInstance(); // エフェクト開始入力を取得する入力管理
    if (inputManager
        && inputManager->IsKeyJustPressed(DIK_R)
        && !IsAnyEffectPlaying()) {
        StartSelectedEffect();
    }
}

/// <summary>
/// 時間演出とポストプロセスの状態を更新する。
/// </summary>
void PlayScene::UpdateTemporalEffects(float deltaTime)
{
    const float hitStopRemainingTime = temporalRiftEffect_.GetHitStopRemainingTime(); // 現在のヒットストップ残り時間
    const bool hasActiveHitStop = HasActiveHitStop(hitStopRemainingTime); // ヒットストップ中か
    const float effectDeltaTime = hasActiveHitStop ? kStoppedDeltaTime : deltaTime; // ヒットストップを反映した演出時間
    UpdateTimeReversalTransformHistory();
    UpdateTemporalRiftEffect(effectDeltaTime);
    UpdateTimeReversalEffect(effectDeltaTime);
    UpdateTimeStopEffect(deltaTime);
    UpdateImpactResponse(deltaTime);
    if (!hasActiveHitStop) {
        UpdateTemporalAfterimages();
    }
    postProcess_.Update(deltaTime);
}

/// <summary>
/// 再生中エフェクトに対応するポストエフェクト中心を計算する。
/// </summary>
Vector2 PlayScene::CalculatePostEffectCenter() const
{
    if (timeStopEffect_.IsPlaying()) {
        return CalculateWorldScreenUv(timeStopEffect_.GetEffectPosition());
    }
    if (timeReversalEffect_.IsPlaying()) {
        return CalculateWorldScreenUv(timeReversalEffect_.GetEffectPosition());
    }

    return temporalRiftEffect_.GetScreenUv();
}

/// <summary>
/// ポストエフェクトの中心座標を更新する。
/// </summary>
void PlayScene::UpdatePostEffectCenters()
{
    temporalRiftEffect_.SetScreenUv(CalculateTemporalRiftScreenUv());
    const Vector2 postEffectCenter = CalculatePostEffectCenter(); // 再生中エフェクトに対応する画面中心
    postProcess_.SetRadialBlurCenter(postEffectCenter);
    postProcess_.SetDistortionCenter(postEffectCenter);
}

/// <summary>
/// 更新処理
/// </summary>
void PlayScene::Update(float dt)
{
    HandleEffectStartInput();
    UpdateTemporalEffects(dt);

    if (ctx_.camera) {
        ctx_.camera->Update();
    }
    UpdatePostEffectCenters();

    UpdateParticleSystems(dt);
    UpdateSceneObjects(dt);
    UpdateDemoSprites();

    UpdateAfterimageSprites();
    UpdateTimeReversalSprites();
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

/// <summary>
/// シーン内の3Dオブジェクトを更新する。
/// </summary>
void PlayScene::UpdateSceneObjects(float deltaTime)
{
    if (!ctx_.camera) {
        return;
    }

    const Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // 3Dオブジェクト更新に使用するビュー行列
    const Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // 3Dオブジェクト更新に使用する射影行列
    for (auto& object3d : objects3d_) { // 更新対象の3Dオブジェクト
        if (object3d) {
            object3d->UpdateAnimation(deltaTime);
            object3d->Update(viewMatrix, projectionMatrix);
        }
    }
}

/// <summary>
/// 確認用スプライトを更新する。
/// </summary>
void PlayScene::UpdateDemoSprites()
{
    for (auto& sprite : sprites_) { // 更新対象の確認用スプライト
        if (sprite) {
            sprite->Update();
        }
    }
}
/// <summary>
/// 描画処理を行う
/// </summary>
void PlayScene::Draw()
{
    if (DrawPostProcessedScene()) {
        return;
    }

    DrawSceneContent();
    DrawSprites();
}

/// <summary>
/// シーンに入るときの処理
/// </summary>
void PlayScene::OnEnter() { std::cout << "PlayScene OnEnter\n"; }

/// <summary>
/// シーンから出るときの処理
/// </summary>
void PlayScene::OnExit() { std::cout << "PlayScene OnExit\n"; }

/// <summary>
/// 描画モードの更新を受け取る
/// </summary>
void PlayScene::SetSelectedDrawType(int t)
{
    ctx_.selectedDrawType = t;
}

/// <summary>
/// Scene View用のオフスクリーン描画だけにするか設定する
/// </summary>
void PlayScene::SetSceneViewOnly(bool enabled)
{
    sceneViewOnly_ = enabled;
}

uint32_t PlayScene::GetSceneViewSrvIndex() const
{
    if (finalRenderTarget_.HasColorSrv()) {
        return finalRenderTarget_.GetColorSrvIndex();
    }
    if (sceneRenderTarget_.HasColorSrv()) {
        return sceneRenderTarget_.GetColorSrvIndex();
    }
    return UINT32_MAX;
}

/// <summary>
/// シーンが使用しているポストプロセスを取得する
/// </summary>
PostProcess* PlayScene::GetPostProcess()
{
    return &postProcess_;
}

/// <summary>
/// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out)
        return;
    out->clear();
    out->reserve(objects3d_.size());
    for (auto& o : objects3d_) {
        out->push_back(o.get());
    }
}

/// <summary>
/// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out)
        return;
    out->clear();
    out->reserve(sprites_.size());
    for (auto& s : sprites_) {
        out->push_back(s.get());
    }
}
