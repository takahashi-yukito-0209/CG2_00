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
    // パーティクルの初期化
    particlePlane_ = std::make_unique<Object3d>();
    particlePlane_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    particlePlane_->SetMesh(PrimitiveFactory::CreatePlane());
    particlePlane_->SetTexture("circle.png");
    particlePlane_->SetEnableLighting(false);

    particleRing_ = std::make_unique<Object3d>();
    particleRing_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    particleRing_->SetMesh(PrimitiveFactory::CreateRing(1.0f, 0.2f));
    particleRing_->SetTexture("gradationLine.png");
    particleRing_->SetEnableLighting(false);
    particleRing_->SetUseAlphaCutoutSampler(true);

    particleCylinder_ = std::make_unique<Object3d>();
    particleCylinder_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    particleCylinder_->SetMesh(PrimitiveFactory::CreateCylinder(1.0f, 1.0f, 1.0f));
    particleCylinder_->SetTexture("gradationLine.png");
    particleCylinder_->SetEnableLighting(false);
    particleCylinder_->SetUseAlphaCutoutSampler(true);
}

/// <summary>
/// パーティクル管理とエミッターを初期化する
/// </summary>
void PlayScene::InitializeParticleEffects()
{
    // パーティクルマネージャー化とグループの作成
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Initialize(ctx_.directXCommon, ctx_.object3dCommon, ctx_.srvManager, ctx_.textureManager, ctx_.imguiManager);
        ParticleManager::GetInstance()->SetParticlePlane(particlePlane_.get());
        ParticleManager::GetInstance()->CreateParticleGroup("Circle", "circle.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Checker", "uvChecker.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Ball", "monsterBall.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Hit", "circle2.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Ring", "gradationLine.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Cylinder", "gradationLine.png");
        ParticleManager::GetInstance()->SetParticleObject("Hit", particlePlane_.get());
        ParticleManager::GetInstance()->SetParticleObject("Ring", particleRing_.get());
        ParticleManager::GetInstance()->SetParticleObject("Cylinder", particleCylinder_.get());
        ParticleManager::GetInstance()->SetGroupBillboard("Cylinder", false);
    }

    // パーティクルエミッターと発射
    pmEmitter_.groupName = "Hit";
    pmEmitter_.transform.translate = { 0.0f, 0.0f, 0.0f };
    pmEmitter_.count = 8;
    pmEmitter_.frequency = 1.0f;
    pmEmitter_.useHitEffect = true;
    pmEmitter_.Emit();

    ringEmitter_.groupName = "Ring";
    ringEmitter_.transform.translate = { 0.0f, 0.0f, 0.0f };
    ringEmitter_.count = 1;
    ringEmitter_.frequency = 1.0f;
    ringEmitter_.useRingEffect = true;
    ringEmitter_.Emit();

    cylinderEmitter_.groupName = "Cylinder";
    cylinderEmitter_.transform.translate = { 0.0f, 0.0f, 0.0f };
    cylinderEmitter_.count = 1;
    cylinderEmitter_.frequency = 1.0f;
    cylinderEmitter_.useCylinderEffect = true;
    cylinderEmitter_.Emit();
}

/// <summary>
/// 時間演出用スプライトを初期化する
/// </summary>
void PlayScene::InitializeTemporalEffectSprites()
{
    constexpr int kMaximumAfterimageCount = 8; // 調整UIで使用できる最大残像数
    temporalAfterimageSprites_.reserve(kMaximumAfterimageCount);
    for (int afterimageIndex = 0; afterimageIndex < kMaximumAfterimageCount; ++afterimageIndex) {
        auto afterimageSprite = std::make_unique<Sprite>(); // Transform履歴を表示する残像スプライト
        afterimageSprite->Initialize(
            ctx_.spriteCommon,
            "circle2.png",
            ctx_.imguiManager);
        afterimageSprite->SetAnchorPoint(kCenteredSpriteAnchor);
        afterimageSprite->SetSize({ 1.0f, 1.0f });
        afterimageSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        afterimageSprite->Update();
        temporalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }

    constexpr int kMaximumTimeReversalParticleCount = 128; // 時間逆流で保持できる最大粒子数
    timeReversalSprites_.reserve(kMaximumTimeReversalParticleCount);
    for (int particleIndex = 0; particleIndex < kMaximumTimeReversalParticleCount; ++particleIndex) {
        auto particleSprite = std::make_unique<Sprite>(); // 時間逆流専用の粒子スプライト
        particleSprite->Initialize(
            ctx_.spriteCommon,
            "circle2.png",
            ctx_.imguiManager);
        particleSprite->SetAnchorPoint(kCenteredSpriteAnchor);
        particleSprite->SetSize({ 1.0f, 1.0f });
        particleSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        particleSprite->Update();
        timeReversalSprites_.push_back(std::move(particleSprite));
    }

    constexpr int kMaximumRewindAfterimageCount = 3; // 1粒子ごとに保持する最大残像数
    const int maximumRewindAfterimageSpriteCount = kMaximumTimeReversalParticleCount * kMaximumRewindAfterimageCount; // 確保する残像スプライト総数
    timeReversalAfterimageSprites_.reserve(maximumRewindAfterimageSpriteCount);
    for (int afterimageIndex = 0;
        afterimageIndex < maximumRewindAfterimageSpriteCount;
        ++afterimageIndex) {
        auto afterimageSprite = std::make_unique<Sprite>(); // 巻き戻し軌道用の残像スプライト
        afterimageSprite->Initialize(
            ctx_.spriteCommon,
            "circle2.png",
            ctx_.imguiManager);
        afterimageSprite->SetAnchorPoint(kCenteredSpriteAnchor);
        afterimageSprite->SetSize({ 1.0f, 1.0f });
        afterimageSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        afterimageSprite->Update();
        timeReversalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }

    timeReversalConvergenceSprite_ = std::make_unique<Sprite>();
    timeReversalConvergenceSprite_->Initialize(
        ctx_.spriteCommon,
        "circle2.png",
        ctx_.imguiManager);
    timeReversalConvergenceSprite_->SetAnchorPoint(kCenteredSpriteAnchor);
    timeReversalConvergenceSprite_->SetSize({ 1.0f, 1.0f });
    timeReversalConvergenceSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    timeReversalConvergenceSprite_->Update();
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

    RenderTargetDesc sceneRenderTargetDesc {}; // シーン描画用RT設定
    sceneRenderTargetDesc.width = WinApp::kWindowWidth;
    sceneRenderTargetDesc.height = WinApp::kWindowHeight;
    sceneRenderTargetDesc.format = directXCommon->GetSwapChainFormat();
    sceneRenderTargetDesc.useDepth = true;
    sceneRenderTargetDesc.createColorSrv = true;
    sceneRenderTargetDesc.createDepthSrv = true;
    sceneRenderTargetDesc.resizeWithWindow = true;
    sceneRenderTargetDesc.clearColor = { 0.53f, 0.71f, 0.82f, 1.0f };
    sceneRenderTarget_.Initialize(directXCommon, sceneRenderTargetDesc);

    RenderTargetDesc intermediateTargetDesc {}; // ポストプロセス中間RT設定
    intermediateTargetDesc.width = WinApp::kWindowWidth;
    intermediateTargetDesc.height = WinApp::kWindowHeight;
    intermediateTargetDesc.format = directXCommon->GetSwapChainFormat();
    intermediateTargetDesc.useDepth = false;
    intermediateTargetDesc.createColorSrv = true;
    intermediateTargetDesc.createDepthSrv = false;
    intermediateTargetDesc.resizeWithWindow = true;
    intermediateTargetDesc.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    postProcessIntermediateTarget_.Initialize(directXCommon, intermediateTargetDesc);

    RenderTargetDesc finalRenderTargetDesc {}; // Scene View表示用RT設定
    finalRenderTargetDesc.width = WinApp::kWindowWidth;
    finalRenderTargetDesc.height = WinApp::kWindowHeight;
    finalRenderTargetDesc.format = directXCommon->GetSwapChainFormat();
    finalRenderTargetDesc.useDepth = false;
    finalRenderTargetDesc.createColorSrv = true;
    finalRenderTargetDesc.createDepthSrv = false;
    finalRenderTargetDesc.resizeWithWindow = true;
    finalRenderTargetDesc.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    finalRenderTarget_.Initialize(directXCommon, finalRenderTargetDesc);

    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture("noise0.png");
        dissolveMaskSrvIndex_ = ctx_.textureManager->GetSrvIndex("noise0.png");
    }

    postProcess_.Initialize(directXCommon);
    postProcess_.SetEffectType(PostEffectType::Copy);
}

/// <summary>
/// 確認用スプライトを初期化する。
/// </summary>
void PlayScene::InitializeDemoSprites()
{
    constexpr uint32_t kSpriteCount = 5; // 作成する確認用スプライト数
    const std::array<std::string, 2> spriteNames = {
        "uvChecker",
        "monsterBall"
    }; // 確認用スプライトに使用するテクスチャ名

    for (uint32_t spriteIndex = 0; spriteIndex < kSpriteCount; ++spriteIndex) {
        auto sprite = std::make_unique<Sprite>(); // 作成中の確認用スプライト
        const std::string textureName = spriteNames[(spriteIndex / 2) == 0 ? 0 : 1] + ".png"; // 使用するテクスチャ名
        sprite->Initialize(ctx_.spriteCommon, textureName, ctx_.imguiManager);
        sprites_.push_back(std::move(sprite));
    }
}

/// <summary>
/// 3Dオブジェクトの初期設定を適用する。
/// </summary>
void PlayScene::ApplySceneObjectInitialSettings(Object3d& object3d, const std::string& modelFileName)
{
    const bool isFenceModel = modelFileName.find("fence") != std::string::npos; // アルファ抜き用サンプラーが必要なモデルか
    if (isFenceModel) {
        object3d.SetUseAlphaCutoutSampler(true);
    }

    const bool isCubeModel = modelFileName.find("cube") != std::string::npos || modelFileName.find("Cube") != std::string::npos; // 環境マップ確認用モデルか
    if (isCubeModel) {
        constexpr float kCubeEnvironmentCoefficient = 0.85f; // cubeに適用する環境マップ反射率
        const Vector3 kCubeTranslate = { 3.0f, 0.0f, 0.0f }; // cubeの初期配置
        object3d.SetEnvironmentCoefficient(kCubeEnvironmentCoefficient);
        object3d.SetTranslate(kCubeTranslate);
    }
}

/// <summary>
/// シーンで使用する3Dオブジェクトを初期化する。
/// </summary>
void PlayScene::InitializeSceneObjects()
{
    const std::vector<std::string> modelFileNames = {
        "plane/plane.gltf",
        "bunny/bunny.obj",
        "teapot/teapot.obj",
        "fence/fence.obj",
        "sphere/sphere.gltf",
        "terrain/terrain.obj",
        "cube/Cube.obj"
    }; // シーンで生成するモデルファイル名

    for (const std::string& modelFileName : modelFileNames) {
        auto object3d = std::make_unique<Object3d>(); // 作成中の3Dオブジェクト
        object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        object3d->SetModel(modelFileName);
        ApplySceneObjectInitialSettings(*object3d, modelFileName);
        objects3d_.push_back(std::move(object3d));
    }

    constexpr size_t kTerrainObjectIndex = 5; // terrainモデルの登録番号
    if (objects3d_.size() > kTerrainObjectIndex && objects3d_[kTerrainObjectIndex]) {
        const Vector3 kTerrainScale = { 5.0f, 5.0f, 5.0f }; // terrainモデルの初期スケール
        objects3d_[kTerrainObjectIndex]->SetScale(kTerrainScale);
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

    ctx_.textureManager->LoadTexture("uvChecker.png");
    ctx_.textureManager->LoadTexture("monsterBall.png");
    ctx_.textureManager->LoadTexture("circle.png");
    ctx_.textureManager->LoadTexture("gradationLine.png");
    constexpr const char* kEnvironmentMapTextureName = "rostock_laage_airport_4k.dds"; // 環境マップ用DDS名
    ctx_.textureManager->LoadTexture(kEnvironmentMapTextureName);
}

/// <summary>
/// 環境マップ用のSkyBoxを初期化する。
/// </summary>
void PlayScene::InitializeSkyBox()
{
    if (!ctx_.textureManager || !ctx_.srvManager || !ctx_.directXCommon) {
        return;
    }

    constexpr const char* kEnvironmentMapTextureName = "rostock_laage_airport_4k.dds"; // 環境マップ用DDS名
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
/// 終了処理を行う
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    StopCameraShake();

    sceneRenderTarget_.Finalize();
    postProcessIntermediateTarget_.Finalize();
    finalRenderTarget_.Finalize();
    dissolveMaskSrvIndex_ = UINT32_MAX;
    postProcess_.Finalize();

    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Finalize();
    }

    sprites_.clear();
    objects3d_.clear();
    temporalAfterimageSprites_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
    timeReversalEffect_.ResetState();

    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->SetParticlePlane(nullptr);
    }

    particlePlane_.reset();
    particleRing_.reset();
    particleCylinder_.reset();
    if (skybox_) {
        skybox_->Finalize();
        skybox_.reset();
    }

    temporalRiftEffect_.ResetState(ctx_.camera);
    timeStopEffect_.ResetState();
    ctx_ = {};
}

/// <summary>
/// 更新処理
/// </summary>
void PlayScene::Update(float dt)
{
    InputManager* inputManager = InputManager::GetInstance(); // 時空破砕の発動入力を取得する入力管理
    if (inputManager
        && inputManager->IsKeyJustPressed(DIK_R)
        && !IsAnyEffectPlaying()) {
        StartSelectedEffect();
    }

    const float effectDeltaTime = temporalRiftEffect_.GetHitStopRemainingTime() > 0.0f ? 0.0f : dt; // ヒットストップを反映した演出時間
    UpdateTimeReversalTransformHistory();
    UpdateTemporalRiftEffect(effectDeltaTime);
    UpdateTimeReversalEffect(effectDeltaTime);
    UpdateTimeStopEffect(dt);
    UpdateImpactResponse(dt);
    if (temporalRiftEffect_.GetHitStopRemainingTime() <= 0.0f) {
        UpdateTemporalAfterimages();
    }
    postProcess_.Update(dt);

    // カメラの更新
    if (ctx_.camera) {
        ctx_.camera->Update();
    }
    temporalRiftEffect_.SetScreenUv(CalculateTemporalRiftScreenUv());
    const Vector2 postEffectCenter = timeStopEffect_.IsPlaying()
        ? CalculateWorldScreenUv(timeStopEffect_.GetEffectPosition())
        : (timeReversalEffect_.IsPlaying()
                  ? CalculateWorldScreenUv(timeReversalEffect_.GetEffectPosition())
                  : temporalRiftEffect_.GetScreenUv()); // 再生中のエフェクトに対応する画面中心
    postProcess_.SetRadialBlurCenter(postEffectCenter);
    postProcess_.SetDistortionCenter(postEffectCenter);

    // パーティクルエミッターの更新とマネージャーの更新
    const float particleDeltaTime = (temporalRiftEffect_.GetHitStopRemainingTime() > 0.0f || IsTimeStopped()) ? 0.0f : dt; // ヒットストップを反映したパーティクル時間
    pmEmitter_.Update(particleDeltaTime);
    ringEmitter_.Update(particleDeltaTime);
    cylinderEmitter_.Update(particleDeltaTime);
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Update(particleDeltaTime);
    }

    // オブジェクトの更新
    for (auto& o : objects3d_) {
        if (o) {
            if (ctx_.camera) {
                o->Update(ctx_.camera->GetViewMatrix(), ctx_.camera->GetProjectionMatrix());
            }
        }
    }

    // スプライトの更新
    for (auto& s : sprites_) {
        if (s)
            s->Update();
    }

    UpdateAfterimageSprites();
    UpdateTimeReversalSprites();
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
/// ポストプロセス描画が利用できるか判定する
/// </summary>
bool PlayScene::CanUsePostProcess() const
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 描画に使用するDirectX基盤
    return directXCommon
        && sceneRenderTarget_.IsValid()
        && sceneRenderTarget_.HasColorSrv()
        && postProcess_.IsReady();
}

/// <summary>
/// シーン描画結果をポストプロセス入力用RTへ描画する
/// </summary>
void PlayScene::DrawSceneToPostProcessTarget()
{
    sceneRenderTarget_.Begin(true);
    DrawSceneContent();
    sceneRenderTarget_.End();
}

/// <summary>
/// 時間演出用のポストプロセス連鎖を適用する
/// </summary>
void PlayScene::ApplyTemporalPostProcessChain(uint32_t& postProcessSourceSrvIndex, PostEffectType& finalEffectType)
{
    const bool canUseTemporalChain = temporalRiftEffect_.IsPostProcessChainPhase()
        && postProcessIntermediateTarget_.IsValid()
        && postProcessIntermediateTarget_.HasColorSrv(); // 2pass目に渡すRTが必要
    if (!canUseTemporalChain) {
        return;
    }

    postProcessIntermediateTarget_.Begin(true);
    postProcess_.DrawTexture(sceneRenderTarget_, PostEffectType::Distortion);
    postProcessIntermediateTarget_.End();

    postProcessSourceSrvIndex = postProcessIntermediateTarget_.GetColorSrvIndex();
    finalEffectType = PostEffectType::RadialBlur;
}

/// <summary>
/// Gaussian Filterの2pass描画が利用できるか判定する
/// </summary>
bool PlayScene::CanUseGaussianFilter(PostEffectType finalEffectType) const
{
    return finalEffectType == PostEffectType::GaussianFilter
        && postProcessIntermediateTarget_.IsValid()
        && postProcessIntermediateTarget_.HasColorSrv();
}

/// <summary>
/// Scene View表示用の最終RTが利用できるか判定する
/// </summary>
bool PlayScene::CanUseFinalRenderTarget() const
{
    return sceneViewOnly_
        && finalRenderTarget_.IsValid()
        && finalRenderTarget_.HasColorSrv();
}

/// <summary>
/// Gaussian Filterの1pass目を中間RTへ描画する
/// </summary>
void PlayScene::ApplyGaussianFirstPass(uint32_t& postProcessSourceSrvIndex)
{
    postProcessIntermediateTarget_.Begin(true);
    postProcess_.DrawGaussianPass(postProcessSourceSrvIndex, 0);
    postProcessIntermediateTarget_.End();
    postProcessSourceSrvIndex = postProcessIntermediateTarget_.GetColorSrvIndex();
}

/// <summary>
/// 最終ポストプロセス描画を実行する
/// </summary>
void PlayScene::DrawFinalPostProcessPass(uint32_t postProcessSourceSrvIndex, PostEffectType finalEffectType, bool useGaussianFilter)
{
    if (useGaussianFilter) {
        postProcess_.DrawGaussianPass(postProcessSourceSrvIndex, 1);
    } else if (finalEffectType == PostEffectType::DepthOutline && sceneRenderTarget_.HasDepthSrv() && ctx_.camera) {
        postProcess_.DrawDepthOutline(postProcessSourceSrvIndex, sceneRenderTarget_, ctx_.camera->GetProjectionMatrix());
    } else if (finalEffectType == PostEffectType::Dissolve && dissolveMaskSrvIndex_ != UINT32_MAX) {
        postProcess_.DrawDissolveTexture(postProcessSourceSrvIndex, dissolveMaskSrvIndex_);
    } else {
        postProcess_.DrawTexture(postProcessSourceSrvIndex, finalEffectType);
    }
}
/// <summary>
/// 最終描画に必要なポストプロセス状態を作成する
/// </summary>
PlayScene::PostProcessDrawContext PlayScene::BuildPostProcessDrawContext()
{
    PostProcessDrawContext drawContext {}; // ポストプロセス描画で共有する状態
    drawContext.sourceSrvIndex = sceneRenderTarget_.GetColorSrvIndex();
    drawContext.finalEffectType = postProcess_.GetEffectType();

    ApplyPostProcessPrePasses(drawContext);
    drawContext.useFinalRenderTarget = CanUseFinalRenderTarget();
    return drawContext;
}

/// <summary>
/// 最終描画前に必要なポストプロセスの前段パスを適用する。
/// </summary>
void PlayScene::ApplyPostProcessPrePasses(PostProcessDrawContext& drawContext)
{
    ApplyTemporalPostProcessChain(drawContext.sourceSrvIndex, drawContext.finalEffectType);

    drawContext.useGaussianFilter = CanUseGaussianFilter(drawContext.finalEffectType);
    if (drawContext.useGaussianFilter) {
        ApplyGaussianFirstPass(drawContext.sourceSrvIndex);
    }
}

/// <summary>
/// Scene View用RTが必要な場合だけ描画先を切り替える
/// </summary>
void PlayScene::BeginSceneViewRenderTargetIfNeeded(bool useFinalRenderTarget)
{
    if (!useFinalRenderTarget) {
        return;
    }

    finalRenderTarget_.Begin(true);
}

/// <summary>
/// Scene View用RTへ描画していた場合だけ描画先を戻す
/// </summary>
void PlayScene::EndSceneViewRenderTargetIfNeeded(bool useFinalRenderTarget)
{
    if (!useFinalRenderTarget) {
        return;
    }

    finalRenderTarget_.End();
}

/// <summary>
/// 現在の描画先へポストプロセス結果とスプライトを描画する
/// </summary>
void PlayScene::DrawPostProcessOutputToCurrentTarget(const PostProcessDrawContext& drawContext)
{
    DrawFinalPostProcessPass(
        drawContext.sourceSrvIndex,
        drawContext.finalEffectType,
        drawContext.useGaussianFilter);
    DrawSprites();
}

/// <summary>
/// 作成済みのポストプロセス状態に従って最終結果を描画する
/// </summary>
void PlayScene::DrawPostProcessResult(const PostProcessDrawContext& drawContext)
{
    BeginSceneViewRenderTargetIfNeeded(drawContext.useFinalRenderTarget);
    DrawPostProcessOutputToCurrentTarget(drawContext);
    EndSceneViewRenderTargetIfNeeded(drawContext.useFinalRenderTarget);
}

/// <summary>
/// ポストプロセス付きでシーンを描画する
/// </summary>
bool PlayScene::DrawPostProcessedScene()
{
    if (!CanUsePostProcess()) {
        return false;
    }

    DrawSceneToPostProcessTarget();

    const PostProcessDrawContext drawContext = BuildPostProcessDrawContext(); // 最終描画に使用する状態
    DrawPostProcessResult(drawContext);
    return true;
}

/// <summary>
/// シーン内の3D要素を描画する
/// </summary>
void PlayScene::DrawSceneContent()
{
    DrawWorldAndParticles();
    DrawTemporalAfterimages();
    DrawTimeReversalParticles();
}

/// <summary>
/// 指定した番号の3Dオブジェクトを描画する。
/// </summary>
void PlayScene::DrawObject3dAtIndex(size_t objectIndex)
{
    if (objects3d_.size() <= objectIndex || !objects3d_[objectIndex]) {
        return;
    }

    objects3d_[objectIndex]->Draw();
}

/// <summary>
/// すべての3Dオブジェクトを描画する。
/// </summary>
void PlayScene::DrawAllObjects3d()
{
    for (auto& object3d : objects3d_) {
        if (object3d) {
            object3d->Draw();
        }
    }
}

/// <summary>
/// 選択中の描画種別に対応する3Dオブジェクトを描画する。
/// </summary>
void PlayScene::DrawSelectedObjects3d(int selectedDrawType)
{
    switch (selectedDrawType) {
    case 0:
        DrawObject3dAtIndex(0);
        break;
    case 3:
        DrawObject3dAtIndex(1);
        break;
    case 4:
        DrawObject3dAtIndex(3);
        break;
    case 5:
        DrawObject3dAtIndex(2);
        break;
    case 6:
        DrawObject3dAtIndex(4);
        DrawObject3dAtIndex(5);
        DrawObject3dAtIndex(6);
        break;
    default:
        break;
    }
}

/// <summary>
/// パーティクルを描画する必要があるか判定する。
/// </summary>
bool PlayScene::ShouldDrawParticles(int selectedDrawType) const
{
    return selectedDrawType == -1
        || selectedDrawType == 1
        || selectedDrawType == 7
        || IsAnyEffectPlaying();
}

/// <summary>
/// 必要な場合だけパーティクルを描画する。
/// </summary>
void PlayScene::DrawParticlesIfNeeded(int selectedDrawType)
{
    if (!ShouldDrawParticles(selectedDrawType)) {
        return;
    }

    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル描画を担当する管理クラス
    if (!particleManager) {
        return;
    }

    particleManager->Draw();
}

/// <summary>
/// 3D空間とパーティクルを描画する。
/// </summary>
void PlayScene::DrawWorldAndParticles()
{
    const int selectedDrawType = ctx_.selectedDrawType; // ImGuiで選択されている描画種別

    if (skybox_ && ctx_.camera) {
        skybox_->Draw(ctx_.camera);
    }

    if (!ctx_.object3dCommon) {
        return;
    }

    ctx_.object3dCommon->SetCommonDrawSetting();
    if (selectedDrawType == -1 || selectedDrawType == 7) {
        DrawAllObjects3d();
    } else {
        DrawSelectedObjects3d(selectedDrawType);
    }

    DrawParticlesIfNeeded(selectedDrawType);
}

/// <summary>
/// ポストプロセスの影響を受けないスプライトを描画する
/// </summary>
void PlayScene::DrawSprites()
{
    const int selectedDrawType = ctx_.selectedDrawType; // ImGuiで選択されている描画種別
    if (!ctx_.spriteCommon) {
        return;
    }

    if (selectedDrawType == -1 || selectedDrawType == 2 || selectedDrawType == 7) {
        ctx_.spriteCommon->SetCommonDrawSetting();
        for (auto& sprite : sprites_) {
            if (sprite) {
                sprite->Draw();
            }
        }
    }
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
