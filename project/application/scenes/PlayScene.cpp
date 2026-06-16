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
#include "../../engine/base/SrvManager.h"
#include "../../engine/3d/SkyBox.h"
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

    // テクスチャのロード
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture("uvChecker.png");
        ctx_.textureManager->LoadTexture("monsterBall.png");
        ctx_.textureManager->LoadTexture("circle.png");
        ctx_.textureManager->LoadTexture("gradationLine.png");
        // 環境マップ用DDSを読み込む（確認用）
        ctx_.textureManager->LoadTexture("rostock_laage_airport_4k.dds");
        // 読み込んだテクスチャをGPUに転送
        ctx_.textureManager->ExecuteResourceUpload();
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
        if (modelFileNames[i].find("sphere") != std::string::npos) {
            obj->SetEnvironmentCoefficient(0.5f);
        }
        if (modelFileNames[i].find("cube") != std::string::npos ||
            modelFileNames[i].find("Cube") != std::string::npos) {
            obj->SetEnvironmentCoefficient(0.85f);
            obj->SetTranslate({ 3.0f, 0.0f, 0.0f });
        }
        objects3d_.push_back(std::move(obj));
    }

    objects3d_[5]->SetScale({ 5.0f, 5.0f, 5.0f }); // terrain を大きくする

    //パーティクルの初期化
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
/// 終了処理
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    // パーティクルマネージャーの終了処理
    if (ParticleManager::GetInstance()) {
        ParticleManager::GetInstance()->Finalize();
    }

    // スプライトとオブジェクトの解放
    sprites_.clear();
    objects3d_.clear();

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
    ringEmitter_.Update(dt);
    cylinderEmitter_.Update(dt);
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
                if (o) o->Draw();
            }
        } else {
            // 個別オブジェクト描画マッピング
            ctx_.object3dCommon->SetCommonDrawSetting();
            switch (sel) {
            case 0: // Model -> index 0
                if (objects3d_.size() > 0 && objects3d_[0]) objects3d_[0]->Draw();
                break;
            case 3: // Bunny -> index 1
                if (objects3d_.size() > 1 && objects3d_[1]) objects3d_[1]->Draw();
                break;
            case 4: // Fence -> index 3
                if (objects3d_.size() > 3 && objects3d_[3]) objects3d_[3]->Draw();
                break;
            case 5: // Checker -> index 2
                if (objects3d_.size() > 2 && objects3d_[2]) objects3d_[2]->Draw();
                break;
            case 6: // Sphere -> index 4 and 5 if present
                if (objects3d_.size() > 4 && objects3d_[4]) objects3d_[4]->Draw();
                if (objects3d_.size() > 5 && objects3d_[5]) objects3d_[5]->Draw();
                if (objects3d_.size() > 6 && objects3d_[6]) objects3d_[6]->Draw();
                break;
            default:
                // その他（Particle/Sprite）はここでは扱わない
                break;
            }
        }

        // パーティクル描画は Particle モードまたは All のときに行う
        if (sel == -1 || sel == 1 || sel == 7) {
            if (ctx_.camera) {
                Matrix4x4 view = ctx_.camera->GetViewMatrix();
                Matrix4x4 proj = ctx_.camera->GetProjectionMatrix();
                Matrix4x4 vp = MathUtil::Multiply(view, proj);
                Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
                Vector3 up = { view.m[0][1], view.m[1][1], view.m[2][1] };
                ctx_.object3dCommon->SetBillboardCameraWithVP(right, up, vp, true);

                if (ParticleManager::GetInstance()) {
                    ParticleManager::GetInstance()->Draw();
                }
            }
        }
    }

    // スプライト描画は Sprite モードまたは All のときに行う
    if (ctx_.spriteCommon) {
        if (sel == -1 || sel == 2 || sel == 7) {
            ctx_.spriteCommon->SetCommonDrawSetting();
            for (auto& s : sprites_) {
                if (s) s->Draw();
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
/// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) return;
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
    if (!out) return;
    out->clear();
    out->reserve(sprites_.size());
    for (auto& s : sprites_) {
        out->push_back(s.get());
    }
}
