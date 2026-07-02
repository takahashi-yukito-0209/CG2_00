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
    DirectXCommon* directXCommon = ctx_.directXCommon; // オフスクリーン描画に使用するDirectX基盤
    if (directXCommon && ctx_.srvManager) {
        sceneRenderTargetHandle_ = directXCommon->CreateRenderTarget(
            WinApp::kWindowWidth,
            WinApp::kWindowHeight,
            directXCommon->GetSwapChainFormat(),
            true,
            { 0.53f, 0.71f, 0.82f, 1.0f },
            true);

        if (sceneRenderTargetHandle_ >= 0) {
            sceneRenderTargetSrvIndex_ = ctx_.srvManager->Allocate();
            directXCommon->CreateRenderTargetSRV(
                sceneRenderTargetHandle_,
                sceneRenderTargetSrvIndex_);

            sceneDepthSrvIndex_ = ctx_.srvManager->Allocate();
            directXCommon->CreateRenderTargetDepthSRV(
                sceneRenderTargetHandle_,
                sceneDepthSrvIndex_);
        }

        postProcessIntermediateHandle_ = directXCommon->CreateRenderTarget(
            WinApp::kWindowWidth,
            WinApp::kWindowHeight,
            directXCommon->GetSwapChainFormat(),
            false,
            { 0.0f, 0.0f, 0.0f, 1.0f },
            true);

        if (postProcessIntermediateHandle_ >= 0) {
            postProcessIntermediateSrvIndex_ = ctx_.srvManager->Allocate();
            directXCommon->CreateRenderTargetSRV(
                postProcessIntermediateHandle_,
                postProcessIntermediateSrvIndex_);
        }

        finalRenderTargetHandle_ = directXCommon->CreateRenderTarget(
            WinApp::kWindowWidth,
            WinApp::kWindowHeight,
            directXCommon->GetSwapChainFormat(),
            false,
            { 0.0f, 0.0f, 0.0f, 1.0f },
            true);

        if (finalRenderTargetHandle_ >= 0) {
            finalRenderTargetSrvIndex_ = ctx_.srvManager->Allocate();
            directXCommon->CreateRenderTargetSRV(
                finalRenderTargetHandle_,
                finalRenderTargetSrvIndex_);
        }

        if (ctx_.textureManager) {
            ctx_.textureManager->LoadTexture("noise0.png");
            dissolveMaskSrvIndex_ = ctx_.textureManager->GetSrvIndex("noise0.png");
        }

        postProcess_.Initialize(directXCommon);
        postProcess_.SetEffectType(PostEffectType::Copy);
    }
}

void PlayScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;

    // テクスチャのロード
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture("uvChecker.png");
        ctx_.textureManager->LoadTexture("monsterBall.png");
        ctx_.textureManager->LoadTexture("circle.png");
        ctx_.textureManager->LoadTexture("gradationLine.png");
        // 環境マップ用DDSを読み込む（確認用）
        ctx_.textureManager->LoadTexture("rostock_laage_airport_4k.dds");
    }

    // SkyBox 初期化（ロード済みの cubemap を使用）
    if (ctx_.textureManager && ctx_.srvManager && ctx_.directXCommon) {
        uint32_t srvIdx = ctx_.textureManager->GetSrvIndex("rostock_laage_airport_4k.dds");
        if (srvIdx != UINT32_MAX) {
            // 環境マップ用のDDSがロードされていれば、それを使用してSkyBoxを初期化
            skybox_ = std::make_unique<SkyBox>();
            skybox_->Initialize(ctx_.directXCommon, ctx_.srvManager, srvIdx);
            if (ctx_.object3dCommon) {
                ctx_.object3dCommon->SetEnvironmentMapSrvIndex(srvIdx);
            }
        }
    }

    // スプライトの作成
    const uint32_t kSpriteCount = 5;
    // 2種類のテクスチャを交互に使用してスプライトを作成
    std::array<std::string, 2> spriteNames = {
        "uvChecker",
        "monsterBall"
    };

    // 2種類のテクスチャファイル名を配列で管理
    for (uint32_t i = 0; i < kSpriteCount; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(ctx_.spriteCommon, spriteNames[(i / 2) == 0 ? 0 : 1] + ".png", ctx_.imguiManager);
        sprites_.push_back(std::move(sprite));
    }

    // モデルルファイル名の配列を作成
    std::vector<std::string> modelFileNames = {
        "plane/plane.gltf",
        "bunny/bunny.obj",
        "teapot/teapot.obj",
        "fence/fence.obj",
        "sphere/sphere.gltf",
        "terrain/terrain.obj",
        "cube/Cube.obj"
    };

    for (size_t i = 0; i < modelFileNames.size(); ++i) {
        auto obj = std::make_unique<Object3d>();
        // Object3d の初期化とモデルのセット
        obj->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        obj->SetModel(modelFileNames[i]);
        if (modelFileNames[i].find("fence") != std::string::npos) {
            obj->SetUseAlphaCutoutSampler(true);
        }
        if (modelFileNames[i].find("cube") != std::string::npos || modelFileNames[i].find("Cube") != std::string::npos) {
            obj->SetEnvironmentCoefficient(0.85f);
            obj->SetTranslate({ 3.0f, 0.0f, 0.0f });
        }
        objects3d_.push_back(std::move(obj));
    }

    objects3d_[5]->SetScale({ 5.0f, 5.0f, 5.0f }); // terrain を大きくする

    InitializeParticleObjects();
    InitializeParticleEffects();
    InitializeTemporalEffectSprites();
    InitializePostProcessTargets();
}

/// <summary>
/// 終了処理
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    StopCameraShake();

    DirectXCommon* directXCommon = ctx_.directXCommon; // 解放対象を管理するDirectX基盤
    if (directXCommon && sceneRenderTargetHandle_ >= 0) {
        directXCommon->DestroyRenderTarget(sceneRenderTargetHandle_);
        sceneRenderTargetHandle_ = -1;
        sceneRenderTargetSrvIndex_ = UINT32_MAX;
        sceneDepthSrvIndex_ = UINT32_MAX;
    }
    if (directXCommon && postProcessIntermediateHandle_ >= 0) {
        directXCommon->DestroyRenderTarget(postProcessIntermediateHandle_);
        postProcessIntermediateHandle_ = -1;
        postProcessIntermediateSrvIndex_ = UINT32_MAX;
    }
    if (directXCommon && finalRenderTargetHandle_ >= 0) {
        directXCommon->DestroyRenderTarget(finalRenderTargetHandle_);
        finalRenderTargetHandle_ = -1;
        finalRenderTargetSrvIndex_ = UINT32_MAX;
    }
    dissolveMaskSrvIndex_ = UINT32_MAX;
    postProcess_.Finalize();
    // パーティクルマネージャーの終了処理
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Finalize();
    }

    // スプライトとオブジェクトの解放
    sprites_.clear();
    objects3d_.clear();
    temporalAfterimageSprites_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
    timeReversalEffect_.ResetState();

    // パーティクル描画に使用していたプレーンを ParticleManager から解除
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->SetParticlePlane(nullptr);
    }

    // プレーンのリセット
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
/// プレイシーンを描画する
/// </summary>
void PlayScene::Draw()
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 描画先を切り替えるDirectX基盤
    const bool canUsePostProcess = directXCommon
        && sceneRenderTargetHandle_ >= 0
        && sceneRenderTargetSrvIndex_ != UINT32_MAX
        && postProcess_.IsReady(); // ポストプロセスを実行できる状態か
    const bool canUseTemporalChain = canUsePostProcess
        && temporalRiftEffect_.IsPostProcessChainPhase()
        && postProcessIntermediateHandle_ >= 0
        && postProcessIntermediateSrvIndex_ != UINT32_MAX; // 2パス連結を実行できる状態か

    if (canUsePostProcess) {
        directXCommon->BeginRenderTo(sceneRenderTargetHandle_, true);
        DrawWorldAndParticles();
        DrawTemporalAfterimages();
        DrawTimeReversalParticles();
        directXCommon->EndRenderTo(sceneRenderTargetHandle_);

        uint32_t postProcessSourceSrvIndex = sceneRenderTargetSrvIndex_; // 最終パスに入力するSRV番号
        PostEffectType finalEffectType = postProcess_.GetEffectType(); // 最終パスで適用するポストエフェクト

        if (canUseTemporalChain) {
            // 1パス目で空間歪みを中間レンダーターゲットへ描画する
            directXCommon->BeginRenderTo(postProcessIntermediateHandle_, true);
            postProcess_.DrawTexture(
                sceneRenderTargetSrvIndex_,
                PostEffectType::Distortion);
            directXCommon->EndRenderTo(postProcessIntermediateHandle_);

            postProcessSourceSrvIndex = postProcessIntermediateSrvIndex_;
            finalEffectType = PostEffectType::RadialBlur;
        }

        const bool canUseFinalRenderTarget = sceneViewOnly_
            && finalRenderTargetHandle_ >= 0
            && finalRenderTargetSrvIndex_ != UINT32_MAX; // Scene Viewへ最終結果を書き込める状態か
        const bool canUseGaussianFilter = finalEffectType == PostEffectType::GaussianFilter
            && postProcessIntermediateHandle_ >= 0
            && postProcessIntermediateSrvIndex_ != UINT32_MAX; // Gaussian Filterの2パス処理を実行できるか

        if (canUseGaussianFilter) {
            directXCommon->BeginRenderTo(postProcessIntermediateHandle_, true);
            postProcess_.DrawGaussianPass(postProcessSourceSrvIndex, 0);
            directXCommon->EndRenderTo(postProcessIntermediateHandle_);
            postProcessSourceSrvIndex = postProcessIntermediateSrvIndex_;
        }

        if (canUseFinalRenderTarget) {
            directXCommon->BeginRenderTo(finalRenderTargetHandle_, true);
        }

        if (canUseGaussianFilter) {
            postProcess_.DrawGaussianPass(postProcessSourceSrvIndex, 1);
        } else if (finalEffectType == PostEffectType::DepthOutline
            && sceneDepthSrvIndex_ != UINT32_MAX
            && ctx_.camera) {
            postProcess_.DrawDepthOutline(
                postProcessSourceSrvIndex,
                sceneDepthSrvIndex_,
                ctx_.camera->GetProjectionMatrix());
        } else if (finalEffectType == PostEffectType::Dissolve
            && dissolveMaskSrvIndex_ != UINT32_MAX) {
            postProcess_.DrawDissolveTexture(
                postProcessSourceSrvIndex,
                dissolveMaskSrvIndex_);
        } else {
            postProcess_.DrawTexture(postProcessSourceSrvIndex, finalEffectType);
        }
        DrawSprites();

        if (canUseFinalRenderTarget) {
            directXCommon->EndRenderTo(finalRenderTargetHandle_);
        }
        return;
    }

    DrawWorldAndParticles();
    DrawTemporalAfterimages();
    DrawTimeReversalParticles();
    DrawSprites();
}

/// <summary>
/// 3Dオブジェクトとパーティクルを描画する
/// </summary>
void PlayScene::DrawWorldAndParticles()
{
    // シーン側でも Game の選択モードに応じて個別描画できるようにする
    int sel = ctx_.selectedDrawType;

    // Draw SkyBox first so it appears behind other geometry (depth disabled in its PSO)
    if (skybox_ && ctx_.camera) {
        skybox_->Draw(ctx_.camera);
    }

    // オブジェクト系の描画（Model, Bunny, Fence, Checker, Sphere, All）
    if (ctx_.object3dCommon) {

        // モデル全体描画（All）または個別モデル描画
        if (sel == -1 || sel == 7) {
            ctx_.object3dCommon->SetCommonDrawSetting();
            for (auto& o : objects3d_) {
                if (o)
                    o->Draw();
            }
        } else {
            // 個別オブジェクト描画マッピング
            ctx_.object3dCommon->SetCommonDrawSetting();
            switch (sel) {
            case 0: // Model -> index 0
                if (objects3d_.size() > 0 && objects3d_[0])
                    objects3d_[0]->Draw();
                break;
            case 3: // Bunny -> index 1
                if (objects3d_.size() > 1 && objects3d_[1])
                    objects3d_[1]->Draw();
                break;
            case 4: // Fence -> index 3
                if (objects3d_.size() > 3 && objects3d_[3])
                    objects3d_[3]->Draw();
                break;
            case 5: // Checker -> index 2
                if (objects3d_.size() > 2 && objects3d_[2])
                    objects3d_[2]->Draw();
                break;
            case 6: // Sphere -> index 4 and 5 if present
                if (objects3d_.size() > 4 && objects3d_[4])
                    objects3d_[4]->Draw();
                if (objects3d_.size() > 5 && objects3d_[5])
                    objects3d_[5]->Draw();
                if (objects3d_.size() > 6 && objects3d_[6])
                    objects3d_[6]->Draw();
                break;
            default:
                // その他（Particle/Sprite）はここでは扱わない
                break;
            }
        }

        // エフェクト再生中は描画モードに関係なくパーティクルを描画する
        const bool shouldDrawParticles = sel == -1 || sel == 1 || sel == 7 || IsAnyEffectPlaying(); // パーティクル描画を行うか
        if (shouldDrawParticles) {
            if (ParticleManager::GetInstance()) {
                ParticleManager::GetInstance()->Draw();
            }
        }
    }

    // スプライト描画は Sprite モードまたは All のときに行う
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

/// <summary>
/// Scene Viewへ表示するSRV番号を取得する
/// </summary>
uint32_t PlayScene::GetSceneViewSrvIndex() const
{
    if (finalRenderTargetSrvIndex_ != UINT32_MAX) {
        return finalRenderTargetSrvIndex_;
    }
    return sceneRenderTargetSrvIndex_;
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
