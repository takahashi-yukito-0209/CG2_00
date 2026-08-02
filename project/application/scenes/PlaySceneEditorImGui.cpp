#include "PlayScene.h"
#include "ImGuiManager.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/3d/Object3d.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace MyEngine;

namespace {
constexpr std::array<const char*, 13> kSceneObjectCreateModelNames = {
    "block/block.obj",
    "plane/plane.gltf",
    "bunny/bunny.obj",
    "teapot/teapot.obj",
    "fence/fence.obj",
    "sphere/sphere.gltf",
    "terrain/terrain.obj",
    "multiMesh/multiMesh.obj",
    "multiMaterial/multiMaterial.obj",
    "AnimatedCube/AnimatedCube.gltf",
    "simpleSkin/simpleSkin.gltf",
    "human/sneakWalk.gltf",
    "human/walk.gltf",
}; // ImGuiから生成できる3Dモデル名
constexpr std::array<const char*, 13> kSceneObjectCreateModelDisplayNames = {
    "block.obj",
    "plane.gltf",
    "bunny.obj",
    "teapot.obj",
    "fence.obj",
    "sphere.gltf",
    "terrain.obj",
    "multiMesh.obj",
    "multiMaterial.obj",
    "AnimatedCube.gltf",
    "simpleSkin.gltf",
    "sneakWalk.gltf",
    "walk.gltf",
}; // ImGuiに表示する3Dモデル名
constexpr std::array<const char*, 5> kSceneSpriteCreateTextureNames = {
    "uvChecker.png",
    "monsterBall.png",
    "circle.png",
    "circle2.png",
    "gradationLine.png",
}; // ImGuiから生成できるスプライト用テクスチャ名

/// <summary>
/// パス文字列から表示用のファイル名部分だけを取得する。
/// </summary>
std::string GetDisplayFileName(const std::string& path)
{
    const size_t separatorPosition = path.find_last_of("/\\"); // 最後に見つかったパス区切り位置
    if (separatorPosition == std::string::npos) {
        return path;
    }

    return path.substr(separatorPosition + 1);
}
} // namespace

/// <summary>
/// ImGuiでシーン内スプライトの生成と削除を行う。
/// </summary>
void PlayScene::DrawSceneSpriteEditImGui()
{
#ifdef USE_IMGUI
    static int selectedCreateTextureIndex = 0; // 生成に使用するテクスチャ番号
    static int selectedDeleteSpriteIndex = 0; // 削除対象のスプライト番号

    ImGui::SeparatorText("Create");
    const char* textureNames[kSceneSpriteCreateTextureNames.size()] = {}; // Combo表示用のテクスチャ名一覧
    for (size_t textureIndex = 0; textureIndex < kSceneSpriteCreateTextureNames.size(); ++textureIndex) {
        textureNames[textureIndex] = kSceneSpriteCreateTextureNames[textureIndex];
    }

    ImGui::Combo(
        "Texture",
        &selectedCreateTextureIndex,
        textureNames,
        static_cast<int>(kSceneSpriteCreateTextureNames.size()));
    selectedCreateTextureIndex = (std::clamp)(
        selectedCreateTextureIndex,
        0,
        static_cast<int>(kSceneSpriteCreateTextureNames.size()) - 1);
    if (ImGui::Button("Create Sprite")) {
        const std::string textureName = kSceneSpriteCreateTextureNames[static_cast<size_t>(selectedCreateTextureIndex)]; // 生成するスプライトのテクスチャ名
        CreateSceneSprite(textureName);
        selectedDeleteSpriteIndex = static_cast<int>(sprites_.size()) - 1;
    }

    ImGui::SeparatorText("Delete");
    if (!sprites_.empty()) {
        const int spriteCount = static_cast<int>(sprites_.size()); // 削除対象として選択できるスプライト数
        selectedDeleteSpriteIndex = (std::clamp)(selectedDeleteSpriteIndex, 0, spriteCount - 1);

        std::string preview = "Sprite " + std::to_string(selectedDeleteSpriteIndex); // Comboの現在表示名
        Sprite* previewSprite = sprites_[static_cast<size_t>(selectedDeleteSpriteIndex)].get(); // 現在選択中のスプライト
        if (previewSprite && !previewSprite->GetTextureFilePath().empty()) {
            preview += " : " + GetDisplayFileName(previewSprite->GetTextureFilePath());
        }

        if (ImGui::BeginCombo("Delete Target", preview.c_str())) {
            for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
                Sprite* sprite = sprites_[static_cast<size_t>(spriteIndex)].get(); // 表示名を作る対象のスプライト
                std::string label = "Sprite " + std::to_string(spriteIndex); // Comboに表示するスプライト名
                if (sprite && !sprite->GetTextureFilePath().empty()) {
                    label += " : " + GetDisplayFileName(sprite->GetTextureFilePath());
                }

                const bool isSelected = selectedDeleteSpriteIndex == spriteIndex; // 現在選択中かどうか
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedDeleteSpriteIndex = spriteIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Delete Sprite")) {
            DeleteSceneSprite(static_cast<size_t>(selectedDeleteSpriteIndex));
            selectedDeleteSpriteIndex = (std::min)(selectedDeleteSpriteIndex, static_cast<int>(sprites_.size()) - 1);
        }
    } else {
        ImGui::Text("No sprites.");
    }
#endif
}

/// <summary>
/// ImGuiでシーン内3Dオブジェクトの生成と削除を行う
/// </summary>
void PlayScene::DrawSceneObjectEditImGui()
{
#ifdef USE_IMGUI
    static int selectedCreateModelIndex = 0; // 生成に使用するモデル番号
    static int selectedDeleteObjectIndex = 0; // 削除対象のオブジェクト番号

    ImGui::SeparatorText("Create");
    const char* modelNames[kSceneObjectCreateModelDisplayNames.size()] = {}; // Combo表示用のモデル名一覧
    for (size_t modelIndex = 0; modelIndex < kSceneObjectCreateModelDisplayNames.size(); ++modelIndex) {
        modelNames[modelIndex] = kSceneObjectCreateModelDisplayNames[modelIndex];
    }

    ImGui::Combo(
        "Model",
        &selectedCreateModelIndex,
        modelNames,
        static_cast<int>(kSceneObjectCreateModelDisplayNames.size()));
    selectedCreateModelIndex = (std::clamp)(
        selectedCreateModelIndex,
        0,
        static_cast<int>(kSceneObjectCreateModelDisplayNames.size()) - 1);
    if (ImGui::Button("Create Object")) {
        const std::string modelFileName = kSceneObjectCreateModelNames[static_cast<size_t>(selectedCreateModelIndex)]; // 生成するモデルファイル名
        CreateSceneObject(modelFileName);
        selectedDeleteObjectIndex = static_cast<int>(objects3d_.size()) - 1;
    }

    ImGui::SeparatorText("Delete");
    if (!objects3d_.empty()) {
        const int objectCount = static_cast<int>(objects3d_.size()); // 削除対象として選択できるオブジェクト数
        selectedDeleteObjectIndex = (std::clamp)(selectedDeleteObjectIndex, 0, objectCount - 1);

        std::string preview = "Object " + std::to_string(selectedDeleteObjectIndex); // Comboの現在表示名
        Object3d* previewObject = objects3d_[static_cast<size_t>(selectedDeleteObjectIndex)].get(); // 現在選択中のオブジェクト
        if (previewObject && !previewObject->GetDebugName().empty()) {
            preview += " : " + GetDisplayFileName(previewObject->GetDebugName());
        }

        if (ImGui::BeginCombo("Delete Target", preview.c_str())) {
            for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
                Object3d* object = objects3d_[static_cast<size_t>(objectIndex)].get(); // 表示名を作る対象のオブジェクト
                std::string label = "Object " + std::to_string(objectIndex); // Comboに表示するオブジェクト名
                if (object && !object->GetDebugName().empty()) {
                    label += " : " + GetDisplayFileName(object->GetDebugName());
                }

                const bool isSelected = selectedDeleteObjectIndex == objectIndex; // 現在選択中かどうか
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedDeleteObjectIndex = objectIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Delete Object")) {
            DeleteSceneObject(static_cast<size_t>(selectedDeleteObjectIndex));
            selectedDeleteObjectIndex = (std::min)(selectedDeleteObjectIndex, static_cast<int>(objects3d_.size()) - 1);
        }
    } else {
        ImGui::Text("No objects.");
    }

    DrawCollisionDebugImGui();
#endif
}

/// <summary>
/// ImGuiで衝突判定の状態を表示する。
/// </summary>
void PlayScene::DrawCollisionDebugImGui()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("Collision");
    ImGui::Text("Collider Count: %zu", collisionSystem_.GetColliderCount());
    ImGui::Text("Broad Phase: %s", collisionSystem_.GetSpatialHashEnabled() ? "Spatial Hash" : "Brute Force");
    ImGui::Text("Cell Size: %.2f", collisionSystem_.GetSpatialHashCellSize());
    ImGui::Text("Candidate Pair Count: %zu", collisionSystem_.GetLastCandidatePairCount());
    ImGui::Text("Hit Pair Count: %zu", lastCollisionPairCount_);

    const std::vector<CollisionSystem::CollisionPair>& collisionPairs = collisionSystem_.GetCollisionPairs(); // 表示対象の衝突ペア一覧
    for (size_t pairIndex = 0; pairIndex < collisionPairs.size(); ++pairIndex) {
        const CollisionSystem::CollisionPair& pair = collisionPairs[pairIndex]; // 表示中の衝突ペア
        ImGui::Text("Pair %zu: Object %u <-> Object %u", pairIndex, pair.objectIdA, pair.objectIdB);
    }
#endif
}
