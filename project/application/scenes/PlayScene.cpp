#include "PlayScene.h"
#include "ImGuiManager.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/SkyBox.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/level/LevelLoader.h"
#include "../../engine/level/LevelWriter.h"
#include "../../engine/utility/ResourceResolver.h"
#include "../../engine/utility/mathUtility.h"
#include "../../engine/utility/Logger.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr float kStoppedDeltaTime = 0.0f; // ヒットストップや時間停止中に使用する停止時間
constexpr float kHitStopFinishedThreshold = 0.0f; // ヒットストップが終了したとみなす残り時間
constexpr float kKeyboardDissolveThreshold = 0.45f; // キー切り替え時に見えやすいDissolve閾値
constexpr float kCubeEnvironmentCoefficient = 0.85f; // cubeに適用する環境マップ反射率
constexpr Vector3 kCubeInitialTranslate = { 3.0f, 0.0f, 0.0f }; // cubeの初期配置
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
constexpr const char* kDefaultLevelDataFileName = "levels/scene.json"; // 起動時に読み込むレベルJSON
constexpr const char* kDefaultLevelSaveFileName = "levels/scene.json"; // ImGuiから書き出すレベルJSON

struct SceneModelLoadDesc {
    const char* fileName; // モデルファイル名
    bool loadOnStartup; // PlayScene遷移直後に読み込むか
};

struct PostProcessShortcutDesc {
    uint8_t key; // 切り替えに使用するDirectInputキー
    PostEffectType effectType; // キーに対応するポストエフェクト種別
};

constexpr std::array<SceneModelLoadDesc, 1> kSceneModelLoadDescs = {
    SceneModelLoadDesc { "block/block.obj", true },
}; // シーンで扱うモデルと起動時ロード設定
constexpr std::array<PostProcessShortcutDesc, 11> kPostProcessShortcutDescs = {
    PostProcessShortcutDesc { DIK_0, PostEffectType::Copy },
    PostProcessShortcutDesc { DIK_1, PostEffectType::Grayscale },
    PostProcessShortcutDesc { DIK_2, PostEffectType::Vignette },
    PostProcessShortcutDesc { DIK_3, PostEffectType::BoxFilter },
    PostProcessShortcutDesc { DIK_4, PostEffectType::GaussianFilter },
    PostProcessShortcutDesc { DIK_5, PostEffectType::LuminanceOutline },
    PostProcessShortcutDesc { DIK_6, PostEffectType::DepthOutline },
    PostProcessShortcutDesc { DIK_7, PostEffectType::RadialBlur },
    PostProcessShortcutDesc { DIK_8, PostEffectType::Dissolve },
    PostProcessShortcutDesc { DIK_9, PostEffectType::Random },
    PostProcessShortcutDesc { DIK_Q, PostEffectType::Distortion },
}; // Releaseでも使えるポストエフェクト切り替えキー

/// <summary>
/// レベルJSONの読み書きに使う実ファイルパスを取得する。
/// </summary>
std::string ResolveEditableLevelFilePath(const std::string& filePath)
{
    const std::string targetFilePath = filePath.empty() ? kDefaultLevelDataFileName : filePath; // 解決対象のレベルJSON指定
    return LevelWriter::ResolveWritableLevelPath(targetFilePath);
}

/// <summary>
/// ImGui操作中にゲーム側ショートカットを止める必要があるか判定する。
/// </summary>
bool ShouldBlockGameShortcutInput()
{
#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext()) {
        return false;
    }

    const ImGuiIO& imguiIo = ImGui::GetIO(); // ImGuiの入力取得状態
    return imguiIo.WantCaptureKeyboard || ImGui::IsAnyItemActive();
#else
    return false;
#endif
}

constexpr const char* kEnvironmentMapTextureName = "rostock_laage_airport_4k.dds"; // 環境マップ用DDS名
constexpr const char* kCircleTextureName = "circle.png"; // 円形パーティクルに使用するテクスチャ名
constexpr const char* kCircleFlashTextureName = "circle2.png"; // 発光系スプライトに使用するテクスチャ名
constexpr const char* kGradationLineTextureName = "gradationLine.png"; // リングと円柱に使用するテクスチャ名
constexpr const char* kUvCheckerTextureName = "uvChecker.png"; // 確認用UVテクスチャ名
constexpr const char* kMonsterBallTextureName = "monsterBall.png"; // 確認用ボールテクスチャ名
constexpr std::array<const char*, 5> kSceneTextureNames = {
    kUvCheckerTextureName,
    kMonsterBallTextureName,
    kCircleTextureName,
    kGradationLineTextureName,
    kEnvironmentMapTextureName,
}; // シーン初期化時に読み込むテクスチャ名

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
/// レベルデータのコライダー情報をObject3dへ適用する。
/// </summary>
void ApplyLevelColliderToObject(const LevelObjectData& objectData, Object3d& object3d)
{
    if (!objectData.collider.enabled) {
        return;
    }

    if (objectData.collider.type == "SPHERE") {
        object3d.SetSphereCollider(objectData.collider.center, objectData.collider.size);
        return;
    }
    if (objectData.collider.type == "CAPSULE") {
        object3d.SetCapsuleCollider(objectData.collider.center, objectData.collider.size);
        return;
    }

    object3d.SetBoxCollider(objectData.collider.center, objectData.collider.size);
}

/// <summary>
/// レベルデータ内のオブジェクトが有効か判定する。
/// </summary>
bool IsLevelObjectEnabled(const LevelObjectData& objectData)
{
    return objectData.enabled;
}

/// <summary>
/// レベルデータ内のオブジェクトが生成対象のMeshか判定する。
/// </summary>
bool IsLevelMeshObject(const LevelObjectData& objectData)
{
    return IsLevelObjectEnabled(objectData) && objectData.type == "MESH" && !objectData.fileName.empty();
}

/// <summary>
/// レベルデータ内の開始カメラ指定を実カメラへ反映する。
/// </summary>
bool ApplyFirstLevelCameraStartToCamera(const std::vector<LevelObjectData>& objectDataList, Camera* camera)
{
    if (!camera) {
        return false;
    }

    for (const LevelObjectData& objectData : objectDataList) {
        if (!IsLevelObjectEnabled(objectData)) {
            continue;
        }
        if (objectData.cameraStart) {
            camera->SetRotate(objectData.transform.rotate);
            camera->SetTranslate(objectData.transform.translate);
            camera->Update();
            return true;
        }
        if (!objectData.children.empty() && ApplyFirstLevelCameraStartToCamera(objectData.children, camera)) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// レベルJSONの集計情報。
/// </summary>
struct LevelDataSummary {
    size_t totalObjectCount = 0; // 子階層を含めた総オブジェクト数
    size_t meshObjectCount = 0; // モデル生成対象のMesh数
    size_t colliderObjectCount = 0; // 有効コライダーを持つオブジェクト数
    size_t disabledObjectCount = 0; // 無効化されているオブジェクト数
    size_t spawnPointCount = 0; // スポーン地点として設定された数
    size_t eventTriggerCount = 0; // イベントトリガーとして設定された数
    size_t cameraStartCount = 0; // 開始カメラとして設定された数
};

/// <summary>
/// レベルオブジェクト階層の集計情報を加算する。
/// </summary>
void AccumulateLevelDataSummary(const std::vector<LevelObjectData>& objectDataList, LevelDataSummary& summary)
{
    for (const LevelObjectData& objectData : objectDataList) {
        summary.totalObjectCount++;
        if (!IsLevelObjectEnabled(objectData)) {
            summary.disabledObjectCount++;
            AccumulateLevelDataSummary(objectData.children, summary);
            continue;
        }
        if (IsLevelMeshObject(objectData)) {
            summary.meshObjectCount++;
        }
        if (objectData.collider.enabled) {
            summary.colliderObjectCount++;
        }
        if (objectData.spawnPoint) {
            summary.spawnPointCount++;
        }
        if (objectData.eventTrigger.enabled) {
            summary.eventTriggerCount++;
        }
        if (objectData.cameraStart) {
            summary.cameraStartCount++;
        }
        AccumulateLevelDataSummary(objectData.children, summary);
    }
}

/// <summary>
/// レベルデータ全体の集計情報を作成する。
/// </summary>
LevelDataSummary BuildLevelDataSummary(const LevelData& levelData)
{
    LevelDataSummary summary {}; // 集計結果
    AccumulateLevelDataSummary(levelData.objects, summary);
    return summary;
}

/// <summary>
/// Transformの単位値を作成する。
/// </summary>
Math::Transform CreateIdentityTransformForSceneSync()
{
    return {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };
}

/// <summary>
/// 除算が不安定になる親スケールを避けてローカルスケールを計算する。
/// </summary>
float DivideScaleComponent(float worldScale, float parentScale)
{
    constexpr float kMinimumParentScale = 0.0001f; // 親スケールを除算できる最小値
    if (std::abs(parentScale) < kMinimumParentScale) {
        return worldScale;
    }

    return worldScale / parentScale;
}

/// <summary>
/// ワールドTransformを親基準のローカルTransformへ変換する。
/// </summary>
Math::Transform BuildLocalTransformFromWorld(const Math::Transform& worldTransform, const Math::Transform& parentTransform)
{
    const Math::Matrix4x4 parentMatrix = MathUtil::MakeAffineMatrix(parentTransform.scale, parentTransform.rotate, parentTransform.translate); // 親のワールド行列
    const Math::Matrix4x4 inverseParentMatrix = MathUtil::Inverse(parentMatrix); // 親の逆ワールド行列

    Math::Transform localTransform {}; // 算出した親基準のローカルTransform
    localTransform.scale = {
        DivideScaleComponent(worldTransform.scale.x, parentTransform.scale.x),
        DivideScaleComponent(worldTransform.scale.y, parentTransform.scale.y),
        DivideScaleComponent(worldTransform.scale.z, parentTransform.scale.z)
    };
    localTransform.rotate = {
        worldTransform.rotate.x - parentTransform.rotate.x,
        worldTransform.rotate.y - parentTransform.rotate.y,
        worldTransform.rotate.z - parentTransform.rotate.z
    };
    localTransform.translate = MathUtil::Transform(worldTransform.translate, inverseParentMatrix);
    return localTransform;
}

/// <summary>
/// 現在のカメラTransformを最初の開始カメラLevelObjectへ書き戻す。
/// </summary>
bool SyncFirstLevelCameraStartFromCameraRecursive(std::vector<LevelObjectData>& objectDataList, const Camera* camera, const Math::Transform& parentTransform)
{
    if (!camera) {
        return false;
    }

    for (LevelObjectData& objectData : objectDataList) {
        if (!IsLevelObjectEnabled(objectData)) {
            continue;
        }

        Math::Transform currentParentTransform = objectData.transform; // 子階層のローカル化に使う親Transform
        if (objectData.cameraStart) {
            Math::Transform cameraWorldTransform { // 保存する現在カメラのワールドTransform
                objectData.transform.scale,
                camera->GetRotate(),
                camera->GetTranslate()
            };
            objectData.transform = cameraWorldTransform;
            objectData.localTransform = BuildLocalTransformFromWorld(cameraWorldTransform, parentTransform);
            return true;
        }

        if (!objectData.children.empty() && SyncFirstLevelCameraStartFromCameraRecursive(objectData.children, camera, currentParentTransform)) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// 現在のカメラTransformを開始カメラLevelObjectへ書き戻す。
/// </summary>
bool SyncFirstLevelCameraStartFromCamera(std::vector<LevelObjectData>& objectDataList, const Camera* camera)
{
    return SyncFirstLevelCameraStartFromCameraRecursive(objectDataList, camera, CreateIdentityTransformForSceneSync());
}
/// <summary>
/// Object3dからLevelDataへ保存するMESHオブジェクト情報を作成する。
/// </summary>
LevelObjectData BuildLevelObjectDataFromSceneObject(const Object3d& object3d, size_t objectIndex)
{
    Math::Transform worldTransform { // Object3dが保持する現在のワールドTransform
        object3d.GetScale(),
        object3d.GetRotate(),
        object3d.GetTranslate()
    };

    LevelObjectData objectData {}; // LevelDataへ追加するオブジェクト情報
    objectData.type = "MESH";
    objectData.name = "Object_" + std::to_string(objectIndex);
    objectData.fileName = object3d.GetDebugName();
    objectData.localTransform = worldTransform;
    objectData.transform = worldTransform;
    return objectData;
}

/// <summary>
/// LevelData階層から指定番号のMESHオブジェクトを削除する。
/// </summary>
bool RemoveLevelMeshObjectByIndex(std::vector<LevelObjectData>& objectDataList, size_t targetMeshIndex, size_t& currentMeshIndex)
{
    for (auto objectIterator = objectDataList.begin(); objectIterator != objectDataList.end(); ++objectIterator) {
        if (IsLevelMeshObject(*objectIterator)) {
            if (currentMeshIndex == targetMeshIndex) {
                objectDataList.erase(objectIterator);
                return true;
            }
            ++currentMeshIndex;
        }

        if (RemoveLevelMeshObjectByIndex(objectIterator->children, targetMeshIndex, currentMeshIndex)) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// レベルデータ階層から参照モデルファイル名を重複なしで集める。
/// </summary>
void CollectUniqueLevelModelFiles(const std::vector<LevelObjectData>& objectDataList, std::vector<std::string>& modelFileNames)
{
    for (const LevelObjectData& objectData : objectDataList) {
        if (!IsLevelObjectEnabled(objectData)) {
            continue;
        }
        if (IsLevelMeshObject(objectData) && std::find(modelFileNames.begin(), modelFileNames.end(), objectData.fileName) == modelFileNames.end()) {
            modelFileNames.push_back(objectData.fileName);
        }

        CollectUniqueLevelModelFiles(objectData.children, modelFileNames);
    }
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
/// 指定したテクスチャ名からシーン用スプライトを生成する。
/// </summary>
void PlayScene::CreateSceneSprite(const std::string& textureName)
{
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture(textureName);
    }

    auto sprite = std::make_unique<Sprite>(); // 生成するスプライト
    sprite->SetSpriteId(IssueSpriteId());
    sprite->Initialize(ctx_.spriteCommon, textureName, ctx_.imguiManager);
    sprite->Update();
    sprites_.push_back(std::move(sprite));
    RebuildSpritePointerView();
}

/// <summary>
/// 指定した番号のシーン用スプライトを削除する。
/// </summary>
void PlayScene::DeleteSceneSprite(size_t spriteIndex)
{
    if (sprites_.size() <= spriteIndex) {
        return;
    }

    if (ctx_.imguiManager) {
        const size_t remainingSpriteCount = sprites_.size() - 1; // 削除後に残るスプライト数
        ctx_.imguiManager->NotifySpriteDeleted(spriteIndex, remainingSpriteCount);
    }

    sprites_.erase(sprites_.begin() + spriteIndex);
    RebuildSpritePointerView();
}

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
/// 指定したモデルファイル名からシーン用3Dオブジェクトを生成する
/// </summary>
void PlayScene::CreateSceneObject(const std::string& modelFileName)
{
    auto object3d = std::make_unique<Object3d>(); // 生成する3Dオブジェクト
    object3d->SetObjectId(IssueObjectId());
    object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    object3d->SetModel(modelFileName);
    if (IsAnimationModelFile(modelFileName)) {
        object3d->SetAnimation(modelFileName);
    }
    ApplySceneObjectInitialSettings(*object3d, modelFileName);
    objects3d_.push_back(std::move(object3d));
    RebuildObjectPointerView();
    if (levelData_.objects.empty()) {
        for (size_t sceneObjectIndex = 0; sceneObjectIndex < objects3d_.size(); ++sceneObjectIndex) {
            AppendSceneObjectToLevelData(sceneObjectIndex);
        }
    } else {
        AppendSceneObjectToLevelData(objects3d_.size() - 1);
    }
}

/// <summary>
/// 指定した番号のシーン用3Dオブジェクトを削除する
/// </summary>
void PlayScene::DeleteSceneObject(size_t objectIndex)
{
    if (objects3d_.size() <= objectIndex) {
        return;
    }

    if (ctx_.imguiManager) {
        const size_t remainingObjectCount = objects3d_.size() - 1; // 削除後に残る3Dオブジェクト数
        ctx_.imguiManager->NotifyObjectDeleted(objectIndex, remainingObjectCount);
    }

    RemoveSceneObjectFromLevelData(objectIndex);
    objects3d_.erase(objects3d_.begin() + objectIndex);
    RebuildObjectPointerView();
}

/// <summary>
/// レベルデータ内のオブジェクト一覧からシーン用3Dオブジェクトを生成する。
/// </summary>
void PlayScene::CreateSceneObjectsFromLevelData(const std::vector<LevelObjectData>& objectDataList)
{
    for (const LevelObjectData& objectData : objectDataList) {
        CreateSceneObjectFromLevelData(objectData);
    }
}

/// <summary>
/// レベルデータ内の1オブジェクトからシーン用3Dオブジェクトを生成する。
/// </summary>
void PlayScene::CreateSceneObjectFromLevelData(const LevelObjectData& objectData)
{
    if (!IsLevelObjectEnabled(objectData)) {
        return;
    }

    if (IsLevelMeshObject(objectData)) {
        auto object3d = std::make_unique<Object3d>(); // レベル配置から生成する3Dオブジェクト
        object3d->SetObjectId(IssueObjectId());
        object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        object3d->SetModel(objectData.fileName);
        if (IsAnimationModelFile(objectData.fileName)) {
            object3d->SetAnimation(objectData.fileName);
        }
        ApplySceneObjectInitialSettings(*object3d, objectData.fileName);
        object3d->SetScale(objectData.transform.scale);
        object3d->SetRotate(objectData.transform.rotate);
        object3d->SetTranslate(objectData.transform.translate);
        ApplyLevelColliderToObject(objectData, *object3d);
        objects3d_.push_back(std::move(object3d));
    }

    if (!objectData.children.empty()) {
        CreateSceneObjectsFromLevelData(objectData.children);
    }
}

/// <summary>
/// 既存のシーン用3DオブジェクトへレベルデータのTransformとColliderを反映する。
/// </summary>
bool PlayScene::ApplyLevelDataToExistingSceneObjects(const std::vector<LevelObjectData>& objectDataList, size_t& objectIndex)
{
    for (const LevelObjectData& objectData : objectDataList) {
        if (!IsLevelObjectEnabled(objectData)) {
            continue;
        }
        if (IsLevelMeshObject(objectData)) {
            if (objectIndex >= objects3d_.size() || !objects3d_[objectIndex]) {
                return false;
            }

            Object3d& object3d = *objects3d_[objectIndex]; // レベルデータを反映する既存3Dオブジェクト
            if (object3d.GetDebugName() != objectData.fileName) {
                return false;
            }

            object3d.SetScale(objectData.transform.scale);
            object3d.SetRotate(objectData.transform.rotate);
            object3d.SetTranslate(objectData.transform.translate);
            object3d.ClearCollider();
            ApplyLevelColliderToObject(objectData, object3d);
            ++objectIndex;
        }

        if (!objectData.children.empty() && !ApplyLevelDataToExistingSceneObjects(objectData.children, objectIndex)) {
            return false;
        }
    }

    return true;
}

/// <summary>
/// 現在のシーン用3DオブジェクトのTransformをレベルデータへ書き戻す。
/// </summary>
bool PlayScene::SyncSceneObjectsToLevelData()
{
    LevelLoader::ResolveWorldTransforms(levelData_);
    RefreshLevelDataSummary();

    if (levelMeshObjectCount_ > objects3d_.size()) {
        SetLevelLoadStatus(false, "Level mesh count is larger than scene object count.");
        return false;
    }

    size_t objectIndex = 0; // 書き戻し中の3Dオブジェクト番号
    const bool syncSucceeded = SyncSceneObjectsToLevelDataRecursive(levelData_.objects, objectIndex, CreateIdentityTransformForSceneSync()); // 階層全体の書き戻し結果
    if (!syncSucceeded) {
        SetLevelLoadStatus(false, "Scene object sync failed.");
        return false;
    }
    while (objectIndex < objects3d_.size()) {
        if (!AppendSceneObjectToLevelData(objectIndex)) {
            SetLevelLoadStatus(false, "Scene object append failed.");
            return false;
        }
        ++objectIndex;
    }
    LevelLoader::ResolveWorldTransforms(levelData_);
    RefreshLevelDataSummary();
    SetLevelLoadStatus(true, "Synced scene objects to level data.");
    return true;
}

/// <summary>
/// 現在のレベルデータで参照しているモデルを事前読み込みする。
/// </summary>
bool PlayScene::PreloadLevelModels()
{
    std::vector<std::string> modelFileNames; // 事前読み込み対象のモデルファイル名一覧
    CollectUniqueLevelModelFiles(levelData_.objects, modelFileNames);
    if (modelFileNames.empty()) {
        SetLevelLoadStatus(false, "No mesh model files to preload.");
        return false;
    }

    size_t loadedModelCount = 0; // 読み込みに成功したモデル数
    for (const std::string& modelFileName : modelFileNames) {
        const Object3d::ModelData modelData = Object3d::LoadModelFile("resources/models", modelFileName); // Assimp解析キャッシュへ載せるモデルデータ
        if (!modelData.vertices.empty()) {
            ++loadedModelCount;
        }
    }

    SetLevelLoadStatus(loadedModelCount == modelFileNames.size(), "Preloaded " + std::to_string(loadedModelCount) + "/" + std::to_string(modelFileNames.size()) + " level models.");
    return loadedModelCount == modelFileNames.size();
}

/// <summary>
/// 指定したシーン用3DオブジェクトをLevelDataのルートへ追加する。
/// </summary>
bool PlayScene::AppendSceneObjectToLevelData(size_t objectIndex)
{
    if (objectIndex >= objects3d_.size() || !objects3d_[objectIndex]) {
        return false;
    }

    if (levelData_.name.empty()) {
        levelData_.name = "scene";
    }

    levelData_.objects.push_back(BuildLevelObjectDataFromSceneObject(*objects3d_[objectIndex], objectIndex));
    LevelLoader::ResolveWorldTransforms(levelData_);
    RefreshLevelDataSummary();
    MarkLevelDataDirty("Scene object added to level data. Save hierarchy snapshot.", true);
    return true;
}

/// <summary>
/// 指定したシーン用3Dオブジェクトに対応するLevelData内MESHを削除する。
/// </summary>
bool PlayScene::RemoveSceneObjectFromLevelData(size_t objectIndex)
{
    if (levelData_.objects.empty()) {
        return false;
    }

    size_t currentMeshIndex = 0; // 探索中のMESH番号
    const bool removed = RemoveLevelMeshObjectByIndex(levelData_.objects, objectIndex, currentMeshIndex); // LevelData側の削除結果
    if (!removed) {
        return false;
    }

    LevelLoader::ResolveWorldTransforms(levelData_);
    RefreshLevelDataSummary();
    MarkLevelDataDirty("Scene object removed from level data. Save hierarchy snapshot.", true);
    return true;
}

/// <summary>
/// 既存のシーン用3Dオブジェクトをレベルデータ階層へ順番に書き戻す。
/// </summary>
bool PlayScene::SyncSceneObjectsToLevelDataRecursive(std::vector<LevelObjectData>& objectDataList, size_t& objectIndex, const Math::Transform& parentTransform)
{
    for (LevelObjectData& objectData : objectDataList) {
        if (!IsLevelObjectEnabled(objectData)) {
            continue;
        }

        Math::Transform currentParentTransform = objectData.transform; // 子階層のローカル化に使う親Transform

        if (IsLevelMeshObject(objectData)) {
            if (objectIndex >= objects3d_.size() || !objects3d_[objectIndex]) {
                return false;
            }

            Object3d& object3d = *objects3d_[objectIndex]; // 書き戻し元の既存3Dオブジェクト
            if (object3d.GetDebugName() != objectData.fileName) {
                return false;
            }

            Math::Transform worldTransform { // Object3dが保持している現在のワールドTransform
                object3d.GetScale(),
                object3d.GetRotate(),
                object3d.GetTranslate()
            };
            objectData.transform = worldTransform;
            objectData.localTransform = BuildLocalTransformFromWorld(worldTransform, parentTransform);
            currentParentTransform = worldTransform;
            ++objectIndex;
        }

        if (!objectData.children.empty() && !SyncSceneObjectsToLevelDataRecursive(objectData.children, objectIndex, currentParentTransform)) {
            return false;
        }
    }

    return true;
}

/// <summary>
/// レベルJSONの読み込み状態を記録する。
/// </summary>
void PlayScene::SetLevelLoadStatus(bool succeeded, const std::string& message)
{
    levelLoadSucceeded_ = succeeded;
    levelLoadMessage_ = message;
}

/// <summary>
/// レベルJSONの保存状態を記録する。
/// </summary>
void PlayScene::SetLevelSaveStatus(bool succeeded, const std::string& message)
{
    levelSaveSucceeded_ = succeeded;
    levelSaveMessage_ = message;
    if (succeeded) {
        levelDirty_ = false;
    }
}

/// <summary>
/// LevelDataが未保存状態になったことを記録する。
/// </summary>
void PlayScene::MarkLevelDataDirty(const std::string& message, bool appliedToScene)
{
    levelDirty_ = true;
    levelAppliedToScene_ = appliedToScene;
    SetLevelSaveStatus(false, message);
}

/// <summary>
/// 現在のレベルデータをJSONスナップショットとして保存する。
/// </summary>
bool PlayScene::SaveLevelSnapshot()
{
    levelDataFileName_ = ResolveEditableLevelFilePath(levelDataFileName_);
    levelSaveFileName_ = levelDataFileName_;

    if (levelData_.objects.empty()) {
        SetLevelSaveStatus(false, "No level objects to save.");
        return false;
    }

    if (levelAppliedToScene_) {
        if (!SyncSceneObjectsToLevelData()) {
            SetLevelSaveStatus(false, "Scene object sync failed before save.");
            return false;
        }
        SyncFirstLevelCameraStartFromCamera(levelData_.objects, ctx_.camera);
        LevelLoader::ResolveWorldTransforms(levelData_);
    }

    levelData_.schemaVersion = kCurrentLevelSchemaVersion;

    std::string saveMessage; // レベルJSON保存結果の詳細
    ResourceResolver::ClearCache();
    const bool saveSucceeded = LevelWriter::SaveHierarchySnapshot(levelDataFileName_, levelData_, &saveMessage); // レベルJSON保存結果
    ResourceResolver::ClearCache();
    SetLevelSaveStatus(saveSucceeded, saveMessage.empty() ? (saveSucceeded ? "Saved hierarchy snapshot." : "Save failed.") : saveMessage);
    return saveSucceeded;
}

/// <summary>
/// 現在保持しているレベルデータの集計情報を更新する。
/// </summary>
void PlayScene::RefreshLevelDataSummary()
{
    const LevelDataSummary summary = BuildLevelDataSummary(levelData_); // 現在のレベルデータ集計
    levelTotalObjectCount_ = summary.totalObjectCount;
    levelMeshObjectCount_ = summary.meshObjectCount;
    levelColliderObjectCount_ = summary.colliderObjectCount;
    levelDisabledObjectCount_ = summary.disabledObjectCount;
    levelSpawnPointCount_ = summary.spawnPointCount;
    levelEventTriggerCount_ = summary.eventTriggerCount;
    levelCameraStartCount_ = summary.cameraStartCount;
}

/// <summary>
/// 現在保持しているレベルデータをシーン用3Dオブジェクトへ反映する。
/// </summary>
bool PlayScene::ApplyLevelDataToScene(bool applyCameraStart)
{
    LevelLoader::ResolveWorldTransforms(levelData_);
    if (applyCameraStart) {
        ApplyFirstLevelCameraStartToCamera(levelData_.objects, ctx_.camera);
    }
    RefreshLevelDataSummary();

    if (levelData_.objects.empty() || levelMeshObjectCount_ == 0) {
        objects3d_.clear();
        objectPointerView_.clear();
        nextObjectId_ = 1;
        collisionSystem_.Clear();
        lastCollisionPairCount_ = 0;
        levelAppliedToScene_ = false;
        SetLevelLoadStatus(false, "Level data has no mesh objects.");
        RebuildObjectPointerView();
        return false;
    }

    if (objects3d_.size() == levelMeshObjectCount_) {
        size_t objectIndex = 0; // 反映中の既存3Dオブジェクト番号
        if (ApplyLevelDataToExistingSceneObjects(levelData_.objects, objectIndex) && objectIndex == objects3d_.size()) {
            collisionSystem_.Clear();
            lastCollisionPairCount_ = 0;
            RebuildObjectPointerView();
            levelAppliedToScene_ = true;
            SetLevelLoadStatus(true, "Applied level data.");
            return true;
        }
    }

    objects3d_.clear();
    objectPointerView_.clear();
    nextObjectId_ = 1;
    collisionSystem_.Clear();
    lastCollisionPairCount_ = 0;
    objects3d_.reserve(levelMeshObjectCount_);
    CreateSceneObjectsFromLevelData(levelData_.objects);
    RebuildObjectPointerView();
    levelAppliedToScene_ = true;
    SetLevelLoadStatus(true, "Applied level data.");
    return true;
}

/// <summary>
/// 現在の3DオブジェクトをクリアしてレベルJSONから作り直す。
/// </summary>
bool PlayScene::ReloadLevelSceneObjects()
{
    levelDataFileName_ = ResolveEditableLevelFilePath(levelDataFileName_);
    levelSaveFileName_ = levelDataFileName_;
    ResourceResolver::ClearCache();

    objects3d_.clear();
    objectPointerView_.clear();
    nextObjectId_ = 1;
    collisionSystem_.Clear();
    lastCollisionPairCount_ = 0;

    LevelData loadedLevelData; // 読み込み先の一時レベルデータ
    std::string loadMessage; // レベルJSON読み込み結果の詳細
    const bool loadSucceeded = LevelLoader::Load(levelDataFileName_, loadedLevelData, &loadMessage); // レベルJSON読み込み結果
    if (!loadSucceeded) {
        levelData_ = {};
        levelTotalObjectCount_ = 0;
        levelMeshObjectCount_ = 0;
        levelColliderObjectCount_ = 0;
        levelDisabledObjectCount_ = 0;
        levelSpawnPointCount_ = 0;
        levelEventTriggerCount_ = 0;
        levelCameraStartCount_ = 0;
        SetLevelLoadStatus(false, loadMessage.empty() ? "Load failed." : loadMessage);
        RebuildObjectPointerView();
        return false;
    }

    levelData_ = std::move(loadedLevelData);
    levelDirty_ = false;
    const bool applySucceeded = ApplyLevelDataToScene(true); // 読み込んだレベルデータのシーン反映結果
    SetLevelLoadStatus(applySucceeded, applySucceeded ? (loadMessage.empty() ? "Loaded." : loadMessage) : "Loaded, but no mesh objects.");
    return applySucceeded;
}

/// <summary>
/// シーンで使用する3Dオブジェクトを初期化する。
/// </summary>
void PlayScene::InitializeSceneObjects()
{
    levelDataFileName_ = ResolveEditableLevelFilePath(kDefaultLevelDataFileName);
    levelSaveFileName_ = levelDataFileName_;
    if (ReloadLevelSceneObjects()) {
        return;
    }

    objects3d_.reserve(kSceneModelLoadDescs.size());
    for (const SceneModelLoadDesc& modelDesc : kSceneModelLoadDescs) {
        auto object3d = std::make_unique<Object3d>(); // 作成中の3Dオブジェクト
        object3d->SetObjectId(IssueObjectId());
        object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        object3d->SetModel(modelDesc.fileName);
        ApplySceneObjectInitialSettings(*object3d, modelDesc.fileName);
        objects3d_.push_back(std::move(object3d));
    }
    RebuildObjectPointerView();
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
    InitializeSceneObjects();
    if (!kUsePostEffectPreviewScene) {
        InitializeParticleObjects();
        InitializeParticleEffects();
        InitializeTemporalEffectSprites();
    }
    InitializePostProcessTargets();
}

/// <summary>
/// シーンが保持している表示用オブジェクトを解放する。
/// </summary>
void PlayScene::ReleaseSceneObjects()
{
    sprites_.clear();
    spritePointerView_.clear();
    nextSpriteId_ = 1;
    objects3d_.clear();
    objectPointerView_.clear();
    nextObjectId_ = 1;
    collisionSystem_.Clear();
    lastCollisionPairCount_ = 0;
    levelData_ = {};
    levelLoadSucceeded_ = false;
    levelLoadMessage_.clear();
    levelSaveSucceeded_ = false;
    levelSaveMessage_.clear();
    levelDirty_ = false;
    levelAppliedToScene_ = false;
    levelTotalObjectCount_ = 0;
    levelMeshObjectCount_ = 0;
    levelColliderObjectCount_ = 0;
    levelDisabledObjectCount_ = 0;
    levelSpawnPointCount_ = 0;
    levelEventTriggerCount_ = 0;
    levelCameraStartCount_ = 0;
    particleEmitterPointerView_.clear();
    nextParticleEmitterId_ = 1;
    temporalAfterimageSprites_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
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
    ClearSceneParticles();
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
    if (ShouldBlockGameShortcutInput()) {
        return;
    }

    InputManager* inputManager = InputManager::GetInstance(); // エフェクト開始入力を取得する入力管理
    if (inputManager
        && inputManager->IsKeyJustPressed(DIK_R)
        && !IsAnyEffectPlaying()) {
        StartSelectedEffect();
    }
}

/// <summary>
/// ポストエフェクト切り替え入力を処理する。
/// </summary>
void PlayScene::HandlePostProcessShortcutInput()
{
    if (ShouldBlockGameShortcutInput()) {
        return;
    }

    InputManager* inputManager = InputManager::GetInstance(); // ポストエフェクト切り替え入力を取得する入力管理
    if (!inputManager) {
        return;
    }

    for (const PostProcessShortcutDesc& shortcutDesc : kPostProcessShortcutDescs) {
        if (!inputManager->IsKeyJustPressed(shortcutDesc.key)) {
            continue;
        }

        ApplyPostProcessShortcut(shortcutDesc.effectType);
        return;
    }
}

/// <summary>
/// キー入力で選択されたポストエフェクトを適用する。
/// </summary>
void PlayScene::ApplyPostProcessShortcut(PostEffectType effectType)
{
    if (!IsValidPostEffectType(effectType)) {
        return;
    }

    postProcess_.SetEnabled(true);
    postProcess_.SetEffectType(effectType);
    if (effectType == PostEffectType::Dissolve) {
        postProcess_.SetDissolveThreshold(kKeyboardDissolveThreshold);
    }

    Logger::Log(std::string("Post effect changed: ") + GetPostEffectTypeName(effectType) + "\n");
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
    if (!kUsePostEffectPreviewScene) {
        HandleEffectStartInput();
    }
    HandlePostProcessShortcutInput();
    HandleEvaluationAnimationInput();
    HandleSkinningModelControlInput(dt);
    if (kUsePostEffectPreviewScene) {
        postProcess_.Update(dt);
    } else {
        UpdateTemporalEffects(dt);
    }

    if (ctx_.camera) {
        ctx_.camera->Update();
    }
    if (!kUsePostEffectPreviewScene) {
        UpdatePostEffectCenters();
        UpdateParticleSystems(dt);
    }

    UpdateSceneObjects(dt);
    UpdateSceneCollisions();

    if (!kUsePostEffectPreviewScene) {
        UpdateAfterimageSprites();
        UpdateTimeReversalSprites();
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
/// シーン内3Dオブジェクトの衝突判定を更新する。
/// </summary>
void PlayScene::UpdateSceneCollisions()
{
    collisionSystem_.Clear();

    for (const auto& object3d : objects3d_) {
        if (!object3d || !object3d->HasCollider()) {
            continue;
        }

        collisionSystem_.RegisterCollider(object3d->GetObjectId(), &object3d->GetCollider(), object3d.get());
    }

    collisionSystem_.Update();
    lastCollisionPairCount_ = collisionSystem_.GetCollisionPairs().size();
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
    DrawDebugLines3D();
    DrawSprites();
    DrawDebugLines2D();
}

/// <summary>
/// シーンに入るときの処理
/// </summary>
void PlayScene::OnEnter()
{
    std::cout << "PlayScene OnEnter\n";
    if (!kUsePostEffectPreviewScene) {
        InitializeParticleManager();
    }
}

/// <summary>
/// シーンから出るときの処理
/// </summary>
void PlayScene::OnExit() { std::cout << "PlayScene OnExit\n"; }

/// <summary>
/// 所有中のスプライトから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildSpritePointerView()
{
    spritePointerView_.clear();
    spritePointerView_.reserve(sprites_.size());
    for (auto& sprite : sprites_) {
        if (sprite) {
            spritePointerView_.push_back(sprite.get());
        }
    }
}

/// <summary>
/// 所有中の3Dオブジェクトから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildObjectPointerView()
{
    objectPointerView_.clear();
    objectPointerView_.reserve(objects3d_.size());
    for (auto& object : objects3d_) {
        if (object) {
            objectPointerView_.push_back(object.get());
        }
    }
}

/// <summary>
/// 所有中のパーティクルエミッターから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildParticleEmitterPointerView()
{
    particleEmitterPointerView_.clear();
    particleEmitterPointerView_.reserve(3);
    particleEmitterPointerView_.push_back(&pmEmitter_);
    particleEmitterPointerView_.push_back(&ringEmitter_);
    particleEmitterPointerView_.push_back(&cylinderEmitter_);
}

/// <summary>
/// 次に生成するスプライトへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueSpriteId()
{
    const uint32_t issuedSpriteId = nextSpriteId_; // 今回割り当てるスプライトID
    nextSpriteId_++;
    return issuedSpriteId;
}

/// <summary>
/// 次に生成する3Dオブジェクトへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueObjectId()
{
    const uint32_t issuedObjectId = nextObjectId_; // 今回割り当てる3DオブジェクトID
    nextObjectId_++;
    return issuedObjectId;
}

/// <summary>
/// 次に生成するパーティクルエミッターへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueParticleEmitterId()
{
    const uint32_t issuedEmitterId = nextParticleEmitterId_; // 今回割り当てるパーティクルエミッターID
    nextParticleEmitterId_++;
    return issuedEmitterId;
}

/// <summary>
/// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) {
        return;
    }

    RebuildObjectPointerView();
    out->clear();
    out->reserve(objectPointerView_.size());
    out->insert(out->end(), objectPointerView_.begin(), objectPointerView_.end());
}

/// <summary>
/// Level EditorとGizmoで共有する選択中3Dオブジェクト番号を取得する。
/// </summary>
int PlayScene::GetSelectedSceneObjectIndex() const
{
    return ctx_.imguiManager ? ctx_.imguiManager->GetSelectedObjectIndex() : -1;
}

/// <summary>
/// Level EditorからGizmo対象の3Dオブジェクトを選択する。
/// </summary>
void PlayScene::SelectSceneObjectForEditor(size_t objectIndex)
{
    if (!ctx_.imguiManager || objectIndex >= objects3d_.size()) {
        return;
    }

    ctx_.imguiManager->SetSelectedObjectIndex(static_cast<int>(objectIndex));
}

/// <summary>
/// MESH順の番号からLevelData内のオブジェクトを再帰的に取得する。
/// </summary>
LevelObjectData* PlayScene::FindLevelMeshObjectByIndexRecursive(std::vector<LevelObjectData>& objectDataList, size_t targetMeshIndex, size_t& currentMeshIndex)
{
    for (LevelObjectData& objectData : objectDataList) {
        if (IsLevelMeshObject(objectData)) {
            if (currentMeshIndex == targetMeshIndex) {
                return &objectData;
            }
            ++currentMeshIndex;
        }

        if (!objectData.children.empty()) {
            LevelObjectData* childObjectData = FindLevelMeshObjectByIndexRecursive(objectData.children, targetMeshIndex, currentMeshIndex); // 子階層で見つかったMESH
            if (childObjectData) {
                return childObjectData;
            }
        }
    }

    return nullptr;
}

/// <summary>
/// MESH順の番号からLevelData内のオブジェクトを取得する。
/// </summary>
LevelObjectData* PlayScene::FindLevelMeshObjectByIndex(size_t objectIndex)
{
    size_t currentMeshIndex = 0; // 探索中のMESH番号
    return FindLevelMeshObjectByIndexRecursive(levelData_.objects, objectIndex, currentMeshIndex);
}

/// <summary>
/// LevelDataの選択コライダーを対応するObject3dへ反映する。
/// </summary>
void PlayScene::ApplyLevelColliderEditToSceneObject(size_t objectIndex, const LevelObjectData& objectData)
{
    if (objectIndex >= objects3d_.size() || !objects3d_[objectIndex]) {
        return;
    }

    Object3d& object3d = *objects3d_[objectIndex]; // コライダー反映先の3Dオブジェクト
    if (!objectData.collider.enabled) {
        object3d.ClearCollider();
        return;
    }

    if (objectData.collider.type == "SPHERE") {
        object3d.SetSphereCollider(objectData.collider.center, objectData.collider.size);
        return;
    }
    if (objectData.collider.type == "CAPSULE") {
        object3d.SetCapsuleCollider(objectData.collider.center, objectData.collider.size);
        return;
    }

    object3d.SetBoxCollider(objectData.collider.center, objectData.collider.size);
}

/// <summary>
/// Scene ViewのGizmoで編集された3DオブジェクトのTransformをLevelDataへ書き戻す。
/// </summary>
void PlayScene::NotifyObjectTransformEdited(size_t objectIndex)
{
    if (levelData_.objects.empty() || objectIndex >= objects3d_.size()) {
        return;
    }

    const bool syncSucceeded = SyncSceneObjectsToLevelData(); // Gizmo編集後のTransform書き戻し結果
    if (syncSucceeded) {
        MarkLevelDataDirty("Scene View gizmo synced to level data. Save hierarchy snapshot.", true);
    }
}

/// <summary>
/// シーンが所有するパーティクルエミッターポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillParticleEmitterPointers(std::vector<::ParticleEmitter*>* out)
{
    if (!out) {
        return;
    }

    RebuildParticleEmitterPointerView();
    out->clear();
    out->reserve(particleEmitterPointerView_.size());
    out->insert(out->end(), particleEmitterPointerView_.begin(), particleEmitterPointerView_.end());
}

/// <summary>
/// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out) {
        return;
    }

    RebuildSpritePointerView();
    out->clear();
    out->reserve(spritePointerView_.size());
    out->insert(out->end(), spritePointerView_.begin(), spritePointerView_.end());
}
