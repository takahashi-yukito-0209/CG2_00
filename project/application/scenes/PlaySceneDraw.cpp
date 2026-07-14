#include "PlayScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/SkyBox.h"
#include "../../engine/particle/ParticleManager.h"

using namespace MyEngine;

namespace {
constexpr int kDrawTypeAll = -1; // すべての描画対象を表示する種別
constexpr int kDrawTypeModel = 0; // モデル単体を表示する種別
constexpr int kDrawTypeParticle = 1; // パーティクルを表示する種別
constexpr int kDrawTypeSprite = 2; // スプライトを表示する種別
constexpr int kDrawTypeBunny = 3; // Bunnyモデルを表示する種別
constexpr int kDrawTypeFence = 4; // Fenceモデルを表示する種別
constexpr int kDrawTypeChecker = 5; // Checkerモデルを表示する種別
constexpr int kDrawTypeSphere = 6; // Sphere系モデルを表示する種別
constexpr int kDrawTypeSimpleSkin = 7; // simpleSkinモデルを表示する種別
constexpr int kDrawTypeHumanSneakWalk = 8; // human/sneakWalkモデルを表示する種別
constexpr int kDrawTypeHumanWalk = 9; // human/walkモデルを表示する種別
constexpr int kDrawTypeAllDebug = 10; // デバッグ用にすべてを表示する種別
constexpr size_t kModelObjectIndex = 0; // 通常モデルの登録番号
constexpr size_t kBunnyObjectIndex = 1; // Bunnyモデルの登録番号
constexpr size_t kCheckerObjectIndex = 2; // Checkerモデルの登録番号
constexpr size_t kFenceObjectIndex = 3; // Fenceモデルの登録番号
constexpr size_t kSphereObjectStartIndex = 4; // Sphere系モデルの先頭登録番号
constexpr size_t kSphereObjectEndIndex = 6; // Sphere系モデルの末尾登録番号
constexpr size_t kSimpleSkinObjectIndex = 7; // simpleSkinモデルの登録番号
constexpr size_t kHumanSneakWalkObjectIndex = 8; // human/sneakWalkモデルの登録番号
constexpr size_t kHumanWalkObjectIndex = 9; // human/walkモデルの登録番号
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
    for (auto& object3d : objects3d_) { // 更新対象の3Dオブジェクト
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
    case kDrawTypeModel:
        DrawObject3dAtIndex(kModelObjectIndex);
        break;
    case kDrawTypeBunny:
        DrawObject3dAtIndex(kBunnyObjectIndex);
        break;
    case kDrawTypeFence:
        DrawObject3dAtIndex(kFenceObjectIndex);
        break;
    case kDrawTypeChecker:
        DrawObject3dAtIndex(kCheckerObjectIndex);
        break;
    case kDrawTypeSphere:
        for (size_t objectIndex = kSphereObjectStartIndex; objectIndex <= kSphereObjectEndIndex; ++objectIndex) {
            DrawObject3dAtIndex(objectIndex);
        }
        break;
    case kDrawTypeSimpleSkin:
        DrawObject3dAtIndex(kSimpleSkinObjectIndex);
        break;
    case kDrawTypeHumanSneakWalk:
        DrawObject3dAtIndex(kHumanSneakWalkObjectIndex);
        break;
    case kDrawTypeHumanWalk:
        DrawObject3dAtIndex(kHumanWalkObjectIndex);
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
    return selectedDrawType == kDrawTypeAll
        || selectedDrawType == kDrawTypeParticle
        || selectedDrawType == kDrawTypeAllDebug
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
    if (selectedDrawType == kDrawTypeAll || selectedDrawType == kDrawTypeAllDebug) {
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

    if (selectedDrawType == kDrawTypeAll || selectedDrawType == kDrawTypeSprite || selectedDrawType == kDrawTypeAllDebug) {
        ctx_.spriteCommon->SetCommonDrawSetting();
        for (auto& sprite : sprites_) { // 更新対象の確認用スプライト
            if (sprite) {
                sprite->Draw();
            }
        }
    }
}