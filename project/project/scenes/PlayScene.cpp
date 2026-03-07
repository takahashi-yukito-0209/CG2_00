#include "PlayScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/base/SrvManager.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/utility/mathUtility.h"
#include <iostream>

/// <summary>
/// コンストラクタ
/// </summary>
PlayScene::PlayScene() { }

/// <summary>
/// デストラクタ
/// </summary>
PlayScene::~PlayScene() { }

/// <summary>
/// 初期化処理
/// </summary>
void PlayScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;
    std::cout << "PlayScene Initialize\n";

    // テクスチャのロード
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture("resources/uvChecker.png");
        ctx_.textureManager->LoadTexture("resources/monsterBall.png");
        ctx_.textureManager->LoadTexture("resources/circle.png");
    }

    // スプライトの作成
    const uint32_t kSpriteCount = 5;
    // 2種類のテクスチャを交互に使用してスプライトを作成
    std::array<std::string, 2> spriteNames = {
        "resources/uvChecker",
        "resources/monsterBall"
    };

    // 2種類のテクスチャファイル名を配列で管理
    for (uint32_t i = 0; i < kSpriteCount; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(ctx_.spriteCommon, spriteNames[(i / 2) == 0 ? 0 : 1] + ".png");
        sprites_.push_back(std::move(sprite));
    }

    // モデルルファイル名の配列を作成
    std::vector<std::string> modelFileNames = {
        "plane.gltf",
        "bunny.obj",
        "teapot.obj",
        "models/fence/fence.obj",
        "models/sphere/sphere.gltf",
        "models/terrain/terrain.obj"
    };

    for (size_t i = 0; i < modelFileNames.size(); ++i) {
        auto obj = std::make_unique<Object3d>();
        obj->Initialize(ctx_.object3dCommon);
        obj->SetModel(modelFileNames[i]);
        if (modelFileNames[i].find("fence") != std::string::npos) {
            obj->SetUseAlphaCutoutSampler(true);
        }
        objects3d_.push_back(std::move(obj));
    }

    //パーティクルの初期化
    particlePlane_ = std::make_unique<Object3d>();
    particlePlane_->Initialize(ctx_.object3dCommon);
    particlePlane_->SetModel("plane.obj");
    particlePlane_->SetTexture("resources/circle.png");

    // パーティクルマネージャー化とグループの作成
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Initialize(ctx_.directXCommon, ctx_.object3dCommon, ctx_.srvManager, ctx_.textureManager);
        ParticleManager::GetInstance()->SetParticlePlane(particlePlane_.get());
        ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/circle.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Checker", "resources/uvChecker.png");
        ParticleManager::GetInstance()->CreateParticleGroup("Ball", "resources/monsterBall.png");
    }

    // パーティクルエミッターと発射
    pmEmitter_.groupName = "Circle";
    pmEmitter_.transform.translate = { 0.0f, 0.0f, 0.0f };
    pmEmitter_.count = 3;
    pmEmitter_.frequency = 0.5f;
    for (int i = 0; i < 20; ++i) {
        pmEmitter_.Emit();
    }
}

/// <summary>
/// 終了処理
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    sprites_.clear();
    objects3d_.clear();
    // 3Dオブジェクトの解放
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->SetParticlePlane(nullptr);
    }
    particlePlane_.reset();
}

/// <summary>
/// 更新処理
/// </summary>
void PlayScene::Update(float dt)
{
    //カメラの更新
    if (ctx_.camera) {
        ctx_.camera->Update();
    }

    // パーティクルエミッターの更新とマネージャーの更新
    pmEmitter_.Update(dt);
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Update(dt);
    }

    //オブジェクトの更新
    for (auto& o : objects3d_) {
        if (o) {
            if (ctx_.camera) {
                o->Update(ctx_.camera->GetViewMatrix(), ctx_.camera->GetProjectionMatrix());
            }
        }
    }

    //スプライトの更新
    for (auto& s : sprites_) {
        if (s)
            s->Update();
    }
}

/// <summary>
/// 描画処理
/// </summary>
void PlayScene::Draw()
{
    //オブジェクトの描画
    if (ctx_.object3dCommon) {
        ctx_.object3dCommon->SetCommonDrawSetting();
        for (auto& o : objects3d_) {
            if (o)
                o->Draw();
        }
        // パーティクルの描画
        MathUtility math;
        if (ctx_.camera) {
            Matrix4x4 view = ctx_.camera->GetViewMatrix();
            Matrix4x4 proj = ctx_.camera->GetProjectionMatrix();
            Matrix4x4 vp = math.Multiply(view, proj);
            Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
            Vector3 up = { view.m[0][1], view.m[1][1], view.m[2][1] };
            ctx_.object3dCommon->SetBillboardCameraWithVP(right, up, vp, true);

            if (ParticleManager::GetInstance()) {
                ParticleManager::GetInstance()->Draw();
            }
        }
    }

    // スプライトの描画
    if (ctx_.spriteCommon) {
        ctx_.spriteCommon->SetCommonDrawSetting();
        for (auto& s : sprites_) {
            if (s)
                s->Draw();
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
