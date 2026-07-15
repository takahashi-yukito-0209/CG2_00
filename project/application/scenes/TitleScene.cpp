#include "TitleScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/PrimitiveFactory.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/io/InputManager.h"
#include <iostream>
#include <array>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr int kDrawTypeLegacyAll = -1; // 旧形式の全描画指定
constexpr int kDrawTypeModel = 0; // 3Dモデル描画
constexpr int kDrawTypeParticle = 1; // パーティクル描画
constexpr int kDrawTypeSprite = 2; // スプライト描画
constexpr int kDrawTypeAll = 10; // すべての描画対象
constexpr const char* kTitleGpuParticleGroupName = "TitleGpuParticle"; // タイトル確認用GPUパーティクルグループ名
constexpr const char* kTitleGpuParticleTextureName = "circle.png"; // タイトル確認用GPUパーティクルテクスチャ
constexpr std::array<float, 4> kTitleSceneClearColor = { 0.10f, 0.12f, 0.16f, 1.0f }; // タイトルScene Viewのクリア色
constexpr bool kDisableAlphaCutoutSampler = false; // 円形テクスチャの透明度をそのまま使う設定

/// <summary>
/// タイトルシーンをScene Viewへ表示するためのRenderTarget設定を作成する。
/// </summary>
RenderTargetDesc CreateTitleSceneRenderTargetDesc(DirectXCommon* directXCommon)
{
    RenderTargetDesc desc {}; // Scene View用RenderTarget設定
    desc.width = directXCommon ? static_cast<uint32_t>(directXCommon->GetRenderWidth()) : 1u;
    desc.height = directXCommon ? static_cast<uint32_t>(directXCommon->GetRenderHeight()) : 1u;
    desc.format = directXCommon ? directXCommon->GetSwapChainFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.useDepth = true;
    desc.createColorSrv = true;
    desc.createDepthSrv = false;
    desc.resizeWithWindow = true;
    desc.clearColor = kTitleSceneClearColor;
    return desc;
}

/// <summary>
/// タイトルシーンでGPUパーティクル確認用の描画平面を作成する。
/// </summary>
std::unique_ptr<Object3d> CreateTitleParticleDrawObject(const SceneContext& ctx)
{
    auto particleObject = std::make_unique<Object3d>(); // GPUパーティクルを描画する平面Object
    particleObject->Initialize(ctx.object3dCommon, ctx.imguiManager);
    particleObject->SetMesh(PrimitiveFactory::CreatePlane());
    particleObject->SetTexture(kTitleGpuParticleTextureName);
    particleObject->SetEnableLighting(false);
    particleObject->SetUseAlphaCutoutSampler(kDisableAlphaCutoutSampler);
    return particleObject;
}
}

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

    particlePlane_ = CreateTitleParticleDrawObject(ctx_);
    if (ctx_.directXCommon) {
        sceneRenderTarget_.Initialize(ctx_.directXCommon, ctx_.srvManager, CreateTitleSceneRenderTargetDesc(ctx_.directXCommon));
    }

    ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル確認に使う管理クラス
        if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }

    if (particleManager) {
        particleManager->Initialize(ctx_.directXCommon, ctx_.object3dCommon, ctx_.srvManager, ctx_.textureManager, ctx_.imguiManager);
        particleManager->SetParticlePlane(particlePlane_.get());
        particleManager->CreateParticleGroup(kTitleGpuParticleGroupName, kTitleGpuParticleTextureName);
        particleManager->SetParticleObject(kTitleGpuParticleGroupName, particlePlane_.get());
    }
}

/// <summary>
/// 終了処理
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";

    ParticleManager* particleManager = ctx_.particleManager; // タイトルで登録したパーティクル管理
        if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }
    if (particleManager) {
        particleManager->Finalize();
    }

    sceneRenderTarget_.Finalize();
    particlePlane_.reset();
    sprites_.clear();
    objects3d_.clear();
    ctx_ = {};
}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update(float dt)
{
    ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル確認に使う管理クラス
        if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }
    if (particleManager) {
        particleManager->Update(dt);
    }

    InputManager* inputManager = InputManager::GetInstance(); // 蜈･蜉帷ｮ｡逅・
    if (inputManager && inputManager->IsKeyJustPressed(DIK_SPACE)) {
        if (ctx_.requestSceneChange) {
            ctx_.requestSceneChange("Play");
        }
        return;
    }

    if (ctx_.camera) {
        ctx_.camera->Update();
    }

    if (!ctx_.camera) {
        return;
    }

    const Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // 3Dオブジェクト更新に使用するビュー行列
    const Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // 3Dオブジェクト更新に使用する射影行列
    for (auto& object : objects3d_) {
        if (object) {
            object->Update(viewMatrix, projectionMatrix);
        }
    }

    for (auto& sprite : sprites_) {
        if (sprite) {
            sprite->Update();
        }
    }
}

/// <summary>
/// 描画処理
/// </summary>
void TitleScene::Draw()
{
    const bool useSceneViewTarget = sceneViewOnly_ && sceneRenderTarget_.IsValid(); // ImGui Scene Viewへ描画するか
    if (useSceneViewTarget) {
        sceneRenderTarget_.Begin(true);
    }

    DrawWorldObjects();

    if (ctx_.spriteCommon) {
        const int selectedDrawType = ctx_.selectedDrawType; // ImGui縺ｧ驕ｸ謚槭＆繧後※縺・ｋ謠冗判遞ｮ蛻･
        if (selectedDrawType == kDrawTypeLegacyAll || selectedDrawType == kDrawTypeSprite || selectedDrawType == kDrawTypeAll) {
            for (auto& sprite : sprites_) {
                if (sprite) {
                    sprite->Draw();
                }
            }
        }
    }

    if (useSceneViewTarget) {
        sceneRenderTarget_.End();
    }
}

/// <summary>
/// タイトル用の3Dオブジェクトを描画する
/// </summary>
void TitleScene::DrawWorldObjects()
{
    if (!ctx_.object3dCommon) {
        return;
    }

    const int selectedDrawType = ctx_.selectedDrawType; // ImGui縺ｧ驕ｸ謚槭＆繧後※縺・ｋ謠冗判遞ｮ蛻･
    if (selectedDrawType != kDrawTypeLegacyAll && selectedDrawType != kDrawTypeModel && selectedDrawType != kDrawTypeParticle && selectedDrawType != kDrawTypeAll) {
        return;
    }

    if (selectedDrawType == kDrawTypeLegacyAll || selectedDrawType == kDrawTypeModel || selectedDrawType == kDrawTypeAll) {
        ctx_.object3dCommon->SetCommonDrawSetting();
        for (auto& object : objects3d_) {
            if (object) {
                object->Draw();
            }
        }
    }

    if (selectedDrawType == kDrawTypeLegacyAll || selectedDrawType == kDrawTypeParticle || selectedDrawType == kDrawTypeAll) {
        ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル確認に使う管理クラス
        if (!particleManager) {
            particleManager = ParticleManager::GetInstance();
        }
        if (particleManager) {
            particleManager->Draw();
        }
    }
}

/// <summary>
/// シーンに入るときの処理
/// </summary>
void TitleScene::OnEnter()
{
    std::cout << "TitleScene OnEnter\n";
}

/// <summary>
/// シーンから出るときの処理
/// </summary>
void TitleScene::OnExit()
{
    std::cout << "TitleScene OnExit\n";
}

/// <summary>
/// 描画モードの更新を受け取る
/// </summary>
void TitleScene::SetSelectedDrawType(int type)
{
    ctx_.selectedDrawType = type;
}
/// <summary>
/// Scene View用のオフスクリーン描画だけにするか設定する。
/// </summary>
void TitleScene::SetSceneViewOnly(bool enabled)
{
    sceneViewOnly_ = enabled;
}

/// <summary>
/// Scene Viewに表示するSRV番号を取得する。
/// </summary>
uint32_t TitleScene::GetSceneViewSrvIndex() const
{
    if (sceneRenderTarget_.HasColorSrv()) {
        return sceneRenderTarget_.GetColorSrvIndex();
    }
    return UINT32_MAX;
}

/// <summary>
/// ウィンドウリサイズ時にScene View用RenderTargetを追従させる。
/// </summary>
void TitleScene::OnWindowResize(uint32_t width, uint32_t height)
{
    if (sceneRenderTarget_.IsValid()) {
        sceneRenderTarget_.Resize(width, height);
    }
}

/// <summary>
/// シーンが所有する3Dオブジェクトのポインタを収集する
/// </summary>
void TitleScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) {
        return;
    }

    out->reserve(objects3d_.size());
    for (auto& object : objects3d_) {
        if (object) {
            out->push_back(object.get());
        }
    }
}

/// <summary>
/// シーンが所有するスプライトのポインタを収集する
/// </summary>
void TitleScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out) {
        return;
    }

    out->reserve(sprites_.size());
    for (auto& sprite : sprites_) {
        if (sprite) {
            out->push_back(sprite.get());
        }
    }
}
