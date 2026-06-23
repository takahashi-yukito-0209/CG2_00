#include "TitleScene.h"
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

/// <summary>
/// コンストラクタ
/// </summary>
TitleScene::TitleScene() { }

/// <summary>
/// デストラクタ
/// </summary>
TitleScene::~TitleScene() { }

/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;

    // テクスチャのロード
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture("circle.png");
        ctx_.textureManager->LoadTexture("gradationLine.png");
        // 読み込んだテクスチャをGPUに転送
        ctx_.textureManager->ReleaseIntermediateResources();
    }

    if (ctx_.object3dCommon) {
        auto terrain = std::make_unique<Object3d>(); // タイトル画面に表示する地形
        terrain->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        terrain->SetModel("terrain/terrain.obj");
        terrain->SetScale({ 5.0f, 5.0f, 5.0f });
        objects3d_.push_back(std::move(terrain));
    }

    // パーティクルの初期化
    particlePlane_ = std::make_unique<Object3d>();
    particlePlane_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    particlePlane_->SetMesh(PrimitiveFactory::CreatePlane());
    particlePlane_->SetTexture("circle.png");

    particleRing_ = std::make_unique<Object3d>();
    particleRing_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    particleRing_->SetMesh(PrimitiveFactory::CreateRing(1.0f, 0.2f));
    particleRing_->SetTexture("gradationLine.png");
    particleRing_->SetUseAlphaCutoutSampler(true);

    particleCylinder_ = std::make_unique<Object3d>();
    particleCylinder_->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    particleCylinder_->SetMesh(PrimitiveFactory::CreateCylinder(1.0f, 1.0f, 1.0f));
    particleCylinder_->SetTexture("gradationLine.png");
    particleCylinder_->SetUseAlphaCutoutSampler(true);

    // パーティクルマネージャー化とグループの作成
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Initialize(ctx_.directXCommon, ctx_.object3dCommon, ctx_.srvManager, ctx_.textureManager, ctx_.imguiManager);
        ParticleManager::GetInstance()->SetParticlePlane(particlePlane_.get());
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

    constexpr int kMaximumAfterimageCount = 8; // 調整UIで使用できる最大残像数
    temporalAfterimageSprites_.reserve(kMaximumAfterimageCount);
    for (int afterimageIndex = 0; afterimageIndex < kMaximumAfterimageCount; ++afterimageIndex) {
        auto afterimageSprite = std::make_unique<Sprite>(); // Transform履歴を表示する残像スプライト
        afterimageSprite->Initialize(
            ctx_.spriteCommon,
            "circle2.png",
            ctx_.imguiManager);
        afterimageSprite->SetAnchorPoint({ 0.5f, 0.5f });
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
        particleSprite->SetAnchorPoint({ 0.5f, 0.5f });
        particleSprite->SetSize({ 1.0f, 1.0f });
        particleSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        particleSprite->Update();
        timeReversalSprites_.push_back(std::move(particleSprite));
    }

    constexpr int kMaximumRewindAfterimageCount = 3; // 1粒子ごとに保持する最大残像数
    const int maximumRewindAfterimageSpriteCount =
        kMaximumTimeReversalParticleCount * kMaximumRewindAfterimageCount; // 確保する残像スプライト総数
    timeReversalAfterimageSprites_.reserve(maximumRewindAfterimageSpriteCount);
    for (int afterimageIndex = 0;
         afterimageIndex < maximumRewindAfterimageSpriteCount;
         ++afterimageIndex) {
        auto afterimageSprite = std::make_unique<Sprite>(); // 巻き戻し軌道用の残像スプライト
        afterimageSprite->Initialize(
            ctx_.spriteCommon,
            "circle2.png",
            ctx_.imguiManager);
        afterimageSprite->SetAnchorPoint({ 0.5f, 0.5f });
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
    timeReversalConvergenceSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    timeReversalConvergenceSprite_->SetSize({ 1.0f, 1.0f });
    timeReversalConvergenceSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    timeReversalConvergenceSprite_->Update();

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

        postProcess_.Initialize(directXCommon);
        postProcess_.SetEffectType(PostEffectType::Copy);
    }
}

/// <summary>
/// 終了処理
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";
    StopCameraShake();

    DirectXCommon* directXCommon = ctx_.directXCommon; // 解放対象を管理するDirectX基盤
    if (directXCommon && sceneRenderTargetHandle_ >= 0) {
        directXCommon->DestroyRenderTarget(sceneRenderTargetHandle_);
        sceneRenderTargetHandle_ = -1;
        sceneRenderTargetSrvIndex_ = UINT32_MAX;
    }
    if (directXCommon && postProcessIntermediateHandle_ >= 0) {
        directXCommon->DestroyRenderTarget(postProcessIntermediateHandle_);
        postProcessIntermediateHandle_ = -1;
        postProcessIntermediateSrvIndex_ = UINT32_MAX;
    }
    postProcess_.Finalize();
    // パーティクルマネージャーの終了処理
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Finalize();
    }

    // スプライトとオブジェクトの解放
    sprites_.clear();
    objects3d_.clear();
    temporalAfterimageSprites_.clear();
    temporalTransformHistory_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
    timeReversalParticles_.clear();
    timeReversalTransformHistory_.clear();

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

    temporalRiftPhase_ = TemporalRiftPhase::Idle;
    temporalRiftPhaseTime_ = 0.0f;
    timeStopPhase_ = TimeStopPhase::Idle;
    timeStopPhaseTime_ = 0.0f;
    ctx_ = {};
}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update(float dt)
{
    InputManager* inputManager = InputManager::GetInstance(); // 時空破砕の発動入力を取得する入力管理
    if (inputManager
        && inputManager->IsKeyJustPressed(DIK_R)
        && !IsAnyEffectPlaying()
        && IsSelectedEffectReady()) {
        StartSelectedEffect();
    }

    const float effectDeltaTime = hitStopRemainingTime_ > 0.0f ? 0.0f : dt; // ヒットストップを反映した演出時間
    UpdateTimeReversalTransformHistory();
    UpdateTemporalRiftEffect(effectDeltaTime);
    UpdateTimeReversalEffect(effectDeltaTime);
    UpdateTimeStopEffect(dt);
    UpdateImpactResponse(dt);
    if (hitStopRemainingTime_ <= 0.0f) {
        UpdateTemporalAfterimages();
    }
    postProcess_.Update(dt);

    // カメラの更新
    if (ctx_.camera) {
        ctx_.camera->Update();
    }
    temporalRiftScreenUv_ = CalculateTemporalRiftScreenUv();
    const Vector2 postEffectCenter = timeStopPhase_ != TimeStopPhase::Idle
        ? CalculateWorldScreenUv(timeStopSettings_.effectPosition)
        : (timeReversalPhase_ != TimeReversalPhase::Idle
                ? CalculateWorldScreenUv(timeReversalSettings_.effectPosition)
                : temporalRiftScreenUv_); // 再生中のエフェクトに対応する画面中心
    postProcess_.SetRadialBlurCenter(postEffectCenter);
    postProcess_.SetDistortionCenter(postEffectCenter);

    // パーティクルエミッターの更新とマネージャーの更新
    const float particleDeltaTime = (hitStopRemainingTime_ > 0.0f || IsTimeStopped()) ? 0.0f : dt; // ヒットストップを反映したパーティクル時間
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
/// 描画処理
/// </summary>
/// <summary>
/// 時空破砕エフェクトを開始する
/// </summary>
/// <summary>
/// 時空破砕エフェクトの調整UIを描画する
/// </summary>
void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
    const char* phaseName = "Idle"; // 現在の演出状態を表示する文字列
    switch (temporalRiftPhase_) {
    case TemporalRiftPhase::Idle:
        phaseName = "Idle";
        break;
    case TemporalRiftPhase::Compress:
        phaseName = "Compress";
        break;
    case TemporalRiftPhase::Freeze:
        phaseName = "Freeze";
        break;
    case TemporalRiftPhase::Crack:
        phaseName = "Crack";
        break;
    case TemporalRiftPhase::Burst:
        phaseName = "Burst";
        break;
    case TemporalRiftPhase::Recover:
        phaseName = "Recover";
        break;
    }

    ImGui::Begin("Effect Controller");

    int selectedEffectIndex = static_cast<int>(selectedEffectType_); // ImGuiで編集中のエフェクト番号
    const char* effectNames[] = {
        "Dimensional Shatter",
        "Time Reversal",
        "Time Stop",
    }; // 選択可能なエフェクト名
    if (!IsAnyEffectPlaying()
        && ImGui::Combo(
            "Effect Type",
            &selectedEffectIndex,
            effectNames,
            IM_ARRAYSIZE(effectNames))) {
        selectedEffectType_ = static_cast<EffectType>(selectedEffectIndex);
    }

    ImGui::Text("Trigger Key: R");
    ImGui::Text(
        "Implementation: %s",
        IsSelectedEffectReady() ? "Ready" : "Not Implemented");
    ImGui::Text("Phase: %s", phaseName);
    ImGui::Text("Phase Time: %.3f", temporalRiftPhaseTime_);
    ImGui::Text(
        "Blur Center UV: %.3f, %.3f",
        temporalRiftScreenUv_.x,
        temporalRiftScreenUv_.y);

    if (!IsAnyEffectPlaying() && IsSelectedEffectReady()) {
        if (ImGui::Button("Play Effect")) {
            StartSelectedEffect();
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Play Effect");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (selectedEffectType_ == EffectType::DimensionalShatter
        && ImGui::Button("Reset Settings")) {
        temporalRiftSettings_ = TemporalRiftSettings {};
        temporalRiftPosition_ = { 0.0f, 1.0f, 0.0f };
    }

    if (selectedEffectType_ == EffectType::TimeStop) {
        const char* timeStopPhaseName = "Idle"; // 現在の時間停止状態
        switch (timeStopPhase_) {
        case TimeStopPhase::Idle:
            timeStopPhaseName = "Idle";
            break;
        case TimeStopPhase::Entering:
            timeStopPhaseName = "Entering";
            break;
        case TimeStopPhase::Stopped:
            timeStopPhaseName = "Stopped";
            break;
        case TimeStopPhase::Releasing:
            timeStopPhaseName = "Releasing";
            break;
        }

        ImGui::Text("Time Stop Phase: %s", timeStopPhaseName);
        ImGui::Text("Phase Time: %.3f", timeStopPhaseTime_);
        if (ImGui::Button("Reset Time Stop Settings")) {
            timeStopSettings_ = TimeStopSettings {};
        }

        if (ImGui::CollapsingHeader(
                "Time Stop Settings",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat(
                "Enter Duration",
                &timeStopSettings_.enterDuration,
                0.01f,
                0.01f,
                3.0f,
                "%.2f sec");
            ImGui::DragFloat(
                "Stop Duration",
                &timeStopSettings_.stopDuration,
                0.05f,
                0.05f,
                10.0f,
                "%.2f sec");
            ImGui::DragFloat(
                "Release Duration",
                &timeStopSettings_.releaseDuration,
                0.01f,
                0.01f,
                3.0f,
                "%.2f sec");
            ImGui::SeparatorText("Distortion");
            ImGui::DragFloat(
                "Stop Distortion Strength",
                &timeStopSettings_.distortionStrength,
                0.001f,
                0.0f,
                0.2f,
                "%.3f");
            ImGui::DragFloat(
                "Stop Distortion Radius",
                &timeStopSettings_.distortionRadius,
                0.01f,
                0.05f,
                1.5f,
                "%.2f");
            ImGui::DragFloat(
                "Stop Distortion Waves",
                &timeStopSettings_.distortionWaveCount,
                0.1f,
                1.0f,
                12.0f,
                "%.1f");
            ImGui::SeparatorText("Particles");
            ImGui::SliderInt(
                "Start Ring Count",
                &timeStopSettings_.startRingCount,
                0,
                8);
            ImGui::SliderInt(
                "Release Ring Count",
                &timeStopSettings_.releaseRingCount,
                0,
                8);
            ImGui::SliderInt(
                "Release Fragment Count",
                &timeStopSettings_.releaseFragmentCount,
                0,
                64);
            ImGui::DragFloat3(
                "Time Stop Position",
                &timeStopSettings_.effectPosition.x,
                0.05f);
        }

        ImGui::End();
        return;
    }
    if (selectedEffectType_ == EffectType::TimeReversal) {
        const char* timeReversalPhaseName = "Idle"; // 時間逆流の現在状態
        switch (timeReversalPhase_) {
        case TimeReversalPhase::Idle:
            timeReversalPhaseName = "Idle";
            break;
        case TimeReversalPhase::Expanding:
            timeReversalPhaseName = "Expanding";
            break;
        case TimeReversalPhase::Paused:
            timeReversalPhaseName = "Paused";
            break;
        case TimeReversalPhase::Rewinding:
            timeReversalPhaseName = "Rewinding";
            break;
        case TimeReversalPhase::Converging:
            timeReversalPhaseName = "Converging";
            break;
        }
        ImGui::Text("Time Reversal Phase: %s", timeReversalPhaseName);
        ImGui::Text("Phase Time: %.3f", timeReversalPhaseTime_);

        if (ImGui::CollapsingHeader(
                "Time Reversal Settings",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt(
                "Particle Count",
                &timeReversalSettings_.particleCount,
                1,
                128);
            ImGui::DragFloat(
                "Min Speed",
                &timeReversalSettings_.minSpeed,
                0.1f,
                0.0f,
                20.0f,
                "%.1f");
            ImGui::DragFloat(
                "Max Speed",
                &timeReversalSettings_.maxSpeed,
                0.1f,
                0.0f,
                20.0f,
                "%.1f");
            ImGui::DragFloat(
                "Expansion Duration",
                &timeReversalSettings_.expansionDuration,
                0.01f,
                0.05f,
                5.0f,
                "%.2f sec");
            ImGui::DragFloat(
                "Pause Duration",
                &timeReversalSettings_.pauseDuration,
                0.01f,
                0.0f,
                5.0f,
                "%.2f sec");
            ImGui::DragFloat(
                "Rewind Duration",
                &timeReversalSettings_.rewindDuration,
                0.01f,
                0.05f,
                5.0f,
                "%.2f sec");
            ImGui::SliderInt(
                "Rewind Afterimage Count",
                &timeReversalSettings_.rewindAfterimageCount,
                0,
                3);
            ImGui::DragFloat(
                "Rewind Afterimage Spacing",
                &timeReversalSettings_.rewindAfterimageSpacing,
                0.01f,
                0.0f,
                0.5f,
                "%.2f");
            ImGui::DragFloat(
                "Rewind Afterimage Alpha",
                &timeReversalSettings_.rewindAfterimageAlpha,
                0.01f,
                0.0f,
                1.0f,
                "%.2f");
            ImGui::SeparatorText("Screen Distortion");
            ImGui::DragFloat(
                "Rewind Distortion Strength",
                &timeReversalSettings_.distortionStrength,
                0.001f,
                -0.2f,
                0.2f,
                "%.3f");
            ImGui::DragFloat(
                "Rewind Distortion Radius",
                &timeReversalSettings_.distortionRadius,
                0.01f,
                0.05f,
                1.0f,
                "%.2f");
            ImGui::DragFloat(
                "Rewind Distortion Waves",
                &timeReversalSettings_.distortionWaveCount,
                0.1f,
                1.0f,
                12.0f,
                "%.1f");
            ImGui::SeparatorText("Convergence");
            ImGui::DragFloat(
                "Convergence Duration",
                &timeReversalSettings_.convergenceDuration,
                0.01f,
                0.05f,
                2.0f,
                "%.2f sec");
            ImGui::DragFloat(
                "Convergence Flash Size",
                &timeReversalSettings_.convergenceFlashSize,
                1.0f,
                8.0f,
                512.0f,
                "%.0f");
            ImGui::DragFloat(
                "Convergence Flash Alpha",
                &timeReversalSettings_.convergenceFlashAlpha,
                0.01f,
                0.0f,
                1.0f,
                "%.2f");
            ImGui::SliderInt(
                "Convergence Ring Count",
                &timeReversalSettings_.convergenceRingCount,
                0,
                8);
            ImGui::SeparatorText("Transform Rewind");
            ImGui::DragFloat(
                "Transform History Duration",
                &timeReversalSettings_.transformHistoryDuration,
                0.1f,
                0.1f,
                10.0f,
                "%.1f sec");
            ImGui::DragFloat(
                "Particle Size",
                &timeReversalSettings_.particleSize,
                1.0f,
                2.0f,
                128.0f,
                "%.0f");
            ImGui::ColorEdit4(
                "Particle Color",
                &timeReversalSettings_.particleColor.x);
            ImGui::DragFloat3(
                "Effect Position",
                &timeReversalSettings_.effectPosition.x,
                0.05f);
        }

        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Compress Duration", &temporalRiftSettings_.compressDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
        ImGui::DragFloat("Freeze Duration", &temporalRiftSettings_.freezeDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
        ImGui::DragFloat("Crack Duration", &temporalRiftSettings_.crackDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
        ImGui::DragFloat("Burst Duration", &temporalRiftSettings_.burstDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
        ImGui::DragFloat("Recover Duration", &temporalRiftSettings_.recoverDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
    }

    if (ImGui::CollapsingHeader("Radial Blur", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Compress Blur Start", &temporalRiftSettings_.compressBlurStart, 0.001f, 0.0f, 0.1f, "%.3f");
        ImGui::DragFloat("Compress Blur End", &temporalRiftSettings_.compressBlurEnd, 0.001f, 0.0f, 0.1f, "%.3f");
        ImGui::DragFloat("Burst Blur Strength", &temporalRiftSettings_.burstBlurStrength, 0.001f, 0.0f, 0.1f, "%.3f");
        ImGui::SliderInt("Blur Sample Count", &temporalRiftSettings_.blurSampleCount, 1, 32);
    }

    if (ImGui::CollapsingHeader("Distortion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat(
            "Distortion Radius",
            &temporalRiftSettings_.distortionRadius,
            0.01f,
            0.01f,
            1.5f,
            "%.2f");
        ImGui::DragFloat(
            "Compress Distortion",
            &temporalRiftSettings_.compressDistortionStrength,
            0.001f,
            -0.1f,
            0.0f,
            "%.3f");
        ImGui::DragFloat(
            "Burst Distortion",
            &temporalRiftSettings_.burstDistortionStrength,
            0.001f,
            0.0f,
            0.1f,
            "%.3f");
        ImGui::DragFloat(
            "Distortion Wave Count",
            &temporalRiftSettings_.distortionWaveCount,
            0.1f,
            0.0f,
            12.0f,
            "%.1f");
    }

    if (ImGui::CollapsingHeader("Afterimage", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt(
            "Afterimage Count",
            &temporalRiftSettings_.afterimageCount,
            1,
            8);
        ImGui::SliderInt(
            "History Interval",
            &temporalRiftSettings_.afterimageFrameInterval,
            1,
            12);
        ImGui::DragFloat(
            "Afterimage Size",
            &temporalRiftSettings_.afterimageSize,
            1.0f,
            10.0f,
            300.0f,
            "%.0f");
        ImGui::SliderFloat(
            "Afterimage Alpha",
            &temporalRiftSettings_.afterimageAlpha,
            0.0f,
            1.0f,
            "%.2f");
        ImGui::DragFloat(
            "Temporal Displacement",
            &temporalRiftSettings_.temporalDisplacement,
            0.05f,
            0.0f,
            5.0f,
            "%.2f");
        ImGui::ColorEdit3(
            "Afterimage Color",
            &temporalRiftSettings_.afterimageColor.x);
    }

    if (ImGui::CollapsingHeader("Impact", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat(
            "Hit Stop Duration",
            &temporalRiftSettings_.hitStopDuration,
            0.005f,
            0.0f,
            0.5f,
            "%.3f sec");
        ImGui::DragFloat(
            "Camera Shake Duration",
            &temporalRiftSettings_.cameraShakeDuration,
            0.01f,
            0.0f,
            2.0f,
            "%.2f sec");
        ImGui::DragFloat(
            "Camera Shake Strength",
            &temporalRiftSettings_.cameraShakeStrength,
            0.01f,
            0.0f,
            2.0f,
            "%.2f");
        ImGui::DragFloat(
            "Camera Shake Frequency",
            &temporalRiftSettings_.cameraShakeFrequency,
            1.0f,
            1.0f,
            120.0f,
            "%.0f");
        ImGui::Text("Hit Stop Remaining: %.3f", hitStopRemainingTime_);
        ImGui::Text("Shake Remaining: %.3f", cameraShakeRemainingTime_);
    }

    if (ImGui::CollapsingHeader("Crack", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Effect Position", &temporalRiftPosition_.x, 0.05f);
        ImGui::SliderInt("Crack Count", &temporalRiftSettings_.crackCount, 1, 7);
        ImGui::DragFloat("Crack Length", &temporalRiftSettings_.crackLength, 0.05f, 0.05f, 10.0f);
        ImGui::DragFloat("Length Variation", &temporalRiftSettings_.crackLengthVariation, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloat("Crack Width", &temporalRiftSettings_.crackWidth, 0.005f, 0.005f, 1.0f);
        ImGui::DragFloat("Width Variation", &temporalRiftSettings_.crackWidthVariation, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Crack Life Time", &temporalRiftSettings_.crackLifeTime, 0.01f, 0.01f, 5.0f, "%.2f sec");
        ImGui::ColorEdit4("Crack Color", &temporalRiftSettings_.crackColor.x);
    }

    if (ImGui::CollapsingHeader("Burst", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Ring Count", &temporalRiftSettings_.ringCount, 0, 8);
        ImGui::SliderInt("Fragment Count", &temporalRiftSettings_.fragmentCount, 0, 64);
        ImGui::ColorEdit4("Inner Ring Color", &temporalRiftSettings_.innerRingColor.x);
        ImGui::ColorEdit4("Outer Ring Color", &temporalRiftSettings_.outerRingColor.x);
        ImGui::ColorEdit4("Fragment Color", &temporalRiftSettings_.fragmentColor.x);
        ImGui::DragFloat("Fragment Min Speed", &temporalRiftSettings_.fragmentMinSpeed, 0.1f, 0.0f, 20.0f, "%.1f");
        ImGui::DragFloat("Fragment Max Speed", &temporalRiftSettings_.fragmentMaxSpeed, 0.1f, 0.0f, 20.0f, "%.1f");
        ImGui::DragFloat("Fragment Life Time", &temporalRiftSettings_.fragmentLifeTime, 0.01f, 0.05f, 3.0f, "%.2f sec");
    }

    ImGui::End();
#endif
}

/// <summary>
/// 時空破砕エフェクトを開始する
/// </summary>
/// <summary>
/// ImGuiで選択中のエフェクトを開始する
/// </summary>
void TitleScene::StartSelectedEffect()
{
    switch (selectedEffectType_) {
    case EffectType::DimensionalShatter:
        StartTemporalRiftEffect();
        break;
    case EffectType::TimeReversal:
        StartTimeReversalEffect();
        break;
    case EffectType::TimeStop:
        StartTimeStopEffect();
        break;
    }
}

/// <summary>
/// 選択中のエフェクトが実装済みか確認する
/// </summary>
bool TitleScene::IsSelectedEffectReady() const
{
    switch (selectedEffectType_) {
    case EffectType::DimensionalShatter:
        return true;
    case EffectType::TimeReversal:
        return true;
    case EffectType::TimeStop:
        return true;
    }
    return false;
}

/// <summary>
/// いずれかのエフェクトが再生中か確認する
/// </summary>
bool TitleScene::IsAnyEffectPlaying() const
{
    return temporalRiftPhase_ != TemporalRiftPhase::Idle
        || timeReversalPhase_ != TimeReversalPhase::Idle
        || timeStopPhase_ != TimeStopPhase::Idle;
}

/// <summary>
/// 次元破砕エフェクトを開始する
/// </summary>
/// <summary>
/// 時間停止エフェクトを開始する
/// </summary>
void TitleScene::StartTimeStopEffect()
{
    timeStopPreviousPostEffect_ = postProcess_.GetEffectType(); // 再生前のポストエフェクト
    timeStopPhase_ = TimeStopPhase::Entering;
    timeStopPhaseTime_ = 0.0f;

    const Vector2 effectCenter = CalculateWorldScreenUv(timeStopSettings_.effectPosition); // 時間停止の画面中心
    postProcess_.SetDistortionCenter(effectCenter);
    postProcess_.SetRadialBlurCenter(effectCenter);
    postProcess_.SetDistortionRadius(timeStopSettings_.distortionRadius);
    postProcess_.SetDistortionWaveCount(timeStopSettings_.distortionWaveCount);
    postProcess_.SetDistortionStrength(0.0f);
    postProcess_.SetDistortionProgress(0.0f);
    postProcess_.SetEffectType(PostEffectType::Distortion);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 開始演出を生成するパーティクル管理
    if (particleManager) {
        particleManager->EmitCylinderEffect("Cylinder", timeStopSettings_.effectPosition, 1);
    }
}

/// <summary>
/// 時間停止エフェクトの状態を更新する
/// </summary>
void TitleScene::UpdateTimeStopEffect(float deltaTime)
{
    if (timeStopPhase_ == TimeStopPhase::Idle) {
        return;
    }

    timeStopPhaseTime_ += deltaTime;
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 時間停止演出を生成するパーティクル管理

    switch (timeStopPhase_) {
    case TimeStopPhase::Idle:
        break;

    case TimeStopPhase::Entering: {
        const float duration = (std::max)(timeStopSettings_.enterDuration, 0.0001f); // 開始演出時間
        const float progress = (std::clamp)(timeStopPhaseTime_ / duration, 0.0f, 1.0f); // 開始演出の進行率
        postProcess_.SetEffectType(PostEffectType::Distortion);
        postProcess_.SetDistortionStrength(-timeStopSettings_.distortionStrength * progress);
        postProcess_.SetDistortionProgress(progress);

        if (progress >= 1.0f) {
            timeStopPhase_ = TimeStopPhase::Stopped;
            timeStopPhaseTime_ = 0.0f;
            postProcess_.SetEffectType(PostEffectType::Grayscale);
            postProcess_.SetDistortionStrength(0.0f);
            if (particleManager) {
                particleManager->EmitRingEffect("Ring", timeStopSettings_.effectPosition, static_cast<uint32_t>((std::max)(timeStopSettings_.startRingCount, 0)));
            }
        }
        break;
    }

    case TimeStopPhase::Stopped:
        postProcess_.SetEffectType(PostEffectType::Grayscale);
        if (timeStopPhaseTime_ >= timeStopSettings_.stopDuration) {
            timeStopPhase_ = TimeStopPhase::Releasing;
            timeStopPhaseTime_ = 0.0f;
            if (particleManager) {
                particleManager->EmitRingEffect("Ring", timeStopSettings_.effectPosition, static_cast<uint32_t>((std::max)(timeStopSettings_.releaseRingCount, 0)));
                particleManager->EmitHitEffect("Hit", timeStopSettings_.effectPosition, static_cast<uint32_t>((std::max)(timeStopSettings_.releaseFragmentCount, 0)));
            }
        }
        break;

    case TimeStopPhase::Releasing: {
        const float duration = (std::max)(timeStopSettings_.releaseDuration, 0.0001f); // 再開演出時間
        const float progress = (std::clamp)(timeStopPhaseTime_ / duration, 0.0f, 1.0f); // 再開演出の進行率
        postProcess_.SetEffectType(PostEffectType::Distortion);
        postProcess_.SetDistortionStrength(timeStopSettings_.distortionStrength * (1.0f - progress));
        postProcess_.SetDistortionProgress(progress);

        if (progress >= 1.0f) {
            timeStopPhase_ = TimeStopPhase::Idle;
            timeStopPhaseTime_ = 0.0f;
            postProcess_.SetDistortionStrength(0.0f);
            postProcess_.SetEffectType(timeStopPreviousPostEffect_);
        }
        break;
    }
    }
}

/// <summary>
/// 時間停止中か確認する
/// </summary>
bool TitleScene::IsTimeStopped() const
{
    return timeStopPhase_ == TimeStopPhase::Stopped;
}
/// <summary>
/// 時間逆流エフェクトを開始する
/// </summary>
void TitleScene::StartTimeReversalEffect()
{
    timeReversalPhase_ = TimeReversalPhase::Expanding;
    timeReversalPhaseTime_ = 0.0f;
    timeReversalParticles_.clear();

    const int particleCount = (std::clamp)(
        timeReversalSettings_.particleCount,
        1,
        static_cast<int>(timeReversalSprites_.size())); // 実際に生成する粒子数
    const float minimumSpeed = (std::min)(
        timeReversalSettings_.minSpeed,
        timeReversalSettings_.maxSpeed); // 生成速度の最小値
    const float maximumSpeed = (std::max)(
        timeReversalSettings_.minSpeed,
        timeReversalSettings_.maxSpeed); // 生成速度の最大値
    std::uniform_real_distribution<float> directionDistribution(-1.0f, 1.0f); // 方向成分の乱数
    std::uniform_real_distribution<float> speedDistribution(
        minimumSpeed,
        maximumSpeed); // 拡散速度の乱数

    timeReversalParticles_.reserve(static_cast<size_t>(particleCount));
    for (int particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
        Vector3 direction = {
            directionDistribution(timeReversalRandom_),
            directionDistribution(timeReversalRandom_),
            directionDistribution(timeReversalRandom_) * 0.45f,
        }; // 放射状に飛ばすための仮方向
        direction = MathUtil::Normalize(direction);
        const float speed = speedDistribution(timeReversalRandom_); // 現在の粒子へ与える速度

        TimeReversalParticle particle {}; // 生成する時間逆流専用パーティクル
        particle.position = timeReversalSettings_.effectPosition;
        particle.velocity = {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed,
        };
        particle.rewindStartPosition = particle.position;
        particle.elapsedTime = 0.0f;
        particle.lifeTime = timeReversalSettings_.expansionDuration;
        timeReversalParticles_.push_back(particle);
    }

    postProcess_.SetEffectType(PostEffectType::Copy);
    postProcess_.SetDistortionRadius(timeReversalSettings_.distortionRadius);
    postProcess_.SetDistortionWaveCount(timeReversalSettings_.distortionWaveCount);
    postProcess_.SetDistortionStrength(0.0f);
    postProcess_.SetDistortionProgress(0.0f);
}

/// <summary>
/// 時間逆流専用パーティクルを更新する
/// </summary>
void TitleScene::UpdateTimeReversalEffect(float deltaTime)
{
    if (timeReversalPhase_ == TimeReversalPhase::Idle) {
        return;
    }

    timeReversalPhaseTime_ += deltaTime;

    if (timeReversalPhase_ == TimeReversalPhase::Paused) {
        if (timeReversalPhaseTime_ >= timeReversalSettings_.pauseDuration) {
            timeReversalPhase_ = TimeReversalPhase::Rewinding;
            timeReversalPhaseTime_ = 0.0f;
            for (TimeReversalParticle& particle : timeReversalParticles_) {
                particle.rewindStartPosition = particle.position;
            }
        }
        return;
    }

    if (timeReversalPhase_ == TimeReversalPhase::Rewinding) {
        const float rewindDuration = (std::max)(
            timeReversalSettings_.rewindDuration,
            0.0001f); // ゼロ除算を防ぐ巻き戻し時間
        const float rewindRate = (std::clamp)(
            timeReversalPhaseTime_ / rewindDuration,
            0.0f,
            1.0f); // 巻き戻しフェーズの進行率

        for (TimeReversalParticle& particle : timeReversalParticles_) {
            particle.position = {
                particle.rewindStartPosition.x
                    + (timeReversalSettings_.effectPosition.x - particle.rewindStartPosition.x) * rewindRate,
                particle.rewindStartPosition.y
                    + (timeReversalSettings_.effectPosition.y - particle.rewindStartPosition.y) * rewindRate,
                particle.rewindStartPosition.z
                    + (timeReversalSettings_.effectPosition.z - particle.rewindStartPosition.z) * rewindRate,
            };
            particle.elapsedTime = particle.lifeTime * (1.0f - rewindRate);
        }
        postProcess_.SetEffectType(PostEffectType::Distortion);
        postProcess_.SetDistortionRadius(timeReversalSettings_.distortionRadius);
        postProcess_.SetDistortionWaveCount(timeReversalSettings_.distortionWaveCount);
        postProcess_.SetDistortionStrength(
            timeReversalSettings_.distortionStrength * (1.0f - rewindRate * 0.35f));
        postProcess_.SetDistortionProgress(rewindRate);
        ApplyTimeReversalTransform();

        if (rewindRate >= 1.0f) {
            timeReversalParticles_.clear();
            StartTimeReversalConvergence();
        }
        return;
    }

    if (timeReversalPhase_ == TimeReversalPhase::Converging) {
        const float convergenceDuration = (std::max)(
            timeReversalSettings_.convergenceDuration,
            0.0001f); // ゼロ除算を防ぐ収束演出時間
        const float convergenceRate = (std::clamp)(
            timeReversalPhaseTime_ / convergenceDuration,
            0.0f,
            1.0f); // 収束演出の進行率
        postProcess_.SetEffectType(PostEffectType::Distortion);
        postProcess_.SetDistortionRadius(timeReversalSettings_.distortionRadius);
        postProcess_.SetDistortionWaveCount(timeReversalSettings_.distortionWaveCount);
        postProcess_.SetDistortionStrength(
            -timeReversalSettings_.distortionStrength * (1.0f - convergenceRate));
        postProcess_.SetDistortionProgress(convergenceRate);

        if (convergenceRate >= 1.0f) {
            timeReversalPhase_ = TimeReversalPhase::Idle;
            timeReversalPhaseTime_ = 0.0f;
            timeReversalTransformHistory_.clear();
            postProcess_.SetEffectType(PostEffectType::Copy);
            postProcess_.SetDistortionStrength(0.0f);
        }
        return;
    }

    bool hasActiveParticle = false; // 拡散中の粒子が残っているか
    for (TimeReversalParticle& particle : timeReversalParticles_) {
        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
        particle.elapsedTime += deltaTime;
        if (particle.elapsedTime < particle.lifeTime) {
            hasActiveParticle = true;
        }
    }

    if (!hasActiveParticle) {
        timeReversalPhase_ = TimeReversalPhase::Paused;
        timeReversalPhaseTime_ = 0.0f;
    }
}

/// <summary>
/// 時間逆流専用パーティクルのスプライトを更新する
/// </summary>
void TitleScene::UpdateTimeReversalSprites()
{
    for (size_t spriteIndex = 0; spriteIndex < timeReversalSprites_.size(); ++spriteIndex) {
        Sprite* particleSprite = timeReversalSprites_[spriteIndex].get(); // 更新対象の粒子スプライト
        if (!particleSprite) {
            continue;
        }

        if (spriteIndex >= timeReversalParticles_.size()
            || timeReversalPhase_ == TimeReversalPhase::Idle) {
            particleSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            particleSprite->Update();
            continue;
        }

        const TimeReversalParticle& particle = timeReversalParticles_[spriteIndex]; // 表示する専用パーティクル
        const Vector2 screenUv = CalculateWorldScreenUv(particle.position); // 粒子の画面UV
        const float lifeRate = particle.lifeTime > 0.0f
            ? (std::clamp)(particle.elapsedTime / particle.lifeTime, 0.0f, 1.0f)
            : 1.0f; // 拡散フェーズの進行率
        const float size = timeReversalSettings_.particleSize
            * (1.0f - lifeRate * 0.35f); // 時間経過で少し縮小した粒子サイズ

        particleSprite->SetPosition({
            screenUv.x * static_cast<float>(WinApp::kWindowWidth),
            screenUv.y * static_cast<float>(WinApp::kWindowHeight),
        });
        particleSprite->SetSize({ size, size });
        particleSprite->SetColor({
            timeReversalSettings_.particleColor.x,
            timeReversalSettings_.particleColor.y,
            timeReversalSettings_.particleColor.z,
            timeReversalSettings_.particleColor.w * (1.0f - lifeRate * 0.5f),
        });
        particleSprite->Update();
    }

    constexpr size_t kMaximumRewindAfterimageCount = 3; // 1粒子ごとの最大残像数
    const size_t afterimageCount = static_cast<size_t>((std::clamp)(
        timeReversalSettings_.rewindAfterimageCount,
        0,
        static_cast<int>(kMaximumRewindAfterimageCount))); // 実際に表示する残像数
    const float rewindDuration = (std::max)(
        timeReversalSettings_.rewindDuration,
        0.0001f); // 残像位置の計算に使用する巻き戻し時間
    const float rewindRate = (std::clamp)(
        timeReversalPhaseTime_ / rewindDuration,
        0.0f,
        1.0f); // 巻き戻しの進行率

    for (size_t spriteIndex = 0;
         spriteIndex < timeReversalAfterimageSprites_.size();
         ++spriteIndex) {
        Sprite* afterimageSprite =
            timeReversalAfterimageSprites_[spriteIndex].get(); // 更新対象の残像スプライト
        if (!afterimageSprite) {
            continue;
        }

        const size_t particleIndex =
            spriteIndex / kMaximumRewindAfterimageCount; // 対応する粒子番号
        const size_t afterimageIndex =
            spriteIndex % kMaximumRewindAfterimageCount; // 粒子内での残像番号
        if (timeReversalPhase_ != TimeReversalPhase::Rewinding
            || particleIndex >= timeReversalParticles_.size()
            || afterimageIndex >= afterimageCount) {
            afterimageSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            afterimageSprite->Update();
            continue;
        }

        const TimeReversalParticle& particle =
            timeReversalParticles_[particleIndex]; // 残像元の粒子
        const float trailOffset = timeReversalSettings_.rewindAfterimageSpacing
            * static_cast<float>(afterimageIndex + 1)
            * (1.0f - rewindRate); // 終点で収束する軌道上の遅れ
        const float afterimageRate = (std::clamp)(
            rewindRate - trailOffset,
            0.0f,
            1.0f); // 残像位置の巻き戻し進行率
        const Vector3 afterimagePosition = {
            particle.rewindStartPosition.x
                + (timeReversalSettings_.effectPosition.x - particle.rewindStartPosition.x)
                    * afterimageRate,
            particle.rewindStartPosition.y
                + (timeReversalSettings_.effectPosition.y - particle.rewindStartPosition.y)
                    * afterimageRate,
            particle.rewindStartPosition.z
                + (timeReversalSettings_.effectPosition.z - particle.rewindStartPosition.z)
                    * afterimageRate,
        }; // 軌道上の残像ワールド座標
        const Vector2 screenUv =
            CalculateWorldScreenUv(afterimagePosition); // 残像の画面UV
        const float alphaRate = 1.0f
            - static_cast<float>(afterimageIndex)
                / static_cast<float>((std::max)(afterimageCount, size_t { 1 })); // 後方ほど薄くする係数
        const float sizeRate = 0.85f
            - static_cast<float>(afterimageIndex) * 0.12f; // 後方ほど小さくする係数

        afterimageSprite->SetPosition({
            screenUv.x * static_cast<float>(WinApp::kWindowWidth),
            screenUv.y * static_cast<float>(WinApp::kWindowHeight),
        });
        afterimageSprite->SetSize({
            timeReversalSettings_.particleSize * sizeRate,
            timeReversalSettings_.particleSize * sizeRate,
        });
        afterimageSprite->SetColor({
            timeReversalSettings_.particleColor.x,
            timeReversalSettings_.particleColor.y,
            timeReversalSettings_.particleColor.z,
            timeReversalSettings_.particleColor.w
                * timeReversalSettings_.rewindAfterimageAlpha * alphaRate,
        });
        afterimageSprite->Update();
    }

    if (timeReversalConvergenceSprite_) {
        if (timeReversalPhase_ != TimeReversalPhase::Converging) {
            timeReversalConvergenceSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            timeReversalConvergenceSprite_->Update();
        } else {
            const float convergenceDuration = (std::max)(
                timeReversalSettings_.convergenceDuration,
                0.0001f); // フラッシュ計算に使用する収束時間
            const float convergenceRate = (std::clamp)(
                timeReversalPhaseTime_ / convergenceDuration,
                0.0f,
                1.0f); // 収束フラッシュの進行率
            const Vector2 screenUv = CalculateWorldScreenUv(
                timeReversalSettings_.effectPosition); // 収束地点の画面UV
            const float flashSize = timeReversalSettings_.convergenceFlashSize
                * (0.25f + convergenceRate * 0.75f); // 中心から広がるフラッシュサイズ
            const float flashAlpha = timeReversalSettings_.convergenceFlashAlpha
                * (1.0f - convergenceRate); // 終了へ向けて減衰する透明度

            timeReversalConvergenceSprite_->SetPosition({
                screenUv.x * static_cast<float>(WinApp::kWindowWidth),
                screenUv.y * static_cast<float>(WinApp::kWindowHeight),
            });
            timeReversalConvergenceSprite_->SetSize({ flashSize, flashSize });
            timeReversalConvergenceSprite_->SetColor({
                0.75f,
                0.92f,
                1.0f,
                flashAlpha,
            });
            timeReversalConvergenceSprite_->Update();
        }
    }
}

/// <summary>
/// 時間巻き戻し対象のTransform履歴を更新する
/// </summary>
void TitleScene::UpdateTimeReversalTransformHistory()
{
    constexpr size_t kTimeReversalTargetIndex = 0; // Transformを巻き戻す地形の番号
    constexpr float kHistorySampleRate = 60.0f; // 履歴時間をフレーム数へ変換する基準値

    if (timeReversalPhase_ != TimeReversalPhase::Idle
        || temporalRiftPhase_ != TemporalRiftPhase::Idle
        || objects3d_.size() <= kTimeReversalTargetIndex
        || !objects3d_[kTimeReversalTargetIndex]) {
        return;
    }

    Object3d* timeReversalTarget =
        objects3d_[kTimeReversalTargetIndex].get(); // Transform履歴を保存する対象
    Transform currentTransform {}; // 現在フレームのTransform
    currentTransform.scale = timeReversalTarget->GetScale();
    currentTransform.rotate = timeReversalTarget->GetRotate();
    currentTransform.translate = timeReversalTarget->GetTranslate();
    timeReversalTransformHistory_.push_front(currentTransform);

    const size_t maximumHistoryCount = static_cast<size_t>((std::max)(
        timeReversalSettings_.transformHistoryDuration * kHistorySampleRate,
        2.0f)); // 保持する最大Transform数
    while (timeReversalTransformHistory_.size() > maximumHistoryCount) {
        timeReversalTransformHistory_.pop_back();
    }
}

/// <summary>
/// 保存したTransform履歴を対象オブジェクトへ逆順に適用する
/// </summary>
void TitleScene::ApplyTimeReversalTransform()
{
    constexpr size_t kTimeReversalTargetIndex = 0; // Transformを巻き戻す地形の番号
    if (objects3d_.size() <= kTimeReversalTargetIndex
        || !objects3d_[kTimeReversalTargetIndex]
        || timeReversalTransformHistory_.empty()) {
        return;
    }

    const float rewindDuration = (std::max)(
        timeReversalSettings_.rewindDuration,
        0.0001f); // Transform巻き戻しに使用する時間
    const float rewindRate = (std::clamp)(
        timeReversalPhaseTime_ / rewindDuration,
        0.0f,
        1.0f); // Transform巻き戻しの進行率
    const float historyPosition = rewindRate
        * static_cast<float>(timeReversalTransformHistory_.size() - 1); // 履歴内の参照位置
    const size_t currentHistoryIndex =
        static_cast<size_t>(historyPosition); // 補間元の履歴番号
    const size_t nextHistoryIndex = (std::min)(
        currentHistoryIndex + 1,
        timeReversalTransformHistory_.size() - 1); // 補間先の履歴番号
    const float interpolationRate =
        historyPosition - static_cast<float>(currentHistoryIndex); // 履歴間の補間率
    const Transform& currentTransform =
        timeReversalTransformHistory_[currentHistoryIndex]; // 補間元Transform
    const Transform& nextTransform =
        timeReversalTransformHistory_[nextHistoryIndex]; // 補間先Transform

    const auto interpolateVector3 = [interpolationRate](
                                        const Vector3& start,
                                        const Vector3& end) {
        return Vector3 {
            start.x + (end.x - start.x) * interpolationRate,
            start.y + (end.y - start.y) * interpolationRate,
            start.z + (end.z - start.z) * interpolationRate,
        };
    }; // Transform要素を線形補間する処理

    Object3d* timeReversalTarget =
        objects3d_[kTimeReversalTargetIndex].get(); // Transformを適用する対象
    timeReversalTarget->SetScale(
        interpolateVector3(currentTransform.scale, nextTransform.scale));
    timeReversalTarget->SetRotate(
        interpolateVector3(currentTransform.rotate, nextTransform.rotate));
    timeReversalTarget->SetTranslate(
        interpolateVector3(currentTransform.translate, nextTransform.translate));
}

/// <summary>
/// 時間巻き戻し完了時の収束演出を開始する
/// </summary>
void TitleScene::StartTimeReversalConvergence()
{
    timeReversalPhase_ = TimeReversalPhase::Converging;
    timeReversalPhaseTime_ = 0.0f;

    ParticleManager* particleManager =
        ParticleManager::GetInstance(); // 収束リングを生成するパーティクル管理
    if (particleManager && timeReversalSettings_.convergenceRingCount > 0) {
        particleManager->EmitRingEffect(
            "Ring",
            timeReversalSettings_.effectPosition,
            static_cast<uint32_t>(timeReversalSettings_.convergenceRingCount));
    }
}

/// <summary>
/// 時間逆流専用パーティクルを描画する
/// </summary>
void TitleScene::DrawTimeReversalParticles()
{
    if (!ctx_.spriteCommon || timeReversalPhase_ == TimeReversalPhase::Idle) {
        return;
    }

    ctx_.spriteCommon->SetCommonDrawSetting();
    for (auto& afterimageSprite : timeReversalAfterimageSprites_) {
        if (afterimageSprite && afterimageSprite->GetColor().w > 0.0f) {
            afterimageSprite->Draw();
        }
    }
    for (auto& particleSprite : timeReversalSprites_) {
        if (particleSprite && particleSprite->GetColor().w > 0.0f) {
            particleSprite->Draw();
        }
    }
    if (timeReversalConvergenceSprite_
        && timeReversalConvergenceSprite_->GetColor().w > 0.0f) {
        timeReversalConvergenceSprite_->Draw();
    }
}

/// <summary>
/// 次元破砕エフェクトを開始する
/// </summary>
void TitleScene::StartTemporalRiftEffect()
{
    constexpr size_t kTemporalTargetIndex = 0; // 時間ずれと残像の対象にする地形の番号
    if (objects3d_.size() > kTemporalTargetIndex && objects3d_[kTemporalTargetIndex]) {
        Object3d* temporalTarget = objects3d_[kTemporalTargetIndex].get(); // 時間ずれ対象の3Dオブジェクト
        temporalTargetBaseTransform_.scale = temporalTarget->GetScale();
        temporalTargetBaseTransform_.rotate = temporalTarget->GetRotate();
        temporalTargetBaseTransform_.translate = temporalTarget->GetTranslate();
        hasTemporalTargetBaseTransform_ = true;
        temporalTransformHistory_.clear();
    }

    temporalRiftPhase_ = TemporalRiftPhase::Compress;
    temporalRiftPhaseTime_ = 0.0f;
    temporalCracksEmitted_ = 0;
    postProcess_.SetEffectType(PostEffectType::Distortion);
    postProcess_.SetRadialBlurCenter(temporalRiftScreenUv_);
    postProcess_.SetDistortionCenter(temporalRiftScreenUv_);
    postProcess_.SetDistortionRadius(temporalRiftSettings_.distortionRadius);
    postProcess_.SetDistortionWaveCount(temporalRiftSettings_.distortionWaveCount);
    postProcess_.SetDistortionStrength(0.0f);
    postProcess_.SetDistortionProgress(0.0f);
    postProcess_.SetRadialBlurWidth(temporalRiftSettings_.compressBlurStart);
    postProcess_.SetRadialBlurSampleCount(
        static_cast<uint32_t>(temporalRiftSettings_.blurSampleCount));

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 圧縮予兆を生成するパーティクル管理
    if (particleManager) {
        particleManager->EmitCylinderEffect("Cylinder", temporalRiftPosition_, 1);
    }
}

/// <summary>
/// 時空破砕エフェクトの進行状態を更新する
/// </summary>
void TitleScene::UpdateTemporalRiftEffect(float deltaTime)
{
    temporalRiftPhaseTime_ += deltaTime;

    switch (temporalRiftPhase_) {
    case TemporalRiftPhase::Idle:
        // エフェクト未再生時はImGuiで選択したポストエフェクトを維持する
        break;

    case TemporalRiftPhase::Compress: {
        const float compressDuration = temporalRiftSettings_.compressDuration; // 空間圧縮を見せる時間
        const float progress = (std::min)(temporalRiftPhaseTime_ / compressDuration, 1.0f); // 圧縮演出の進行率
        const float blurWidth = temporalRiftSettings_.compressBlurStart
            + (temporalRiftSettings_.compressBlurEnd - temporalRiftSettings_.compressBlurStart) * progress; // 現在のブラー幅
        postProcess_.SetEffectType(PostEffectType::Distortion);
        postProcess_.SetDistortionRadius(temporalRiftSettings_.distortionRadius);
        postProcess_.SetDistortionWaveCount(temporalRiftSettings_.distortionWaveCount);
        postProcess_.SetDistortionStrength(
            temporalRiftSettings_.compressDistortionStrength * progress);
        postProcess_.SetDistortionProgress(progress);
        postProcess_.SetRadialBlurWidth(blurWidth);

        if (temporalRiftPhaseTime_ >= compressDuration) {
            temporalRiftPhase_ = TemporalRiftPhase::Freeze;
            temporalRiftPhaseTime_ = 0.0f;
        }
        break;
    }

    case TemporalRiftPhase::Freeze:
        postProcess_.SetEffectType(PostEffectType::Grayscale);
        if (temporalRiftPhaseTime_ >= temporalRiftSettings_.freezeDuration) {
            temporalRiftPhase_ = TemporalRiftPhase::Crack;
            temporalRiftPhaseTime_ = 0.0f;
        }
        break;

    case TemporalRiftPhase::Crack:
        postProcess_.SetEffectType(PostEffectType::Grayscale);
        EmitSpaceCracks();
        if (temporalRiftPhaseTime_ >= temporalRiftSettings_.crackDuration) {
            temporalRiftPhase_ = TemporalRiftPhase::Burst;
            temporalRiftPhaseTime_ = 0.0f;
            EmitRiftBurst();
            StartImpactResponse();
        }
        break;

    case TemporalRiftPhase::Burst: {
        const float burstDuration = temporalRiftSettings_.burstDuration; // 破砕の衝撃を見せる時間
        const float progress = (std::min)(temporalRiftPhaseTime_ / burstDuration, 1.0f); // 破砕演出の進行率
        postProcess_.SetEffectType(PostEffectType::Distortion);
        postProcess_.SetDistortionRadius(temporalRiftSettings_.distortionRadius);
        postProcess_.SetDistortionWaveCount(temporalRiftSettings_.distortionWaveCount);
        postProcess_.SetDistortionStrength(
            temporalRiftSettings_.burstDistortionStrength * (1.0f - progress));
        postProcess_.SetDistortionProgress(progress);
        postProcess_.SetRadialBlurWidth(
            temporalRiftSettings_.burstBlurStrength * (1.0f - progress));

        if (temporalRiftPhaseTime_ >= burstDuration) {
            temporalRiftPhase_ = TemporalRiftPhase::Recover;
            temporalRiftPhaseTime_ = 0.0f;
        }
        break;
    }

    case TemporalRiftPhase::Recover:
        postProcess_.SetEffectType(PostEffectType::Copy);
        if (temporalRiftPhaseTime_ >= temporalRiftSettings_.recoverDuration) {
            temporalRiftPhase_ = TemporalRiftPhase::Idle;
            temporalRiftPhaseTime_ = 0.0f;
        }
        break;
    }
}

/// <summary>
/// 空間亀裂を複数生成する
/// </summary>
/// <summary>
/// 時空破砕のワールド座標を画面UV座標へ変換する
/// </summary>
Math::Vector2 TitleScene::CalculateTemporalRiftScreenUv() const
{
    constexpr Vector2 kScreenCenterUv = { 0.5f, 0.5f }; // 変換できない場合に使用する画面中央
    constexpr float kMinimumClipW = 0.0001f; // カメラ背面とゼロ除算を判定する最小値

    if (!ctx_.camera) {
        return kScreenCenterUv;
    }

    const Matrix4x4 viewProjection = MathUtil::Multiply(
        ctx_.camera->GetViewMatrix(),
        ctx_.camera->GetProjectionMatrix()); // ワールド座標をクリップ座標へ変換する行列
    const float clipX = temporalRiftPosition_.x * viewProjection.m[0][0]
        + temporalRiftPosition_.y * viewProjection.m[1][0]
        + temporalRiftPosition_.z * viewProjection.m[2][0]
        + viewProjection.m[3][0]; // クリップ座標のX成分
    const float clipY = temporalRiftPosition_.x * viewProjection.m[0][1]
        + temporalRiftPosition_.y * viewProjection.m[1][1]
        + temporalRiftPosition_.z * viewProjection.m[2][1]
        + viewProjection.m[3][1]; // クリップ座標のY成分
    const float clipW = temporalRiftPosition_.x * viewProjection.m[0][3]
        + temporalRiftPosition_.y * viewProjection.m[1][3]
        + temporalRiftPosition_.z * viewProjection.m[2][3]
        + viewProjection.m[3][3]; // 透視除算に使用するW成分

    if (clipW <= kMinimumClipW) {
        return kScreenCenterUv;
    }

    const float ndcX = clipX / clipW; // 透視除算後のX座標
    const float ndcY = clipY / clipW; // 透視除算後のY座標
    Vector2 screenUv = {
        ndcX * 0.5f + 0.5f,
        -ndcY * 0.5f + 0.5f,
    }; // NDC座標を左上原点のUV座標へ変換した結果

    screenUv.x = (std::clamp)(screenUv.x, 0.0f, 1.0f);
    screenUv.y = (std::clamp)(screenUv.y, 0.0f, 1.0f);
    return screenUv;
}

/// <summary>
/// 空間亀裂を複数生成する
/// </summary>
Math::Vector2 TitleScene::CalculateWorldScreenUv(const Math::Vector3& worldPosition) const
{
    constexpr Vector2 kScreenCenterUv = { 0.5f, 0.5f }; // 変換できない場合に使用する画面中央
    constexpr float kMinimumClipW = 0.0001f; // カメラ背面とゼロ除算を判定する最小値
    if (!ctx_.camera) {
        return kScreenCenterUv;
    }

    const Matrix4x4 viewProjection = MathUtil::Multiply(
        ctx_.camera->GetViewMatrix(),
        ctx_.camera->GetProjectionMatrix()); // ワールド座標をクリップ座標へ変換する行列
    const float clipX = worldPosition.x * viewProjection.m[0][0]
        + worldPosition.y * viewProjection.m[1][0]
        + worldPosition.z * viewProjection.m[2][0]
        + viewProjection.m[3][0]; // クリップ座標のX成分
    const float clipY = worldPosition.x * viewProjection.m[0][1]
        + worldPosition.y * viewProjection.m[1][1]
        + worldPosition.z * viewProjection.m[2][1]
        + viewProjection.m[3][1]; // クリップ座標のY成分
    const float clipW = worldPosition.x * viewProjection.m[0][3]
        + worldPosition.y * viewProjection.m[1][3]
        + worldPosition.z * viewProjection.m[2][3]
        + viewProjection.m[3][3]; // 透視除算に使用するW成分
    if (clipW <= kMinimumClipW) {
        return kScreenCenterUv;
    }

    Vector2 screenUv = {
        (clipX / clipW) * 0.5f + 0.5f,
        -(clipY / clipW) * 0.5f + 0.5f,
    }; // NDC座標を左上原点のUV座標へ変換した結果
    screenUv.x = (std::clamp)(screenUv.x, 0.0f, 1.0f);
    screenUv.y = (std::clamp)(screenUv.y, 0.0f, 1.0f);
    return screenUv;
}

/// <summary>
/// 時間ずれ対象のTransformと履歴を更新する
/// </summary>
void TitleScene::UpdateTemporalAfterimages()
{
    constexpr size_t kTemporalTargetIndex = 0; // 時間ずれと残像の対象にする地形の番号
    if (objects3d_.size() <= kTemporalTargetIndex || !objects3d_[kTemporalTargetIndex]) {
        return;
    }

    Object3d* temporalTarget = objects3d_[kTemporalTargetIndex].get(); // 時間ずれ対象の3Dオブジェクト
    if (!hasTemporalTargetBaseTransform_) {
        return;
    }

    float displacementRate = 0.0f; // 現在フェーズで適用する時間ずれ移動率
    switch (temporalRiftPhase_) {
    case TemporalRiftPhase::Compress:
        displacementRate = (std::min)(
            temporalRiftPhaseTime_ / temporalRiftSettings_.compressDuration,
            1.0f);
        break;
    case TemporalRiftPhase::Freeze:
        displacementRate = 1.0f;
        break;
    case TemporalRiftPhase::Crack:
        displacementRate = 1.0f - (std::min)(
            temporalRiftPhaseTime_ / temporalRiftSettings_.crackDuration,
            1.0f);
        break;
    case TemporalRiftPhase::Burst:
    case TemporalRiftPhase::Recover:
        displacementRate = 0.0f;
        break;
    case TemporalRiftPhase::Idle:
        temporalTarget->SetScale(temporalTargetBaseTransform_.scale);
        temporalTarget->SetRotate(temporalTargetBaseTransform_.rotate);
        temporalTarget->SetTranslate(temporalTargetBaseTransform_.translate);
        hasTemporalTargetBaseTransform_ = false;
        temporalTransformHistory_.clear();
        return;
    }

    const float displacement = temporalRiftSettings_.temporalDisplacement * displacementRate; // 現在の時間ずれ移動量
    Vector3 displacedPosition = temporalTargetBaseTransform_.translate; // 時間ずれ適用後の位置
    displacedPosition.x += displacement;
    displacedPosition.y += displacement * 0.25f;
    temporalTarget->SetTranslate(displacedPosition);

    Transform currentTransform {}; // 履歴へ保存する現在のTransform
    currentTransform.scale = temporalTarget->GetScale();
    currentTransform.rotate = temporalTarget->GetRotate();
    currentTransform.translate = temporalTarget->GetTranslate();
    temporalTransformHistory_.push_front(currentTransform);

    const size_t maximumHistoryCount = static_cast<size_t>(
        temporalRiftSettings_.afterimageCount
        * temporalRiftSettings_.afterimageFrameInterval
        + 1); // 残像表示に必要な最大履歴数
    while (temporalTransformHistory_.size() > maximumHistoryCount) {
        temporalTransformHistory_.pop_back();
    }
}

/// <summary>
/// Transform履歴から残像スプライトを更新する
/// </summary>
void TitleScene::UpdateAfterimageSprites()
{
    const int visibleAfterimageCount = temporalRiftPhase_ == TemporalRiftPhase::Idle
        ? 0
        : (std::min)(
              temporalRiftSettings_.afterimageCount,
              static_cast<int>(temporalAfterimageSprites_.size())); // 実際に表示する残像数

    for (size_t spriteIndex = 0; spriteIndex < temporalAfterimageSprites_.size(); ++spriteIndex) {
        Sprite* afterimageSprite = temporalAfterimageSprites_[spriteIndex].get(); // 更新対象の残像スプライト
        if (!afterimageSprite) {
            continue;
        }

        const size_t historyIndex = spriteIndex
            * static_cast<size_t>(temporalRiftSettings_.afterimageFrameInterval); // 残像が参照する履歴番号
        if (static_cast<int>(spriteIndex) >= visibleAfterimageCount
            || historyIndex >= temporalTransformHistory_.size()) {
            afterimageSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            afterimageSprite->Update();
            continue;
        }

        const Transform& historyTransform = temporalTransformHistory_[historyIndex]; // 表示する過去Transform
        const Vector2 screenUv = CalculateWorldScreenUv(historyTransform.translate); // 過去位置の画面UV
        const float lifeRate = 1.0f
            - static_cast<float>(spriteIndex)
                / static_cast<float>((std::max)(visibleAfterimageCount, 1)); // 残像の新しさ
        const float scaleAverage = (
            historyTransform.scale.x
            + historyTransform.scale.y
            + historyTransform.scale.z)
            / 3.0f; // Transformのスケール平均
        const float spriteSize = temporalRiftSettings_.afterimageSize
            * (std::max)(scaleAverage, 0.1f)
            * (0.75f + lifeRate * 0.25f); // 過去Transformに応じた表示サイズ

        afterimageSprite->SetPosition({
            screenUv.x * static_cast<float>(WinApp::kWindowWidth),
            screenUv.y * static_cast<float>(WinApp::kWindowHeight),
        });
        afterimageSprite->SetRotation(historyTransform.rotate.z);
        afterimageSprite->SetSize({ spriteSize, spriteSize });
        afterimageSprite->SetColor({
            temporalRiftSettings_.afterimageColor.x,
            temporalRiftSettings_.afterimageColor.y,
            temporalRiftSettings_.afterimageColor.z,
            temporalRiftSettings_.afterimageAlpha * lifeRate,
        });
        afterimageSprite->Update();
    }
}

/// <summary>
/// Transform履歴による残像を描画する
/// </summary>
void TitleScene::DrawTemporalAfterimages()
{
    if (!ctx_.spriteCommon || temporalRiftPhase_ == TemporalRiftPhase::Idle) {
        return;
    }

    ctx_.spriteCommon->SetCommonDrawSetting();
    for (auto& afterimageSprite : temporalAfterimageSprites_) {
        if (afterimageSprite && afterimageSprite->GetColor().w > 0.0f) {
            afterimageSprite->Draw();
        }
    }
}

/// <summary>
/// 空間亀裂を複数生成する
/// </summary>
/// <summary>
/// 破砕時のヒットストップとカメラシェイクを開始する
/// </summary>
void TitleScene::StartImpactResponse()
{
    hitStopRemainingTime_ = temporalRiftSettings_.hitStopDuration;
    cameraShakeRemainingTime_ = temporalRiftSettings_.cameraShakeDuration;
    cameraShakeElapsedTime_ = 0.0f;

    if (ctx_.camera) {
        cameraShakeBasePosition_ = ctx_.camera->GetTranslate();
        isCameraShakeActive_ = cameraShakeRemainingTime_ > 0.0f;
    }
}

/// <summary>
/// ヒットストップとカメラシェイクを更新する
/// </summary>
void TitleScene::UpdateImpactResponse(float deltaTime)
{
    hitStopRemainingTime_ = (std::max)(
        hitStopRemainingTime_ - deltaTime,
        0.0f);

    if (!isCameraShakeActive_ || !ctx_.camera) {
        return;
    }

    cameraShakeRemainingTime_ = (std::max)(
        cameraShakeRemainingTime_ - deltaTime,
        0.0f);
    cameraShakeElapsedTime_ += deltaTime;

    if (cameraShakeRemainingTime_ <= 0.0f
        || temporalRiftSettings_.cameraShakeDuration <= 0.0f) {
        StopCameraShake();
        return;
    }

    const float shakeRate = cameraShakeRemainingTime_
        / temporalRiftSettings_.cameraShakeDuration; // 揺れの残り割合
    const float phase = cameraShakeElapsedTime_
        * temporalRiftSettings_.cameraShakeFrequency; // 現在の振動位相
    const float strength = temporalRiftSettings_.cameraShakeStrength
        * shakeRate; // 減衰を反映した現在の揺れ幅
    const Vector3 shakeOffset = {
        std::sin(phase * 1.37f) * strength,
        std::sin(phase * 1.91f + 1.2f) * strength * 0.65f,
        std::sin(phase * 1.13f + 2.4f) * strength * 0.35f,
    }; // 軸ごとに周期をずらしたカメラ移動量
    ctx_.camera->SetTranslate({
        cameraShakeBasePosition_.x + shakeOffset.x,
        cameraShakeBasePosition_.y + shakeOffset.y,
        cameraShakeBasePosition_.z + shakeOffset.z,
    });
}

/// <summary>
/// カメラシェイクを終了してカメラ位置を復元する
/// </summary>
void TitleScene::StopCameraShake()
{
    if (isCameraShakeActive_ && ctx_.camera) {
        ctx_.camera->SetTranslate(cameraShakeBasePosition_);
        ctx_.camera->Update();
    }

    isCameraShakeActive_ = false;
    cameraShakeRemainingTime_ = 0.0f;
    cameraShakeElapsedTime_ = 0.0f;
}

/// <summary>
/// 空間亀裂を進行率に合わせて段階的に生成する
/// </summary>
void TitleScene::EmitSpaceCracks()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 亀裂を生成するパーティクル管理
    if (!particleManager) {
        return;
    }

    constexpr std::array<float, 7> kCrackAngles = {
        -1.15f, -0.72f, -0.28f, 0.12f, 0.55f, 0.95f, 1.34f,
    }; // 各亀裂のZ軸回転
    constexpr std::array<Vector3, 7> kCrackOffsets = {
        Vector3 { -0.20f, 0.08f, 0.00f },
        Vector3 { -0.38f, -0.12f, 0.01f },
        Vector3 { -0.12f, 0.30f, 0.02f },
        Vector3 { 0.10f, -0.28f, 0.03f },
        Vector3 { 0.30f, 0.16f, 0.04f },
        Vector3 { 0.48f, -0.06f, 0.05f },
        Vector3 { 0.62f, 0.30f, 0.06f },
    }; // 中心から枝分かれして見える位置補正

    const int crackCount = (std::clamp)(
        temporalRiftSettings_.crackCount,
        1,
        static_cast<int>(kCrackAngles.size())); // 生成する亀裂総数
    const float crackDuration = (std::max)(temporalRiftSettings_.crackDuration, 0.0001f); // 亀裂展開時間
    const float progress = (std::clamp)(temporalRiftPhaseTime_ / crackDuration, 0.0f, 1.0f); // 亀裂展開の進行率
    const int targetCrackCount = (std::min)(
        crackCount,
        static_cast<int>(progress * static_cast<float>(crackCount)) + 1); // 現在までに表示する亀裂数

    while (temporalCracksEmitted_ < targetCrackCount) {
        const size_t crackIndex = static_cast<size_t>(temporalCracksEmitted_); // 今回生成する亀裂番号
        const Vector3& offset = kCrackOffsets[crackIndex]; // 亀裂の位置補正
        const Vector3 crackPosition = {
            temporalRiftPosition_.x + offset.x,
            temporalRiftPosition_.y + offset.y,
            temporalRiftPosition_.z + offset.z,
        }; // 亀裂のワールド座標
        const float branchRate = static_cast<float>(crackIndex) / static_cast<float>((std::max)(crackCount - 1, 1)); // 枝先への進行率
        const float crackLength = temporalRiftSettings_.crackLength
            + branchRate * temporalRiftSettings_.crackLengthVariation; // 枝ごとの長さ
        const float crackWidth = temporalRiftSettings_.crackWidth
            + static_cast<float>(crackIndex % 2) * temporalRiftSettings_.crackWidthVariation; // 枝ごとの太さ
        Vector4 crackColor = temporalRiftSettings_.crackColor; // 亀裂の発光色
        crackColor.x = (std::min)(crackColor.x + branchRate * 0.18f, 1.0f);
        crackColor.z = (std::min)(crackColor.z + branchRate * 0.12f, 1.0f);

        particleManager->EmitSpaceCrack(
            "Hit",
            crackPosition,
            kCrackAngles[crackIndex],
            crackLength,
            crackWidth,
            crackColor,
            temporalRiftSettings_.crackLifeTime);
        ++temporalCracksEmitted_;
    }
}
/// <summary>
/// 空間崩壊時の多重リングと放射破片を生成する
/// </summary>
void TitleScene::EmitRiftBurst()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 破裂表現を生成するパーティクル管理
    if (!particleManager) {
        return;
    }

    const uint32_t totalRingCount = static_cast<uint32_t>((std::max)(temporalRiftSettings_.ringCount, 0)); // 生成するリング総数
    const uint32_t innerRingCount = (totalRingCount + 1u) / 2u; // 青白い内側リング数
    const uint32_t outerRingCount = totalRingCount / 2u; // 紫色の外側リング数
    particleManager->EmitRiftRing(
        "Ring",
        temporalRiftPosition_,
        innerRingCount,
        temporalRiftSettings_.innerRingColor,
        0.12f,
        1.8f,
        0.55f);
    particleManager->EmitRiftRing(
        "Ring",
        temporalRiftPosition_,
        outerRingCount,
        temporalRiftSettings_.outerRingColor,
        0.28f,
        2.5f,
        0.85f);
    particleManager->EmitRiftFragments(
        "Hit",
        temporalRiftPosition_,
        static_cast<uint32_t>((std::max)(temporalRiftSettings_.fragmentCount, 0)),
        temporalRiftSettings_.fragmentColor,
        temporalRiftSettings_.fragmentMinSpeed,
        temporalRiftSettings_.fragmentMaxSpeed,
        temporalRiftSettings_.fragmentLifeTime);
}
/// <summary>
/// プレイシーンを描画する
/// </summary>
void TitleScene::Draw()
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 描画先を切り替えるDirectX基盤
    const bool canUsePostProcess = directXCommon
        && sceneRenderTargetHandle_ >= 0
        && sceneRenderTargetSrvIndex_ != UINT32_MAX
        && postProcess_.IsReady(); // ポストプロセスを実行できる状態か
    const bool isTemporalChainPhase =
        temporalRiftPhase_ == TemporalRiftPhase::Compress
        || temporalRiftPhase_ == TemporalRiftPhase::Burst; // 歪みとブラーを連結するフェーズか
    const bool canUseTemporalChain = canUsePostProcess
        && isTemporalChainPhase
        && postProcessIntermediateHandle_ >= 0
        && postProcessIntermediateSrvIndex_ != UINT32_MAX; // 2パス連結を実行できる状態か

    if (canUsePostProcess) {
        directXCommon->BeginRenderTo(sceneRenderTargetHandle_, true);
        DrawWorldAndParticles();
        DrawTemporalAfterimages();
        DrawTimeReversalParticles();
        directXCommon->EndRenderTo(sceneRenderTargetHandle_);

        if (canUseTemporalChain) {
            // 1パス目で空間歪みを中間レンダーターゲットへ描画する
            directXCommon->BeginRenderTo(postProcessIntermediateHandle_, true);
            postProcess_.DrawTexture(
                sceneRenderTargetSrvIndex_,
                PostEffectType::Distortion);
            directXCommon->EndRenderTo(postProcessIntermediateHandle_);

            // 2パス目で歪み結果へラジアルブラーを適用する
            postProcess_.DrawTexture(
                postProcessIntermediateSrvIndex_,
                PostEffectType::RadialBlur);
        } else {
            postProcess_.DrawTexture(sceneRenderTargetSrvIndex_);
        }

        DrawSprites();
        return;
    }

    DrawWorldAndParticles();
    DrawTemporalAfterimages();
    DrawTimeReversalParticles();
    DrawSprites();
}

/// <summary>
/// 地形とパーティクルを描画する
/// </summary>
void TitleScene::DrawWorldAndParticles()
{
    if (!ctx_.object3dCommon) {
        return;
    }

    ctx_.object3dCommon->SetCommonDrawSetting();
    for (auto& object : objects3d_) {
        if (object) {
            object->Draw();
        }
    }

    if (!ctx_.camera) {
        return;
    }

    const Matrix4x4 view = ctx_.camera->GetViewMatrix(); // パーティクル描画に使用するビュー行列
    const Matrix4x4 projection = ctx_.camera->GetProjectionMatrix(); // パーティクル描画に使用する射影行列
    const Matrix4x4 viewProjection = MathUtil::Multiply(view, projection); // ビュー射影行列
    const Vector3 cameraRight = { view.m[0][0], view.m[1][0], view.m[2][0] }; // ビルボードの右方向
    const Vector3 cameraUp = { view.m[0][1], view.m[1][1], view.m[2][1] }; // ビルボードの上方向
    ctx_.object3dCommon->SetBillboardCameraWithVP(
        cameraRight,
        cameraUp,
        viewProjection,
        true);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // 描画するパーティクル管理
    if (particleManager) {
        particleManager->Draw();
    }
}
/// <summary>
/// ポストプロセスの影響を受けないスプライトを描画する
/// </summary>
void TitleScene::DrawSprites()
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
void TitleScene::OnEnter() { std::cout << "TitleScene OnEnter\n"; }

/// <summary>
/// シーンから出るときの処理
/// </summary>
void TitleScene::OnExit() { std::cout << "TitleScene OnExit\n"; }

/// <summary>
/// 描画モードの更新を受け取る
/// </summary>
void TitleScene::SetSelectedDrawType(int t)
{
    ctx_.selectedDrawType = t;
}

/// <summary>
/// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void TitleScene::FillObject3dPointers(std::vector<Object3d*>* out)
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
void TitleScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out)
        return;
    out->clear();
    out->reserve(sprites_.size());
    for (auto& s : sprites_) {
        out->push_back(s.get());
    }
}
