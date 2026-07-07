#include "TitleScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/io/InputManager.h"
#include <iostream>

using namespace MyEngine;
using namespace Math;

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
}

/// <summary>
/// 終了処理
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";

    sprites_.clear();
    objects3d_.clear();
    ctx_ = {};
}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update(float dt)
{
    (void)dt;

    InputManager* inputManager = InputManager::GetInstance(); // 入力管理
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
    DrawWorldObjects();

    if (!ctx_.spriteCommon) {
        return;
    }

    const int selectedDrawType = ctx_.selectedDrawType; // ImGuiで選択されている描画種別
    if (selectedDrawType == -1 || selectedDrawType == 2 || selectedDrawType == 7) {
        for (auto& sprite : sprites_) {
            if (sprite) {
                sprite->Draw();
            }
        }
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

    const int selectedDrawType = ctx_.selectedDrawType; // ImGuiで選択されている描画種別
    if (selectedDrawType != -1 && selectedDrawType != 0 && selectedDrawType != 7) {
        return;
    }

    ctx_.object3dCommon->SetCommonDrawSetting();
    for (auto& object : objects3d_) {
        if (object) {
            object->Draw();
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
