#include "PlayScene.h"
#include "PlaySceneLevelEditorOverlay.h"
#include "PlaySceneLevelEditorHierarchy.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../../engine/3d/Object3d.h"
#include "../../engine/level/LevelLoader.h"
#include "../../engine/level/LevelWriter.h"
#include "../../engine/utility/FileUtility.h"
#include "../../engine/utility/mathUtility.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace MyEngine;
using namespace Math;
using namespace PlaySceneLevelEditorHierarchy;

namespace {
namespace fs = std::filesystem;
constexpr std::array<const char*, 2> kLevelObjectTypeNames = { "MESH", "EMPTY" }; // LevelObjectのtype候補
constexpr std::array<const char*, 3> kLevelColliderTypeNames = { "BOX", "SPHERE", "CAPSULE" }; // LevelObjectのcollider type候補
constexpr std::array<const char*, 3> kModelFileExtensions = { ".obj", ".gltf", ".glb" }; // LevelObjectに指定できるモデル拡張子
constexpr const char* kSceneEditorWindowName = "Scene Editor"; // シーン編集用ImGuiウィンドウ名
constexpr const char* kEffectTypeComboLabel = "Effect Type"; // エフェクト種別選択のImGuiラベル
constexpr const char* kEffectTriggerKeyText = "Trigger Key: R"; // エフェクト開始キーの表示文
constexpr const char* kPlayEffectButtonLabel = "Play Effect"; // エフェクト再生ボタンの表示文
constexpr size_t kMaxLevelUndoHistory = 32; // Level EditorのUndo履歴最大数
constexpr std::array<const char*, 3> kEffectNames = {
    "Dimensional Shatter",
    "Time Reversal",
    "Time Stop",
}; // ImGuiで選択できるエフェクト名

struct ModelLocalBounds {
    Vector3 center; // モデルローカル空間の境界中心
    Vector3 size; // モデルローカル空間の境界サイズ
};

std::unordered_map<std::string, ModelLocalBounds> g_modelLocalBoundsCache; // モデル境界計算のキャッシュ

/// <summary>
/// LevelDataの編集履歴を上限つきで追加する。
/// </summary>
void PushLevelEditHistory(std::vector<LevelData>& history, const LevelData& levelData)
{
    if (history.size() >= kMaxLevelUndoHistory) {
        history.erase(history.begin());
    }
    history.push_back(levelData);
}

/// <summary>
/// LevelDataの履歴を1件戻す、またはやり直す。
/// </summary>
bool RestoreLevelEditHistory(std::vector<LevelData>& sourceHistory, std::vector<LevelData>& destinationHistory, LevelData& currentLevelData)
{
    if (sourceHistory.empty()) {
        return false;
    }

    PushLevelEditHistory(destinationHistory, currentLevelData);
    currentLevelData = sourceHistory.back();
    sourceHistory.pop_back();
    return true;
}

#ifdef USE_IMGUI

/// <summary>
/// std::stringの内容をImGui入力用バッファへ同期する。
/// </summary>
void SyncTextBuffer(const std::string& sourceText, std::array<char, 256>& textBuffer, std::string& bufferedText)
{
    if (bufferedText == sourceText) {
        return;
    }

    textBuffer.fill('\0');
    const size_t copyLength = (std::min)(sourceText.size(), textBuffer.size() - 1); // バッファへコピーする文字数
    std::memcpy(textBuffer.data(), sourceText.data(), copyLength);
    bufferedText = sourceText;
}

/// <summary>
/// std::stringをImGuiで編集する。
/// </summary>
bool EditStringText(const char* label, std::string& value)
{
    std::array<char, 256> textBuffer {}; // 編集中の文字列バッファ
    const size_t copyLength = (std::min)(value.size(), textBuffer.size() - 1); // バッファへコピーする文字数
    std::memcpy(textBuffer.data(), value.data(), copyLength);

    if (!ImGui::InputText(label, textBuffer.data(), textBuffer.size())) {
        return false;
    }

    value = textBuffer.data();
    return true;
}

/// <summary>
/// Vector3をImGuiで編集する。
/// </summary>
bool EditVector3Value(const char* label, Vector3& value)
{
    float elements[3] = { value.x, value.y, value.z }; // ImGuiへ渡すVector3編集値
    constexpr float kDragSpeed = 0.05f; // Transform編集時のドラッグ速度
    if (!ImGui::DragFloat3(label, elements, kDragSpeed)) {
        return false;
    }

    value = { elements[0], elements[1], elements[2] };
    return true;
}

/// <summary>
/// ラジアン保持の回転値を度数法としてImGuiで編集する。
/// </summary>
bool EditRotationDegrees(const char* label, Vector3& rotationRadians)
{
    float rotationDegrees[3] = { // ImGuiへ表示する度数法回転
        MathUtil::RadToDeg(rotationRadians.x),
        MathUtil::RadToDeg(rotationRadians.y),
        MathUtil::RadToDeg(rotationRadians.z),
    };
    constexpr float kRotationDragSpeed = 0.5f; // 回転編集時のドラッグ速度
    if (!ImGui::DragFloat3(label, rotationDegrees, kRotationDragSpeed)) {
        return false;
    }

    rotationRadians = {
        MathUtil::DegToRad(rotationDegrees[0]),
        MathUtil::DegToRad(rotationDegrees[1]),
        MathUtil::DegToRad(rotationDegrees[2]),
    };
    return true;
}

/// <summary>
/// 文字列一覧に同じ値が含まれているか判定する。
/// </summary>
bool ContainsText(const std::vector<std::string>& values, const std::string& value)
{
    return (std::find)(values.begin(), values.end(), value) != values.end();
}

/// <summary>
/// レベルJSON候補をファイル名重複なしで追加する。
/// </summary>
void AppendLevelJsonFiles(const std::string& directoryPath, std::vector<std::string>& levelFiles, std::vector<std::string>& registeredFileNames)
{
    const std::vector<std::string> foundFiles = FileUtility::ListFiles(directoryPath, ".json"); // 指定ディレクトリで見つかったJSON一覧
    for (const std::string& foundFile : foundFiles) {
        const std::string fileName = FileUtility::GetFileName(foundFile); // 重複判定に使うファイル名
        if (ContainsText(registeredFileNames, fileName)) {
            continue;
        }

        registeredFileNames.push_back(fileName);
        levelFiles.push_back(foundFile);
    }
}

/// <summary>
/// Level UIで選択できるレベルJSON一覧を作成する。
/// </summary>
std::vector<std::string> BuildLevelJsonFileList()
{
    std::vector<std::string> levelFiles; // UIで選択できるレベルJSON一覧
    std::vector<std::string> registeredFileNames; // 追加済みJSONファイル名
    const std::string projectLevelDirectory = FileUtility::GetParentDirectory(LevelWriter::ResolveWritableLevelPath("levels/scene.json")); // プロジェクト側levelsディレクトリ

    AppendLevelJsonFiles(projectLevelDirectory, levelFiles, registeredFileNames);
    AppendLevelJsonFiles("resources/levels", levelFiles, registeredFileNames);
    (std::sort)(levelFiles.begin(), levelFiles.end());
    return levelFiles;
}

/// <summary>
/// 指定拡張子がモデルファイルとして扱えるか判定する。
/// </summary>
bool IsModelFileExtension(const std::string& extension)
{
    for (const char* modelExtension : kModelFileExtensions) {
        if (extension == modelExtension) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// モデル候補をリソースモデルディレクトリ基準の相対パスとして追加する。
/// </summary>
void AppendModelFiles(const std::string& modelRootDirectory, std::vector<std::string>& modelFiles)
{
    std::error_code iteratorError; // ディレクトリ走査のエラー受け取り
    const fs::path modelRootPath(modelRootDirectory); // モデル検索ルート
    if (!fs::exists(modelRootPath, iteratorError) || !fs::is_directory(modelRootPath, iteratorError)) {
        return;
    }

    fs::recursive_directory_iterator iterator(modelRootPath, fs::directory_options::skip_permission_denied, iteratorError);
    fs::recursive_directory_iterator end;
    while (!iteratorError && iterator != end) {
        const fs::directory_entry& entry = *iterator; // 走査中のファイル候補
        std::error_code entryError; // ファイル確認のエラー受け取り
        if (entry.is_regular_file(entryError) && IsModelFileExtension(entry.path().extension().generic_string())) {
            std::error_code relativeError; // 相対パス化のエラー受け取り
            const fs::path relativePath = fs::relative(entry.path(), modelRootPath, relativeError); // modelsディレクトリ基準の相対パス
            if (!relativeError) {
                const std::string modelFile = relativePath.generic_string(); // Object3dに渡すモデルファイル名
                if (!ContainsText(modelFiles, modelFile)) {
                    modelFiles.push_back(modelFile);
                }
            }
        }

        iterator.increment(iteratorError);
    }
}

/// <summary>
/// Level UIで選択できるモデルファイル一覧を作成する。
/// </summary>
std::vector<std::string> BuildModelFileList()
{
    std::vector<std::string> modelFiles; // UIで選択できるモデルファイル一覧
    const std::string projectModelDirectory = FileUtility::GetParentDirectory(LevelWriter::ResolveWritableLevelPath("models/__dummy__.obj")); // プロジェクト側modelsディレクトリ

    AppendModelFiles(projectModelDirectory, modelFiles);
    AppendModelFiles("resources/models", modelFiles);
    (std::sort)(modelFiles.begin(), modelFiles.end());
    return modelFiles;
}

struct LevelValidationSummary {
    size_t invalidTypeCount = 0; // 未対応typeの数
    size_t missingModelFileNameCount = 0; // file_nameが空のMESH数
    size_t missingModelAssetCount = 0; // models配下に見つからないMESHファイル数
};

/// <summary>
/// LevelDataの検証情報を再帰的に集計する。
/// </summary>
void AccumulateLevelValidationSummary(const std::vector<LevelObjectData>& objectDataList, const std::vector<std::string>& modelFiles, LevelValidationSummary& summary)
{
    const std::vector<std::string> objectTypeChoices(kLevelObjectTypeNames.begin(), kLevelObjectTypeNames.end()); // 対応済みtype一覧
    for (const LevelObjectData& objectData : objectDataList) {
        if (!ContainsText(objectTypeChoices, objectData.type)) {
            ++summary.invalidTypeCount;
        }

        if (!objectData.enabled) {
            continue;
        }

        if (objectData.type == "MESH") {
            if (objectData.fileName.empty()) {
                ++summary.missingModelFileNameCount;
            } else if (!ContainsText(modelFiles, objectData.fileName)) {
                ++summary.missingModelAssetCount;
            }
        }

        if (!objectData.children.empty()) {
            AccumulateLevelValidationSummary(objectData.children, modelFiles, summary);
        }
    }
}

/// <summary>
/// LevelDataの読み込み後検証情報を作成する。
/// </summary>
LevelValidationSummary BuildLevelValidationSummary(const LevelData& levelData)
{
    LevelValidationSummary summary {}; // 検証結果
    const std::vector<std::string> modelFiles = BuildModelFileList(); // 存在確認に使うモデルファイル一覧
    AccumulateLevelValidationSummary(levelData.objects, modelFiles, summary);
    return summary;
}

/// <summary>
/// 文字列候補をComboで選択する。
/// </summary>
bool EditStringCombo(const char* label, std::string& value, const std::vector<std::string>& choices)
{
    bool edited = false; // Comboで値が変更されたか
    const std::string previewText = value.empty() ? std::string("-") : value; // Comboの現在表示
    if (ImGui::BeginCombo(label, previewText.c_str())) {
        for (const std::string& choice : choices) {
            const bool isSelected = value == choice; // 現在選択中の候補か
            if (ImGui::Selectable(choice.c_str(), isSelected)) {
                value = choice;
                edited = true;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return edited;
}

/// <summary>
/// 読み込み対象のレベルJSONをComboから選択する。
/// </summary>
bool DrawLevelLoadFileSelector(std::string& levelFileName, std::array<char, 256>& levelPathBuffer, std::string& bufferedLevelPath)
{
    bool selected = false; // Comboから読み込み対象が選択されたか
    const std::vector<std::string> levelFiles = BuildLevelJsonFileList(); // Comboへ表示するレベルJSON候補
    const std::string previewText = levelFileName.empty() ? std::string("-") : FileUtility::GetFileName(levelFileName); // Comboの現在表示

    if (ImGui::BeginCombo("Load File Select", previewText.c_str())) {
        for (const std::string& levelFile : levelFiles) {
            const bool isSelected = FileUtility::GetFileName(levelFileName) == FileUtility::GetFileName(levelFile); // 現在選択中のファイルか
            const std::string label = FileUtility::GetFileName(levelFile) + "##" + levelFile; // 表示名とImGui IDを分けたラベル
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                levelFileName = levelFile;
                bufferedLevelPath.clear();
                SyncTextBuffer(levelFileName, levelPathBuffer, bufferedLevelPath);
                selected = true;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (levelFiles.empty()) {
        ImGui::TextDisabled("No level json files found.");
    }

    return selected;
}

/// <summary>
/// モデル頂点のローカルAABBから中心座標とサイズを計算する。
/// </summary>
bool CalculateModelLocalBounds(const std::string& modelFileName, Vector3& outCenter, Vector3& outSize)
{
    if (modelFileName.empty()) {
        return false;
    }

    const auto cachedBoundsIt = g_modelLocalBoundsCache.find(modelFileName); // 既に計算済みのモデル境界
    if (cachedBoundsIt != g_modelLocalBoundsCache.end()) {
        outCenter = cachedBoundsIt->second.center;
        outSize = cachedBoundsIt->second.size;
        return true;
    }

    const Object3d::ModelData modelData = Object3d::LoadModelFile("resources/models", modelFileName); // 境界計算に使うモデルデータ
    if (modelData.vertices.empty()) {
        return false;
    }

    Vector3 minPosition = {
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()
    }; // モデル頂点の最小位置
    Vector3 maxPosition = {
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)()
    }; // モデル頂点の最大位置

    for (const Object3d::VertexData& vertexData : modelData.vertices) {
        const Vector3 vertexPosition = {
            vertexData.position.x,
            vertexData.position.y,
            vertexData.position.z
        }; // AABBへ反映する頂点座標
        minPosition.x = (std::min)(minPosition.x, vertexPosition.x);
        minPosition.y = (std::min)(minPosition.y, vertexPosition.y);
        minPosition.z = (std::min)(minPosition.z, vertexPosition.z);
        maxPosition.x = (std::max)(maxPosition.x, vertexPosition.x);
        maxPosition.y = (std::max)(maxPosition.y, vertexPosition.y);
        maxPosition.z = (std::max)(maxPosition.z, vertexPosition.z);
    }

    outCenter = {
        (minPosition.x + maxPosition.x) * 0.5f,
        (minPosition.y + maxPosition.y) * 0.5f,
        (minPosition.z + maxPosition.z) * 0.5f
    };
    outSize = {
        (std::max)(maxPosition.x - minPosition.x, 0.001f),
        (std::max)(maxPosition.y - minPosition.y, 0.001f),
        (std::max)(maxPosition.z - minPosition.z, 0.001f)
    };

    g_modelLocalBoundsCache[modelFileName] = ModelLocalBounds { outCenter, outSize };
    return true;
}

/// <summary>
/// Level Editorで扱えるコライダー種別か判定する。
/// </summary>
bool IsSupportedLevelColliderType(const std::string& colliderType)
{
    return colliderType == "BOX" || colliderType == "SPHERE" || colliderType == "CAPSULE";
}

/// <summary>
/// コライダーサイズの1成分を編集可能な正の値へ補正する。
/// </summary>
float SanitizeLevelColliderSizeValue(float value)
{
    constexpr float kMinimumColliderSize = 0.001f; // コライダーサイズの最小値
    return (std::max)(std::fabs(value), kMinimumColliderSize);
}

/// <summary>
/// Level Editorで編集したコライダー情報を破綻しない値へ補正する。
/// </summary>
void SanitizeLevelColliderForEdit(LevelColliderData& collider)
{
    if (!IsSupportedLevelColliderType(collider.type)) {
        collider.type = "BOX";
    }

    collider.size.x = SanitizeLevelColliderSizeValue(collider.size.x);
    collider.size.y = SanitizeLevelColliderSizeValue(collider.size.y);
    collider.size.z = SanitizeLevelColliderSizeValue(collider.size.z);

    if (collider.type == "SPHERE") {
        const float diameter = (std::max)((std::max)(collider.size.x, collider.size.y), collider.size.z); // 球の直径
        collider.size = { diameter, diameter, diameter };
    } else if (collider.type == "CAPSULE") {
        const float diameter = (std::max)(collider.size.x, collider.size.z); // Editorカプセルの直径
        collider.size.x = diameter;
        collider.size.y = (std::max)(collider.size.y, diameter);
        collider.size.z = diameter;
    }
}

/// <summary>
/// LevelObjectのBOXコライダーをモデルのローカル境界へ合わせる。
/// </summary>
bool FitLevelColliderToModelBounds(LevelObjectData& objectData)
{
    Vector3 colliderCenter {}; // モデル境界から計算したコライダー中心
    Vector3 colliderSize {}; // モデル境界から計算したコライダーサイズ
    if (!CalculateModelLocalBounds(objectData.fileName, colliderCenter, colliderSize)) {
        return false;
    }

    objectData.collider.enabled = true;
    objectData.collider.type = "BOX";
    objectData.collider.center = colliderCenter;
    objectData.collider.size = colliderSize;
    return true;
}

/// <summary>
/// レベルオブジェクトの表示名を作成する。
/// </summary>
std::string BuildLevelObjectLabel(const LevelObjectData& objectData, size_t objectIndex)
{
    if (!objectData.name.empty()) {
        return objectData.name;
    }
    if (!objectData.fileName.empty()) {
        return objectData.fileName;
    }

    return objectData.type.empty() ? ("Object " + std::to_string(objectIndex)) : objectData.type;
}

/// <summary>
/// レベルオブジェクトの階層操作ボタンを描画する。
/// </summary>
bool DrawLevelObjectOperationButtons(std::vector<LevelObjectData>& objectDataList, std::vector<LevelObjectData>& rootObjectDataList, size_t objectIndex)
{
    if (objectIndex >= objectDataList.size()) {
        return false;
    }

    LevelObjectData& objectData = objectDataList[objectIndex]; // 操作対象のレベルオブジェクト
    if (ImGui::SmallButton("Duplicate")) {
        LevelObjectData duplicatedObject = objectData; // 複製して追加するオブジェクト
        RenameDuplicatedLevelObject(duplicatedObject);
        objectDataList.insert(objectDataList.begin() + static_cast<std::ptrdiff_t>(objectIndex + 1), std::move(duplicatedObject));
        return true;
    }
    ImGui::SameLine();
    if (objectIndex > 0) {
        if (ImGui::SmallButton("Up")) {
            std::swap(objectDataList[objectIndex - 1], objectDataList[objectIndex]);
            return true;
        }
        ImGui::SameLine();
    }
    if (objectIndex + 1 < objectDataList.size()) {
        if (ImGui::SmallButton("Down")) {
            std::swap(objectDataList[objectIndex], objectDataList[objectIndex + 1]);
            return true;
        }
        ImGui::SameLine();
    }
    if (objectIndex > 0) {
        if (ImGui::SmallButton("Child Of Prev")) {
            const Math::Transform parentTransform = objectDataList[objectIndex - 1].transform; // 新しい親のワールドTransform
            LevelObjectData movedObject = std::move(objectDataList[objectIndex]); // 子へ移動するオブジェクト
            movedObject.localTransform = BuildLevelEditorLocalTransformForParent(movedObject.transform, parentTransform);
            objectDataList.erase(objectDataList.begin() + static_cast<std::ptrdiff_t>(objectIndex));
            objectDataList[objectIndex - 1].children.push_back(std::move(movedObject));
            return true;
        }
        ImGui::SameLine();
    }
    if (&objectDataList != &rootObjectDataList) {
        if (ImGui::SmallButton("To Root")) {
            LevelObjectData movedObject = std::move(objectDataList[objectIndex]); // ルートへ移動するオブジェクト
            movedObject.localTransform = movedObject.transform;
            objectDataList.erase(objectDataList.begin() + static_cast<std::ptrdiff_t>(objectIndex));
            rootObjectDataList.push_back(std::move(movedObject));
            return true;
        }
        ImGui::SameLine();
    }
    if (ImGui::SmallButton("Add Empty Child")) {
        objectData.children.push_back(CreateEmptyLevelEditorObject("Empty"));
        return true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete")) {
        objectDataList.erase(objectDataList.begin() + static_cast<std::ptrdiff_t>(objectIndex));
        return true;
    }

    return false;
}

/// <summary>
/// レベルオブジェクトの基本情報をImGuiで編集する。
/// </summary>
bool DrawLevelObjectBasicImGui(LevelObjectData& objectData)
{
    if (!ImGui::CollapsingHeader("Basic", ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }

    bool edited = false; // 基本情報が変更されたか
    const std::vector<std::string> objectTypeChoices(kLevelObjectTypeNames.begin(), kLevelObjectTypeNames.end()); // type選択候補
    bool objectEnabled = objectData.enabled; // ImGuiで編集中のオブジェクト有効フラグ
    if (ImGui::Checkbox("Enabled", &objectEnabled)) {
        objectData.enabled = objectEnabled;
        edited = true;
    }
    edited |= EditStringCombo("Type", objectData.type, objectTypeChoices);
    edited |= EditStringText("Name", objectData.name);
    edited |= EditStringText("Tag", objectData.tag);
    bool spawnPoint = objectData.spawnPoint; // ImGuiで編集中のスポーン地点フラグ
    if (ImGui::Checkbox("Spawn Point", &spawnPoint)) {
        objectData.spawnPoint = spawnPoint;
        edited = true;
    }
    bool cameraStart = objectData.cameraStart; // ImGuiで編集中の開始カメラフラグ
    if (ImGui::Checkbox("Camera Start", &cameraStart)) {
        objectData.cameraStart = cameraStart;
        edited = true;
    }
    edited |= EditStringText("Prefab Source", objectData.prefabSource);
    if (!objectData.prefabSource.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Prefab Link")) {
            objectData.prefabSource.clear();
            edited = true;
        }
    }
    if (objectData.type == "MESH") {
        const std::vector<std::string> modelFiles = BuildModelFileList(); // file_name選択候補
        edited |= EditStringCombo("File Select", objectData.fileName, modelFiles);
        if (modelFiles.empty()) {
            ImGui::TextDisabled("No model files found.");
        }
    }
    edited |= EditStringText("File Text", objectData.fileName);
    ImGui::Text("Children: %zu", objectData.children.size());
    if (objectData.type.empty()) {
        ImGui::TextDisabled("Type is required for reload.");
    }

    return edited;
}

/// <summary>
/// レベルオブジェクトのローカルTransformをImGuiで編集する。
/// </summary>
bool DrawLevelObjectLocalTransformImGui(LevelObjectData& objectData)
{
    if (!ImGui::CollapsingHeader("Transform (Local)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }

    bool edited = false; // Transformが変更されたか
    edited |= EditVector3Value("Translate", objectData.localTransform.translate);
    edited |= EditRotationDegrees("Rotate Deg", objectData.localTransform.rotate);
    edited |= EditVector3Value("Scale", objectData.localTransform.scale);
    return edited;
}

/// <summary>
/// レベルオブジェクトのイベントトリガーをImGuiで編集する。
/// </summary>
bool DrawLevelObjectEventTriggerImGui(LevelObjectData& objectData)
{
    if (!ImGui::CollapsingHeader("Event Trigger")) {
        return false;
    }

    bool edited = false; // イベントトリガーが変更されたか
    bool triggerEnabled = objectData.eventTrigger.enabled; // ImGuiで編集中のイベントトリガー有効フラグ
    if (ImGui::Checkbox("Trigger Enabled", &triggerEnabled)) {
        objectData.eventTrigger.enabled = triggerEnabled;
        edited = true;
    }
    if (objectData.eventTrigger.enabled) {
        edited |= EditStringText("Event Name", objectData.eventTrigger.eventName);
        edited |= EditVector3Value("Trigger Size", objectData.eventTrigger.size);
        objectData.eventTrigger.size.x = SanitizeLevelColliderSizeValue(objectData.eventTrigger.size.x);
        objectData.eventTrigger.size.y = SanitizeLevelColliderSizeValue(objectData.eventTrigger.size.y);
        objectData.eventTrigger.size.z = SanitizeLevelColliderSizeValue(objectData.eventTrigger.size.z);
    }
    return edited;
}

/// <summary>
/// レベルオブジェクトのコライダーをImGuiで編集する。
/// </summary>
bool DrawLevelObjectColliderImGui(LevelObjectData& objectData)
{
    if (!ImGui::CollapsingHeader("Collider")) {
        return false;
    }

    bool edited = false; // コライダーが変更されたか
    bool colliderEnabled = objectData.collider.enabled; // ImGuiで編集中のコライダー有効フラグ
    if (ImGui::Checkbox("Collider Enabled", &colliderEnabled)) {
        objectData.collider.enabled = colliderEnabled;
        if (objectData.collider.enabled) {
            SanitizeLevelColliderForEdit(objectData.collider);
        }
        edited = true;
    }
    if (objectData.type == "MESH" && !objectData.fileName.empty()) {
        if (ImGui::Button("Fit Box To Model")) {
            edited |= FitLevelColliderToModelBounds(objectData);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Uses local model bounds.");
    }
    if (objectData.collider.enabled) {
        SanitizeLevelColliderForEdit(objectData.collider);
        const std::vector<std::string> colliderTypeChoices(kLevelColliderTypeNames.begin(), kLevelColliderTypeNames.end()); // collider type選択候補
        const bool colliderTypeEdited = EditStringCombo("Collider Shape", objectData.collider.type, colliderTypeChoices); // コライダー形状が変更されたか
        edited |= colliderTypeEdited;
        edited |= EditVector3Value("Collider Center", objectData.collider.center);
        const bool colliderSizeEdited = EditVector3Value("Collider Size", objectData.collider.size); // コライダーサイズが変更されたか
        if (colliderTypeEdited || colliderSizeEdited) {
            SanitizeLevelColliderForEdit(objectData.collider);
            edited = true;
        }
    }
    return edited;
}

/// <summary>
/// レベルオブジェクト1件の詳細をImGuiで編集する。
/// </summary>
bool DrawLevelObjectDetail(LevelObjectData& objectData)
{
    bool edited = false; // オブジェクト情報が変更されたか
    edited |= DrawLevelObjectBasicImGui(objectData);
    edited |= DrawLevelObjectLocalTransformImGui(objectData);
    edited |= DrawLevelObjectEventTriggerImGui(objectData);
    edited |= DrawLevelObjectColliderImGui(objectData);
    return edited;
}

/// <summary>
/// レベルオブジェクト階層をImGuiツリーとして編集する。
/// </summary>
bool DrawLevelObjectTree(PlayScene& scene, std::vector<LevelObjectData>& objectDataList, std::vector<LevelObjectData>& rootObjectDataList, const std::string& parentPath, size_t& meshObjectIndex, std::vector<std::string>& selectedObjectPaths)
{
    bool edited = false; // 階層内で変更が発生したか
    for (size_t objectIndex = 0; objectIndex < objectDataList.size(); ++objectIndex) {
        LevelObjectData& objectData = objectDataList[objectIndex]; // 表示対象のレベルオブジェクト
        const std::string objectPath = parentPath + "/" + std::to_string(objectIndex); // ImGui ID用の階層パス
        const std::string objectLabel = BuildLevelObjectLabel(objectData, objectIndex); // ツリーに表示する名前
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth; // ツリー表示の基本フラグ
        if (objectData.children.empty()) {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf;
        }

        const bool isMeshObject = objectData.enabled && objectData.type == "MESH" && !objectData.fileName.empty(); // Gizmo選択と対応するMESHか
        const size_t currentMeshObjectIndex = meshObjectIndex; // このオブジェクトに対応するObject3D番号
        if (isMeshObject) {
            ++meshObjectIndex;
        }

        ImGui::PushID(objectPath.c_str());
        if (IsLevelObjectPathSelected(selectedObjectPaths, objectPath) || (isMeshObject && scene.GetSelectedSceneObjectIndex() == static_cast<int>(currentMeshObjectIndex))) {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }
        bool objectSelected = IsLevelObjectPathSelected(selectedObjectPaths, objectPath); // 複数選択に含まれているか
        if (ImGui::Checkbox("##MultiSelect", &objectSelected)) {
            SetLevelObjectPathSelected(selectedObjectPaths, objectPath, objectSelected);
        }
        ImGui::SameLine();
        const bool treeNodeOpen = ImGui::TreeNodeEx(objectLabel.c_str(), nodeFlags); // このオブジェクトの詳細を開いているか
        if (isMeshObject && ImGui::IsItemClicked()) {
            scene.SelectSceneObjectForEditor(currentMeshObjectIndex);
        }
        ImGui::SameLine();
        if (DrawLevelObjectOperationButtons(objectDataList, rootObjectDataList, objectIndex)) {
            selectedObjectPaths.clear();
            if (treeNodeOpen) {
                ImGui::TreePop();
            }
            ImGui::PopID();
            return true;
        }
        if (!selectedObjectPaths.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Parent Selected Here")) {
                if (MoveSelectedLevelObjectsToParent(rootObjectDataList, selectedObjectPaths, objectPath)) {
                    selectedObjectPaths.clear();
                    if (treeNodeOpen) {
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    return true;
                }
            }
        }
        if (treeNodeOpen) {
            edited |= DrawLevelObjectDetail(objectData);
            if (!objectData.children.empty()) {
                if (ImGui::CollapsingHeader("Children", ImGuiTreeNodeFlags_DefaultOpen)) {
                    edited |= DrawLevelObjectTree(scene, objectData.children, rootObjectDataList, objectPath, meshObjectIndex, selectedObjectPaths);
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    return edited;
}
#endif

}

/// <summary>
/// Scene View画像上へLevel Editor用の編集表示を重ねて描画する。
/// </summary>
void PlayScene::DrawSceneViewOverlay(const Matrix4x4& viewProjectionMatrix, float imageMinX, float imageMinY, float imageWidth, float imageHeight)
{
#ifdef USE_IMGUI
    if (!ctx_.imguiManager || ctx_.imguiManager->GetGizmoTargetMode() != 4) {
        return;
    }

    const int gizmoOperationMode = ctx_.imguiManager->GetGizmoOperationMode(); // 既存ギズモUIで選ばれている操作
    if (gizmoOperationMode != 0 && gizmoOperationMode != 2) {
        return;
    }

    const int selectedObjectIndex = GetSelectedSceneObjectIndex(); // Level Editorと同期しているObject3D番号
    if (selectedObjectIndex < 0) {
        return;
    }

    LevelObjectData* objectData = FindLevelMeshObjectByIndex(static_cast<size_t>(selectedObjectIndex)); // 編集対象のLevelObject
    if (!objectData || !objectData->collider.enabled || !IsSupportedLevelColliderType(objectData->collider.type)) {
        return;
    }

    if (LevelEditorOverlay::DrawColliderOverlay(*objectData, gizmoOperationMode, viewProjectionMatrix, imageMinX, imageMinY, imageWidth, imageHeight)) {
        SanitizeLevelColliderForEdit(objectData->collider);
        RefreshLevelDataSummary();
        ApplyLevelColliderEditToSceneObject(static_cast<size_t>(selectedObjectIndex), *objectData);
        MarkLevelDataDirty("Level collider edited from Scene View. Save hierarchy snapshot.", true);
    }
#else
    (void)viewProjectionMatrix;
    (void)imageMinX;
    (void)imageMinY;
    (void)imageWidth;
    (void)imageHeight;
#endif
}

/// <summary>
/// エフェクト選択と再生操作用のImGuiを描画する。
/// </summary>
void PlayScene::DrawEffectControllerImGui()
{
#ifdef USE_IMGUI
    int selectedEffectIndex = static_cast<int>(selectedEffectType_); // ImGuiで編集中のエフェクト番号
    if (!IsAnyEffectPlaying()
        && ImGui::Combo(
            kEffectTypeComboLabel,
            &selectedEffectIndex,
            kEffectNames.data(),
            static_cast<int>(kEffectNames.size()))) {
        selectedEffectType_ = static_cast<EffectType>(selectedEffectIndex);
    }

    ImGui::Text(kEffectTriggerKeyText);
    if (!IsAnyEffectPlaying()) {
        if (ImGui::Button(kPlayEffectButtonLabel)) {
            StartSelectedEffect();
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button(kPlayEffectButtonLabel);
        ImGui::EndDisabled();
    }
#endif
}

#ifdef USE_IMGUI

struct PlayScene::LevelEditorImGuiState {
    std::array<char, 256> levelPathBuffer {}; // 編集中の読み込みレベルJSONファイル名
    std::array<char, 256> levelPrefabPathBuffer {}; // 編集中のPrefab JSONファイル名
    std::string bufferedLevelPath; // 読み込みバッファへ反映済みのファイル名
    std::string bufferedLevelPrefabPath; // Prefabバッファへ反映済みのファイル名
    std::string levelPrefabFileName = "levels/prefabs/selected_prefab.json"; // Prefab保存と挿入に使うJSONファイル名
    bool autoApplyEditedLevel = true; // LevelData編集時に即シーンへ反映するか
    bool autoSaveEditedLevel = true; // LevelData編集後にJSONへ自動保存するか
    bool pendingLevelAutoSave = false; // 編集完了後に自動保存を実行するか
    bool autoReloadLevelWhenChanged = false; // レベルJSONの更新時に自動再読込するか
    bool autoReloadHasTimestamp = false; // 自動再読込用の更新日時を保持済みか
    fs::file_time_type autoReloadLastWriteTime {}; // 自動再読込で最後に確認した更新日時
    std::vector<LevelData> levelUndoHistory; // LevelData編集のUndo履歴
    std::vector<LevelData> levelRedoHistory; // LevelData編集のRedo履歴
    bool pendingLevelEditHistory = false; // 編集終了待ちのUndo履歴があるか
    LevelData pendingLevelEditSnapshot; // 編集開始時点のLevelData
    int pendingLevelSaveAction = 0; // 未適用保存確認後に実行する処理
    std::vector<std::string> selectedLevelObjectPaths; // 複数選択中のLevelObjectパス
    std::vector<LevelObjectData> levelObjectClipboard; // コピーしたLevelObject群
};

/// <summary>
/// LevelData編集ImGuiからレベル再読み込みを実行する。
/// </summary>
bool PlayScene::ExecuteLevelEditorReload(LevelEditorImGuiState& state)
{
    if (!ReloadLevelSceneObjects()) {
        return false;
    }

    state.levelUndoHistory.clear();
    state.levelRedoHistory.clear();
    state.pendingLevelEditHistory = false;
    state.pendingLevelAutoSave = false;
    state.autoReloadHasTimestamp = false;
    return true;
}

/// <summary>
/// LevelData編集ImGuiからレベル保存を実行する。
/// </summary>
void PlayScene::ExecuteLevelEditorSave()
{
    levelSaveFileName_ = levelDataFileName_;
    SaveLevelSnapshot();
}

/// <summary>
/// LevelData編集ImGuiから保存後再読み込みを実行する。
/// </summary>
void PlayScene::ExecuteLevelEditorSaveAndReload(LevelEditorImGuiState& state)
{
    levelSaveFileName_ = levelDataFileName_;
    if (SaveLevelSnapshot()) {
        state.bufferedLevelPath.clear();
        ExecuteLevelEditorReload(state);
    }
}

/// <summary>
/// LevelData編集ImGuiの読み込み操作を描画する。
/// </summary>
void PlayScene::DrawLevelLoadSectionImGui(LevelEditorImGuiState& state)
{
    if (!ImGui::CollapsingHeader("Load", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    DrawLevelLoadFileSelector(levelDataFileName_, state.levelPathBuffer, state.bufferedLevelPath);
    if (ImGui::InputText("Load / Save File", state.levelPathBuffer.data(), state.levelPathBuffer.size())) {
        levelDataFileName_ = state.levelPathBuffer.data();
        levelSaveFileName_ = levelDataFileName_;
        state.bufferedLevelPath = levelDataFileName_;
    }
    if (ImGui::Button("Reload")) {
        if (levelDirty_) {
            ImGui::OpenPopup("Confirm Reload Level");
        } else {
            ExecuteLevelEditorReload(state);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Reload On File Change", &state.autoReloadLevelWhenChanged);
    if (state.autoReloadLevelWhenChanged) {
        const std::string resolvedLoadFilePath = LevelWriter::ResolveWritableLevelPath(levelDataFileName_); // 自動再読込で監視する実ファイルパス
        std::error_code fileTimeError; // 更新日時取得のエラー受け取り
        const fs::path loadFilePath(resolvedLoadFilePath); // 更新日時を確認するレベルJSONパス
        const fs::file_time_type currentWriteTime = fs::last_write_time(loadFilePath, fileTimeError); // 現在の更新日時
        if (fileTimeError) {
            state.autoReloadHasTimestamp = false;
        } else if (!state.autoReloadHasTimestamp) {
            state.autoReloadLastWriteTime = currentWriteTime;
            state.autoReloadHasTimestamp = true;
        } else if (currentWriteTime != state.autoReloadLastWriteTime) {
            state.autoReloadLastWriteTime = currentWriteTime;
            if (levelDirty_) {
                SetLevelLoadStatus(false, "Auto reload skipped because level has unsaved edits.");
            } else {
                ExecuteLevelEditorReload(state);
            }
        }
    } else {
        state.autoReloadHasTimestamp = false;
    }
}

/// <summary>
/// LevelData編集ImGuiの保存操作を描画する。
/// </summary>
void PlayScene::DrawLevelSaveSectionImGui(LevelEditorImGuiState& state)
{
    if (!ImGui::CollapsingHeader("Save", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    levelSaveFileName_ = levelDataFileName_;
    const std::string resolvedSaveFilePath = LevelWriter::ResolveWritableLevelPath(levelDataFileName_); // 読み書き共通のレベルJSONパス
    ImGui::TextWrapped("Load / Save File: %s", levelDataFileName_.empty() ? "-" : levelDataFileName_.c_str());
    ImGui::TextWrapped("Resolved File: %s", resolvedSaveFilePath.empty() ? "-" : resolvedSaveFilePath.c_str());
    if (ImGui::Button("Save Snapshot")) {
        if (!levelAppliedToScene_) {
            state.pendingLevelSaveAction = 1;
            ImGui::OpenPopup("Confirm Save Not Applied");
        } else {
            ExecuteLevelEditorSave();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save And Reload")) {
        if (!levelAppliedToScene_) {
            state.pendingLevelSaveAction = 2;
            ImGui::OpenPopup("Confirm Save Not Applied");
        } else {
            ExecuteLevelEditorSaveAndReload(state);
        }
    }
    ImGui::Text("Save Result: %s", levelSaveSucceeded_ ? "Success" : "Failed");
    ImGui::Text("Save Message: %s", levelSaveMessage_.empty() ? "-" : levelSaveMessage_.c_str());
}

/// <summary>
/// LevelData編集ImGuiのPrefab操作を描画する。
/// </summary>
void PlayScene::DrawLevelPrefabSectionImGui(LevelEditorImGuiState& state)
{
    if (!ImGui::CollapsingHeader("Prefab")) {
        return;
    }

    if (ImGui::InputText("Prefab File", state.levelPrefabPathBuffer.data(), state.levelPrefabPathBuffer.size())) {
        state.levelPrefabFileName = state.levelPrefabPathBuffer.data();
        state.bufferedLevelPrefabPath = state.levelPrefabFileName;
    }
    ImGui::SeparatorText("Prefab Source");
    const std::string resolvedPrefabFilePath = LevelWriter::ResolveWritableLevelPath(state.levelPrefabFileName); // 実際に使うPrefab JSONパス
    ImGui::TextWrapped("Resolved Prefab File: %s", resolvedPrefabFilePath.empty() ? "-" : resolvedPrefabFilePath.c_str());
    if (ImGui::Button("Save Selected As Prefab")) {
        const bool levelDirtyBeforePrefabSave = levelDirty_; // Prefab保存前のLevelData未保存状態
        const bool levelAppliedBeforePrefabSave = levelAppliedToScene_; // Prefab保存前のシーン反映状態
        const int selectedObjectIndex = GetSelectedSceneObjectIndex(); // Prefab保存対象のMESH番号
        LevelObjectData* selectedObjectData = selectedObjectIndex >= 0 ? FindLevelMeshObjectByIndex(static_cast<size_t>(selectedObjectIndex)) : nullptr; // Prefab保存対象
        if (!selectedObjectData) {
            SetLevelSaveStatus(false, "No selected mesh object to save as prefab.");
        } else {
            LevelData prefabData {}; // Prefabとして保存する単体LevelData
            LevelObjectData prefabObject = *selectedObjectData; // 選択中オブジェクトの保存用コピー
            prefabData.schemaVersion = kCurrentLevelSchemaVersion;
            prefabData.name = "scene";
            prefabObject.localTransform = prefabObject.transform;
            prefabData.objects.push_back(std::move(prefabObject));
            std::string prefabSaveMessage; // Prefab保存結果メッセージ
            const bool prefabSaveSucceeded = LevelWriter::SaveHierarchySnapshot(state.levelPrefabFileName, prefabData, &prefabSaveMessage); // Prefab保存結果
            SetLevelSaveStatus(prefabSaveSucceeded, prefabSaveMessage.empty() ? (prefabSaveSucceeded ? "Saved prefab." : "Prefab save failed.") : prefabSaveMessage);
            levelDirty_ = levelDirtyBeforePrefabSave;
            levelAppliedToScene_ = levelAppliedBeforePrefabSave;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Selected From Prefab Source")) {
        const int selectedObjectIndex = GetSelectedSceneObjectIndex(); // Prefab再読み込み対象のMESH番号
        LevelObjectData* selectedObjectData = selectedObjectIndex >= 0 ? FindLevelMeshObjectByIndex(static_cast<size_t>(selectedObjectIndex)) : nullptr; // Prefab再読み込み対象
        if (!selectedObjectData) {
            SetLevelLoadStatus(false, "No selected mesh object to reload from prefab.");
        } else {
            PushLevelEditHistory(state.levelUndoHistory, levelData_);
            state.levelRedoHistory.clear();
            state.pendingLevelEditHistory = false;
            std::string reloadMessage; // 再読み込み結果メッセージ
            if (ReloadSelectedLevelObjectFromPrefab(*selectedObjectData, &reloadMessage)) {
                LevelLoader::ResolveWorldTransforms(levelData_);
                RefreshLevelDataSummary();
                levelAppliedToScene_ = false;
                MarkLevelDataDirty("Prefab source reloaded. Apply or save hierarchy snapshot.", false);
                SetLevelLoadStatus(true, reloadMessage);
            } else {
                SetLevelLoadStatus(false, reloadMessage);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Insert Prefab To Root")) {
        LevelData prefabData {}; // 読み込んだPrefabデータ
        std::string prefabLoadMessage; // Prefab読み込み結果メッセージ
        if (!LevelLoader::Load(state.levelPrefabFileName, prefabData, &prefabLoadMessage) || prefabData.objects.empty()) {
            SetLevelLoadStatus(false, prefabLoadMessage.empty() ? "Prefab load failed." : prefabLoadMessage);
        } else {
            PushLevelEditHistory(state.levelUndoHistory, levelData_);
            state.levelRedoHistory.clear();
            state.pendingLevelEditHistory = false;
            for (LevelObjectData& prefabObject : prefabData.objects) {
                if (!prefabObject.name.empty()) {
                    prefabObject.name += "_Instance";
                }
                prefabObject.prefabSource = state.levelPrefabFileName;
                levelData_.objects.push_back(std::move(prefabObject));
            }
            LevelLoader::ResolveWorldTransforms(levelData_);
            RefreshLevelDataSummary();
            levelAppliedToScene_ = false;
            MarkLevelDataDirty("Prefab inserted. Apply or save hierarchy snapshot.", false);
            SetLevelLoadStatus(true, "Inserted prefab to root.");
        }
    }
}

/// <summary>
/// LevelData編集ImGuiのシーン反映操作を描画する。
/// </summary>
void PlayScene::DrawLevelSceneApplySectionImGui(LevelEditorImGuiState& state)
{
    if (!ImGui::CollapsingHeader("Scene Apply", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Checkbox("Auto Apply Edited Level", &state.autoApplyEditedLevel);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Save Edited Level", &state.autoSaveEditedLevel);
    if (ImGui::Button("Sync Scene To Level")) {
        PushLevelEditHistory(state.levelUndoHistory, levelData_);
        state.levelRedoHistory.clear();
        state.pendingLevelEditHistory = false;
        if (SyncSceneObjectsToLevelData()) {
            state.pendingLevelAutoSave = state.autoSaveEditedLevel;
            MarkLevelDataDirty("Scene objects synced to level data. Save hierarchy snapshot.", true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply Edited Level")) {
        if (ApplyLevelDataToScene()) {
            state.pendingLevelAutoSave = state.autoSaveEditedLevel;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Preload Level Models")) {
        PreloadLevelModels();
    }
    if (ImGui::Button("Undo Level Edit")) {
        if (RestoreLevelEditHistory(state.levelUndoHistory, state.levelRedoHistory, levelData_)) {
            RefreshLevelDataSummary();
            state.pendingLevelAutoSave = state.autoSaveEditedLevel;
            if (state.autoApplyEditedLevel) {
                ApplyLevelDataToScene();
            }
            MarkLevelDataDirty("Undo level edit. Save hierarchy snapshot.", state.autoApplyEditedLevel);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo Level Edit")) {
        if (RestoreLevelEditHistory(state.levelRedoHistory, state.levelUndoHistory, levelData_)) {
            RefreshLevelDataSummary();
            state.pendingLevelAutoSave = state.autoSaveEditedLevel;
            if (state.autoApplyEditedLevel) {
                ApplyLevelDataToScene();
            }
            MarkLevelDataDirty("Redo level edit. Save hierarchy snapshot.", state.autoApplyEditedLevel);
        }
    }
    ImGui::Text("Undo: %zu  Redo: %zu", state.levelUndoHistory.size(), state.levelRedoHistory.size());
}

/// <summary>
/// LevelData編集ImGuiの状態表示を描画する。
/// </summary>
void PlayScene::DrawLevelStatusSectionImGui()
{
    if (!ImGui::CollapsingHeader("Status")) {
        return;
    }

    ImGui::Text("Load Result: %s", levelLoadSucceeded_ ? "Success" : "Failed");
    ImGui::Text("Load Message: %s", levelLoadMessage_.empty() ? "-" : levelLoadMessage_.c_str());
    ImGui::Text("Scene Name: %s", levelData_.name.empty() ? "-" : levelData_.name.c_str());
    ImGui::Text("Schema Version: %d", levelData_.schemaVersion);
    ImGui::Text("Root Objects: %zu", levelData_.objects.size());
    ImGui::Text("Total Objects: %zu", levelTotalObjectCount_);
    ImGui::Text("Mesh Objects: %zu", levelMeshObjectCount_);
    ImGui::Text("Colliders: %zu", levelColliderObjectCount_);
    ImGui::Text("Disabled: %zu", levelDisabledObjectCount_);
    ImGui::Text("Spawn Points: %zu", levelSpawnPointCount_);
    ImGui::Text("Event Triggers: %zu", levelEventTriggerCount_);
    ImGui::Text("Camera Starts: %zu", levelCameraStartCount_);
    ImGui::Text("Dirty: %s", levelDirty_ ? "Unsaved" : "Saved");
    ImGui::Text("Apply State: %s", levelAppliedToScene_ ? "Applied" : "Not Applied");
    const LevelValidationSummary validationSummary = BuildLevelValidationSummary(levelData_); // LevelDataの検証結果
    ImGui::Text("Invalid Types: %zu", validationSummary.invalidTypeCount);
    ImGui::Text("Missing Model Names: %zu", validationSummary.missingModelFileNameCount);
    ImGui::Text("Missing Model Assets: %zu", validationSummary.missingModelAssetCount);
}

/// <summary>
/// LevelData編集ImGuiの確認ポップアップを描画する。
/// </summary>
void PlayScene::DrawLevelConfirmPopupsImGui(LevelEditorImGuiState& state)
{
    if (ImGui::BeginPopupModal("Confirm Reload Level", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Unsaved level edits will be discarded by reload.");
        if (ImGui::Button("Discard And Reload")) {
            ExecuteLevelEditorReload(state);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Confirm Save Not Applied", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("LevelData has edits that are not applied to the scene. Save anyway?");
        if (ImGui::Button("Save Anyway")) {
            if (state.pendingLevelSaveAction == 1) {
                ExecuteLevelEditorSave();
            } else if (state.pendingLevelSaveAction == 2) {
                ExecuteLevelEditorSaveAndReload(state);
            }
            state.pendingLevelSaveAction = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            state.pendingLevelSaveAction = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/// <summary>
/// LevelData編集ImGuiの階層オブジェクト編集を描画する。
/// </summary>
void PlayScene::DrawLevelObjectsSectionImGui(LevelEditorImGuiState& state)
{
    if (!ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (levelData_.objects.empty()) {
        ImGui::TextDisabled("No level objects loaded.");
        return;
    }

    const LevelData beforeEditLevelData = levelData_; // 編集前のLevelDataスナップショット
    size_t meshObjectIndex = 0; // LevelData内のMESH順に対応するObject3D番号
    LevelLoader::ResolveWorldTransforms(levelData_);
    bool levelEdited = false; // Levelタブでオブジェクト情報が編集されたか
    ImGui::SeparatorText("Selection Tools");
    PruneSelectedLevelObjectPaths(levelData_.objects, state.selectedLevelObjectPaths);
    ImGui::Text("Selected: %zu", state.selectedLevelObjectPaths.size());
    const bool hasSelectedObjects = !state.selectedLevelObjectPaths.empty(); // 複数選択があるか
    if (!hasSelectedObjects) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Copy Selected")) {
        CopySelectedLevelObjects(levelData_.objects, state.selectedLevelObjectPaths, state.levelObjectClipboard);
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate Selected")) {
        levelEdited |= DuplicateSelectedLevelObjects(levelData_.objects, state.selectedLevelObjectPaths);
        state.selectedLevelObjectPaths.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Move Selected To Root")) {
        levelEdited |= MoveSelectedLevelObjectsToRoot(levelData_.objects, state.selectedLevelObjectPaths);
        state.selectedLevelObjectPaths.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        levelEdited |= DeleteSelectedLevelObjects(levelData_.objects, state.selectedLevelObjectPaths);
        state.selectedLevelObjectPaths.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection")) {
        state.selectedLevelObjectPaths.clear();
    }
    if (!hasSelectedObjects) {
        ImGui::EndDisabled();
    }
    const bool hasClipboardObjects = !state.levelObjectClipboard.empty(); // 貼り付け可能なコピーがあるか
    if (!hasClipboardObjects) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Paste To Root")) {
        levelEdited |= PasteLevelObjectsToRoot(levelData_.objects, state.levelObjectClipboard);
    }
    if (!hasClipboardObjects) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::Text("Clipboard: %zu", state.levelObjectClipboard.size());
    ImGui::SeparatorText("Hierarchy");
    levelEdited |= DrawLevelObjectTree(*this, levelData_.objects, levelData_.objects, "root", meshObjectIndex, state.selectedLevelObjectPaths);
    if (!levelEdited) {
        return;
    }

    if (!state.pendingLevelEditHistory) {
        state.pendingLevelEditSnapshot = beforeEditLevelData;
        state.pendingLevelEditHistory = true;
    }
    RefreshLevelDataSummary();
    state.pendingLevelAutoSave = state.autoSaveEditedLevel;
    if (state.autoApplyEditedLevel) {
        ApplyLevelDataToScene();
        MarkLevelDataDirty("Level data edited and applied. Save hierarchy snapshot.", true);
    } else {
        levelAppliedToScene_ = false;
        MarkLevelDataDirty("Level data edited. Apply or save hierarchy snapshot.", false);
    }
}

/// <summary>
/// LevelData編集ImGuiの遅延履歴と自動保存を処理する。
/// </summary>
void PlayScene::FlushLevelEditorDeferredActions(LevelEditorImGuiState& state)
{
    if (state.pendingLevelEditHistory && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        PushLevelEditHistory(state.levelUndoHistory, state.pendingLevelEditSnapshot);
        state.levelRedoHistory.clear();
        state.pendingLevelEditHistory = false;
    }
    if (state.pendingLevelAutoSave && !state.pendingLevelEditHistory && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        if (SaveLevelSnapshot()) {
            state.autoReloadHasTimestamp = false;
        }
        state.pendingLevelAutoSave = false;
    }
}

#endif

/// <summary>
/// ImGuiでレベルJSONの読み込み状態を表示する。
/// </summary>
void PlayScene::DrawLevelDataImGui()
{
#ifdef USE_IMGUI
    static LevelEditorImGuiState levelEditorImGuiState; // LevelData編集ImGuiの一時状態
    SyncTextBuffer(levelDataFileName_, levelEditorImGuiState.levelPathBuffer, levelEditorImGuiState.bufferedLevelPath);
    levelSaveFileName_ = levelDataFileName_;
    SyncTextBuffer(levelEditorImGuiState.levelPrefabFileName, levelEditorImGuiState.levelPrefabPathBuffer, levelEditorImGuiState.bufferedLevelPrefabPath);

    DrawLevelLoadSectionImGui(levelEditorImGuiState);
    DrawLevelSaveSectionImGui(levelEditorImGuiState);
    DrawLevelPrefabSectionImGui(levelEditorImGuiState);
    DrawLevelSceneApplySectionImGui(levelEditorImGuiState);
    DrawLevelStatusSectionImGui();
    DrawLevelConfirmPopupsImGui(levelEditorImGuiState);
    DrawLevelObjectsSectionImGui(levelEditorImGuiState);
    FlushLevelEditorDeferredActions(levelEditorImGuiState);
#endif
}

/// <summary>
/// 選択中エフェクトの詳細ImGuiを描画する。
/// </summary>
void PlayScene::DrawSelectedEffectImGui()
{
#ifdef USE_IMGUI
    switch (selectedEffectType_) {
    case EffectType::DimensionalShatter:
        temporalRiftEffect_.DrawImGui();
        break;
    case EffectType::TimeStop:
        timeStopEffect_.DrawImGui();
        break;
    case EffectType::TimeReversal:
        timeReversalEffect_.DrawImGui();
        break;
    }
#endif
}

/// <summary>
/// シーン編集用のImGuiを描画する。
/// </summary>
void PlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin(kSceneEditorWindowName);

    if (ImGui::BeginTabBar("SceneEditorTabs")) {
        if (ImGui::BeginTabItem("Level")) {
            DrawLevelDataImGui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Objects")) {
            DrawSceneObjectEditImGui();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sprites")) {
            DrawSceneSpriteEditImGui();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Effects")) {
            DrawEffectControllerImGui();
            ImGui::Separator();
            DrawSelectedEffectImGui();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Evaluation")) {
            DrawEvaluationControlImGui();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
#endif
}

