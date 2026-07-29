#include "PlayScene.h"
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
/// Level Editor用の単位Transformを作成する。
/// </summary>
Math::Transform CreateLevelEditorIdentityTransform()
{
    return {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };
}

/// <summary>
/// 親スケールが小さすぎる場合も破綻しないようにスケール成分を割る。
/// </summary>
float DivideLevelEditorScaleComponent(float worldScale, float parentScale)
{
    constexpr float kMinimumParentScale = 0.0001f; // 親スケールを除算できる最小値
    if (std::fabs(parentScale) < kMinimumParentScale) {
        return worldScale;
    }

    return worldScale / parentScale;
}

/// <summary>
/// ワールドTransformを新しい親基準のローカルTransformへ変換する。
/// </summary>
Math::Transform BuildLevelEditorLocalTransformForParent(const Math::Transform& worldTransform, const Math::Transform& parentTransform)
{
    const Math::Matrix4x4 parentMatrix = MathUtil::MakeAffineMatrix(parentTransform.scale, parentTransform.rotate, parentTransform.translate); // 新しい親のワールド行列
    const Math::Matrix4x4 inverseParentMatrix = MathUtil::Inverse(parentMatrix); // 新しい親の逆ワールド行列

    Math::Transform localTransform {}; // 新しい親基準のローカルTransform
    localTransform.scale = {
        DivideLevelEditorScaleComponent(worldTransform.scale.x, parentTransform.scale.x),
        DivideLevelEditorScaleComponent(worldTransform.scale.y, parentTransform.scale.y),
        DivideLevelEditorScaleComponent(worldTransform.scale.z, parentTransform.scale.z)
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
/// Level Editorで追加する空の子オブジェクトを作成する。
/// </summary>
LevelObjectData CreateEmptyLevelEditorObject(const std::string& objectName)
{
    LevelObjectData objectData {}; // 追加する空オブジェクト
    objectData.type = "EMPTY";
    objectData.name = objectName;
    objectData.localTransform = CreateLevelEditorIdentityTransform();
    objectData.transform = objectData.localTransform;
    return objectData;
}

/// <summary>
/// 複製したレベルオブジェクトの名前を識別しやすい名前へ変更する。
/// </summary>
void RenameDuplicatedLevelObject(LevelObjectData& objectData)
{
    if (objectData.name.empty()) {
        objectData.name = "Object_Copy";
        return;
    }

    objectData.name += "_Copy";
}

/// <summary>
/// LevelObjectの階層パスを番号列へ変換する。
/// </summary>
bool ParseLevelObjectPath(const std::string& objectPath, std::vector<size_t>& outIndices)
{
    constexpr size_t kRootPrefixLength = 5; // "root/" の文字数
    outIndices.clear();
    if (objectPath.rfind("root/", 0) != 0 || objectPath.size() <= kRootPrefixLength) {
        return false;
    }

    size_t segmentStart = kRootPrefixLength; // 現在解析中の区切り開始位置
    while (segmentStart < objectPath.size()) {
        const size_t segmentEnd = objectPath.find('/', segmentStart); // 現在の区切り終了位置
        const std::string segment = objectPath.substr(segmentStart, segmentEnd == std::string::npos ? std::string::npos : segmentEnd - segmentStart); // パス内の番号文字列
        if (segment.empty()) {
            return false;
        }
        try {
            outIndices.push_back(static_cast<size_t>(std::stoull(segment)));
        } catch (...) {
            outIndices.clear();
            return false;
        }
        if (segmentEnd == std::string::npos) {
            break;
        }
        segmentStart = segmentEnd + 1;
    }

    return !outIndices.empty();
}

/// <summary>
/// LevelObjectの親配列と自身の番号を階層パスから取得する。
/// </summary>
bool FindLevelObjectParentListByPath(std::vector<LevelObjectData>& rootObjectDataList, const std::string& objectPath, std::vector<LevelObjectData>** outParentList, size_t* outObjectIndex)
{
    std::vector<size_t> indices; // パスから取り出した階層番号
    if (!ParseLevelObjectPath(objectPath, indices)) {
        return false;
    }

    std::vector<LevelObjectData>* parentList = &rootObjectDataList; // 現在参照中の兄弟配列
    for (size_t depth = 0; depth + 1 < indices.size(); ++depth) {
        const size_t childIndex = indices[depth]; // 次に降りる子番号
        if (childIndex >= parentList->size()) {
            return false;
        }
        parentList = &(*parentList)[childIndex].children;
    }

    const size_t objectIndex = indices.back(); // 操作対象の兄弟内番号
    if (objectIndex >= parentList->size()) {
        return false;
    }

    *outParentList = parentList;
    *outObjectIndex = objectIndex;
    return true;
}

/// <summary>
/// 階層パスからLevelObjectを取得する。
/// </summary>
LevelObjectData* FindLevelObjectByPath(std::vector<LevelObjectData>& rootObjectDataList, const std::string& objectPath)
{
    std::vector<LevelObjectData>* parentList = nullptr; // 親の兄弟配列
    size_t objectIndex = 0; // 兄弟内番号
    if (!FindLevelObjectParentListByPath(rootObjectDataList, objectPath, &parentList, &objectIndex)) {
        return nullptr;
    }
    return &(*parentList)[objectIndex];
}
/// <summary>
/// LevelObjectの階層パスが現在のLevelData上に存在するか判定する。
/// </summary>
bool DoesLevelObjectPathExist(std::vector<LevelObjectData>& rootObjectDataList, const std::string& objectPath)
{
    std::vector<LevelObjectData>* parentList = nullptr; // 親の兄弟配列
    size_t objectIndex = 0; // 兄弟内番号
    return FindLevelObjectParentListByPath(rootObjectDataList, objectPath, &parentList, &objectIndex);
}

/// <summary>
/// 選択済みLevelObjectパスか判定する。
/// </summary>
bool IsLevelObjectPathSelected(const std::vector<std::string>& selectedObjectPaths, const std::string& objectPath)
{
    return std::find(selectedObjectPaths.begin(), selectedObjectPaths.end(), objectPath) != selectedObjectPaths.end();
}

/// <summary>
/// LevelObjectの複数選択状態を切り替える。
/// </summary>
void SetLevelObjectPathSelected(std::vector<std::string>& selectedObjectPaths, const std::string& objectPath, bool selected)
{
    const auto pathIterator = std::find(selectedObjectPaths.begin(), selectedObjectPaths.end(), objectPath); // 既存選択の位置
    if (selected) {
        if (pathIterator == selectedObjectPaths.end()) {
            selectedObjectPaths.push_back(objectPath);
        }
    } else if (pathIterator != selectedObjectPaths.end()) {
        selectedObjectPaths.erase(pathIterator);
    }
}

/// <summary>
/// 存在しなくなったLevelObjectの選択パスを取り除く。
/// </summary>
void PruneSelectedLevelObjectPaths(std::vector<LevelObjectData>& rootObjectDataList, std::vector<std::string>& selectedObjectPaths)
{
    selectedObjectPaths.erase(
        std::remove_if(
            selectedObjectPaths.begin(),
            selectedObjectPaths.end(),
            [&](const std::string& objectPath) { return !DoesLevelObjectPathExist(rootObjectDataList, objectPath); }),
        selectedObjectPaths.end());
}

/// <summary>
/// 片方のLevelObjectパスがもう片方の祖先か判定する。
/// </summary>
bool IsLevelObjectPathAncestorOf(const std::string& ancestorPath, const std::string& descendantPath)
{
    return descendantPath.size() > ancestorPath.size()
        && descendantPath.compare(0, ancestorPath.size(), ancestorPath) == 0
        && descendantPath[ancestorPath.size()] == '/';
}
/// <summary>
/// 祖先が選択済みのLevelObjectパスを操作対象から除外する。
/// </summary>
std::vector<std::string> BuildIndependentLevelObjectSelection(const std::vector<std::string>& selectedObjectPaths)
{
    std::vector<std::string> independentPaths; // 実際に操作する選択パス
    for (const std::string& objectPath : selectedObjectPaths) {
        bool hasSelectedAncestor = false; // 選択済み祖先を持つか
        for (const std::string& otherPath : selectedObjectPaths) {
            if (IsLevelObjectPathAncestorOf(otherPath, objectPath)) {
                hasSelectedAncestor = true;
                break;
            }
        }
        if (!hasSelectedAncestor) {
            independentPaths.push_back(objectPath);
        }
    }
    return independentPaths;
}

/// <summary>
/// 階層編集時に深い階層・後ろの兄弟から処理する順序へ並べる。
/// </summary>
bool CompareLevelObjectPathForReverseEdit(const std::string& lhs, const std::string& rhs)
{
    std::vector<size_t> lhsIndices; // 左辺パスの階層番号
    std::vector<size_t> rhsIndices; // 右辺パスの階層番号
    ParseLevelObjectPath(lhs, lhsIndices);
    ParseLevelObjectPath(rhs, rhsIndices);
    if (lhsIndices.size() != rhsIndices.size()) {
        return lhsIndices.size() > rhsIndices.size();
    }
    for (size_t index = 0; index < lhsIndices.size() && index < rhsIndices.size(); ++index) {
        if (lhsIndices[index] != rhsIndices[index]) {
            return lhsIndices[index] > rhsIndices[index];
        }
    }
    return lhs > rhs;
}

/// <summary>
/// 指定パスのLevelObjectを直後へ複製する。
/// </summary>
bool DuplicateLevelObjectByPath(std::vector<LevelObjectData>& rootObjectDataList, const std::string& objectPath)
{
    std::vector<LevelObjectData>* parentList = nullptr; // 親の兄弟配列
    size_t objectIndex = 0; // 兄弟内番号
    if (!FindLevelObjectParentListByPath(rootObjectDataList, objectPath, &parentList, &objectIndex)) {
        return false;
    }

    LevelObjectData duplicatedObject = (*parentList)[objectIndex]; // 複製して追加するオブジェクト
    RenameDuplicatedLevelObject(duplicatedObject);
    parentList->insert(parentList->begin() + static_cast<std::ptrdiff_t>(objectIndex + 1), std::move(duplicatedObject));
    return true;
}

/// <summary>
/// 指定パスのLevelObjectを削除する。
/// </summary>
bool DeleteLevelObjectByPath(std::vector<LevelObjectData>& rootObjectDataList, const std::string& objectPath)
{
    std::vector<LevelObjectData>* parentList = nullptr; // 親の兄弟配列
    size_t objectIndex = 0; // 兄弟内番号
    if (!FindLevelObjectParentListByPath(rootObjectDataList, objectPath, &parentList, &objectIndex)) {
        return false;
    }

    parentList->erase(parentList->begin() + static_cast<std::ptrdiff_t>(objectIndex));
    return true;
}

/// <summary>
/// 選択中LevelObjectをまとめて複製する。
/// </summary>
bool DuplicateSelectedLevelObjects(std::vector<LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths)
{
    std::vector<std::string> targetPaths = BuildIndependentLevelObjectSelection(selectedObjectPaths); // 実際に複製するパス
    std::sort(targetPaths.begin(), targetPaths.end(), CompareLevelObjectPathForReverseEdit);

    bool edited = false; // 複製が発生したか
    for (const std::string& objectPath : targetPaths) {
        edited |= DuplicateLevelObjectByPath(rootObjectDataList, objectPath);
    }
    return edited;
}

/// <summary>
/// 選択中LevelObjectをまとめて削除する。
/// </summary>
bool DeleteSelectedLevelObjects(std::vector<LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths)
{
    std::vector<std::string> targetPaths = BuildIndependentLevelObjectSelection(selectedObjectPaths); // 実際に削除するパス
    std::sort(targetPaths.begin(), targetPaths.end(), CompareLevelObjectPathForReverseEdit);

    bool edited = false; // 削除が発生したか
    for (const std::string& objectPath : targetPaths) {
        edited |= DeleteLevelObjectByPath(rootObjectDataList, objectPath);
    }
    return edited;
}
/// <summary>
/// 選択中LevelObjectをクリップボードへコピーする。
/// </summary>
bool CopySelectedLevelObjects(std::vector<LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths, std::vector<LevelObjectData>& clipboard)
{
    clipboard.clear();
    const std::vector<std::string> targetPaths = BuildIndependentLevelObjectSelection(selectedObjectPaths); // 実際にコピーするパス
    for (const std::string& objectPath : targetPaths) {
        LevelObjectData* objectData = FindLevelObjectByPath(rootObjectDataList, objectPath); // コピー元オブジェクト
        if (objectData) {
            clipboard.push_back(*objectData);
        }
    }
    return !clipboard.empty();
}

/// <summary>
/// クリップボード内のLevelObjectをルートへ貼り付ける。
/// </summary>
bool PasteLevelObjectsToRoot(std::vector<LevelObjectData>& rootObjectDataList, const std::vector<LevelObjectData>& clipboard)
{
    if (clipboard.empty()) {
        return false;
    }

    for (LevelObjectData pastedObject : clipboard) {
        RenameDuplicatedLevelObject(pastedObject);
        pastedObject.localTransform = pastedObject.transform;
        rootObjectDataList.push_back(std::move(pastedObject));
    }
    return true;
}

/// <summary>
/// 選択中LevelObjectをルートへ移動する。
/// </summary>
bool MoveSelectedLevelObjectsToRoot(std::vector<LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths)
{
    std::vector<std::string> targetPaths = BuildIndependentLevelObjectSelection(selectedObjectPaths); // 実際に移動するパス
    std::sort(targetPaths.begin(), targetPaths.end(), CompareLevelObjectPathForReverseEdit);

    bool edited = false; // 移動が発生したか
    for (const std::string& objectPath : targetPaths) {
        std::vector<LevelObjectData>* parentList = nullptr; // 親の兄弟配列
        size_t objectIndex = 0; // 兄弟内番号
        if (!FindLevelObjectParentListByPath(rootObjectDataList, objectPath, &parentList, &objectIndex) || parentList == &rootObjectDataList) {
            continue;
        }
        LevelObjectData movedObject = std::move((*parentList)[objectIndex]); // ルートへ移動するオブジェクト
        movedObject.localTransform = movedObject.transform;
        parentList->erase(parentList->begin() + static_cast<std::ptrdiff_t>(objectIndex));
        rootObjectDataList.push_back(std::move(movedObject));
        edited = true;
    }
    return edited;
}

/// <summary>
/// 選択中LevelObjectを指定パスの子へ移動する。
/// </summary>
bool MoveSelectedLevelObjectsToParent(std::vector<LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths, const std::string& parentPath)
{
    LevelObjectData* parentObject = FindLevelObjectByPath(rootObjectDataList, parentPath); // 移動先の親オブジェクト
    if (!parentObject) {
        return false;
    }

    const Math::Transform parentTransform = parentObject->transform; // 移動先のワールドTransform
    std::vector<std::string> movedPaths; // 元位置から削除するパス
    const std::vector<std::string> targetPaths = BuildIndependentLevelObjectSelection(selectedObjectPaths); // 実際に移動するパス
    for (const std::string& objectPath : targetPaths) {
        if (objectPath == parentPath || IsLevelObjectPathAncestorOf(objectPath, parentPath)) {
            continue;
        }
        LevelObjectData* objectData = FindLevelObjectByPath(rootObjectDataList, objectPath); // 移動元オブジェクト
        if (!objectData) {
            continue;
        }
        LevelObjectData movedObject = *objectData; // 子として追加するコピー
        movedObject.localTransform = BuildLevelEditorLocalTransformForParent(movedObject.transform, parentTransform);
        parentObject->children.push_back(std::move(movedObject));
        movedPaths.push_back(objectPath);
    }

    std::sort(movedPaths.begin(), movedPaths.end(), CompareLevelObjectPathForReverseEdit);
    bool edited = false; // 移動が発生したか
    for (const std::string& objectPath : movedPaths) {
        edited |= DeleteLevelObjectByPath(rootObjectDataList, objectPath);
    }
    return edited;
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
/// 選択中LevelObjectをPrefab元から再読み込みする。
/// </summary>
bool ReloadSelectedLevelObjectFromPrefab(LevelObjectData& objectData, std::string* outMessage)
{
    if (objectData.prefabSource.empty()) {
        if (outMessage) {
            *outMessage = "Selected object has no prefab source.";
        }
        return false;
    }

    LevelData prefabData {}; // 読み込んだPrefabデータ
    std::string prefabLoadMessage; // Prefab読み込み結果
    if (!LevelLoader::Load(objectData.prefabSource, prefabData, &prefabLoadMessage) || prefabData.objects.empty()) {
        if (outMessage) {
            *outMessage = prefabLoadMessage.empty() ? "Prefab reload failed." : prefabLoadMessage;
        }
        return false;
    }

    const Math::Transform currentLocalTransform = objectData.localTransform; // 維持するローカルTransform
    const Math::Transform currentWorldTransform = objectData.transform; // 維持するワールドTransform
    const std::string currentName = objectData.name; // 維持する表示名
    const std::string prefabSource = objectData.prefabSource; // 維持するPrefab参照
    objectData = prefabData.objects.front();
    objectData.name = currentName.empty() ? objectData.name : currentName;
    objectData.prefabSource = prefabSource;
    objectData.localTransform = currentLocalTransform;
    objectData.transform = currentWorldTransform;

    if (outMessage) {
        *outMessage = "Reloaded selected object from prefab source.";
    }
    return true;
}

/// <summary>
/// レベルオブジェクト1件の詳細をImGuiで編集する。
/// </summary>
bool DrawLevelObjectDetail(LevelObjectData& objectData)
{
    bool edited = false; // オブジェクト情報が変更されたか

    if (ImGui::CollapsingHeader("Basic", ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::vector<std::string> objectTypeChoices(kLevelObjectTypeNames.begin(), kLevelObjectTypeNames.end()); // type選択候補
        edited |= EditStringCombo("Type", objectData.type, objectTypeChoices);
        edited |= EditStringText("Name", objectData.name);
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
    }

    if (ImGui::CollapsingHeader("Transform (Local)", ImGuiTreeNodeFlags_DefaultOpen)) {
        edited |= EditVector3Value("Translate", objectData.localTransform.translate);
        edited |= EditRotationDegrees("Rotate Deg", objectData.localTransform.rotate);
        edited |= EditVector3Value("Scale", objectData.localTransform.scale);
    }

    if (ImGui::CollapsingHeader("Collider")) {
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
    }

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

        const bool isMeshObject = objectData.type == "MESH" && !objectData.fileName.empty(); // Gizmo選択と対応するMESHか
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

#ifdef USE_IMGUI
/// <summary>
/// 2D座標同士の差分を作成する。
/// </summary>
ImVec2 SubtractImVec2(const ImVec2& lhs, const ImVec2& rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}

/// <summary>
/// 2Dベクトルの長さを取得する。
/// </summary>
float GetImVec2Length(const ImVec2& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

/// <summary>
/// 2Dベクトルの内積を取得する。
/// </summary>
float DotImVec2(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

/// <summary>
/// Scene View画像の編集領域が操作可能なサイズか判定する。
/// </summary>
bool IsLevelEditorSceneViewRectUsable(const ImVec2& imageSize)
{
    constexpr float kMinimumSceneViewImageSize = 4.0f; // Scene View操作を許可する最小表示サイズ
    return std::isfinite(imageSize.x)
        && std::isfinite(imageSize.y)
        && imageSize.x >= kMinimumSceneViewImageSize
        && imageSize.y >= kMinimumSceneViewImageSize;
}
/// <summary>
/// ワールド座標をScene View上の座標へ投影する。
/// </summary>
bool ProjectLevelEditorWorldToSceneView(const Vector3& worldPosition, const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, ImVec2& outScreenPosition)
{
    const float clipX = worldPosition.x * viewProjectionMatrix.m[0][0] + worldPosition.y * viewProjectionMatrix.m[1][0] + worldPosition.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0]; // クリップ座標X
    const float clipY = worldPosition.x * viewProjectionMatrix.m[0][1] + worldPosition.y * viewProjectionMatrix.m[1][1] + worldPosition.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1]; // クリップ座標Y
    const float clipZ = worldPosition.x * viewProjectionMatrix.m[0][2] + worldPosition.y * viewProjectionMatrix.m[1][2] + worldPosition.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2]; // クリップ座標Z
    const float clipW = worldPosition.x * viewProjectionMatrix.m[0][3] + worldPosition.y * viewProjectionMatrix.m[1][3] + worldPosition.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3]; // 透視除算に使うW
    if (!std::isfinite(clipW) || std::fabs(clipW) <= 0.000001f) {
        return false;
    }

    const float ndcX = clipX / clipW; // NDC座標X
    const float ndcY = clipY / clipW; // NDC座標Y
    const float ndcZ = clipZ / clipW; // NDC座標Z
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
        return false;
    }

    outScreenPosition.x = imageMin.x + (ndcX + 1.0f) * 0.5f * imageSize.x;
    outScreenPosition.y = imageMin.y + (1.0f - (ndcY + 1.0f) * 0.5f) * imageSize.y;
    return ndcZ >= 0.0f && ndcZ <= 1.0f;
}

/// <summary>
/// 回転行列で方向ベクトルだけを変換する。
/// </summary>
Vector3 TransformLevelEditorDirection(const Vector3& direction, const Matrix4x4& matrix)
{
    Vector3 result {}; // 変換後の方向
    result.x = direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0];
    result.y = direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1];
    result.z = direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2];
    return MathUtil::SafeNormalize(result, direction);
}

/// <summary>
/// 行列で方向ベクトルを長さを保ったまま変換する。
/// </summary>
Vector3 TransformLevelEditorVector(const Vector3& direction, const Matrix4x4& matrix)
{
    Vector3 result {}; // 変換後のベクトル
    result.x = direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0];
    result.y = direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1];
    result.z = direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2];
    return result;
}

/// <summary>
/// Scene View座標をビュー射影逆行列でワールド座標へ戻す。
/// </summary>
Vector3 UnprojectLevelEditorSceneViewPoint(const ImVec2& screenPosition, float ndcDepth, const Matrix4x4& inverseViewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize)
{
    const float screenRateX = imageSize.x != 0.0f ? (screenPosition.x - imageMin.x) / imageSize.x : 0.5f; // 画像内X割合
    const float screenRateY = imageSize.y != 0.0f ? (screenPosition.y - imageMin.y) / imageSize.y : 0.5f; // 画像内Y割合
    const Vector3 ndcPosition {
        screenRateX * 2.0f - 1.0f,
        1.0f - screenRateY * 2.0f,
        ndcDepth,
    }; // 逆変換に使うNDC座標
    return MathUtil::Transform(ndcPosition, inverseViewProjectionMatrix);
}

/// <summary>
/// Scene Viewのマウス位置から指定平面上のワールド座標を計算する。
/// </summary>
bool CalculateLevelEditorMousePlanePoint(const ImVec2& screenPosition, const Matrix4x4& inverseViewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, const Vector3& planePoint, const Vector3& planeNormal, Vector3& outWorldPoint)
{
    const Vector3 rayStart = UnprojectLevelEditorSceneViewPoint(screenPosition, 0.0f, inverseViewProjectionMatrix, imageMin, imageSize); // Near側のレイ位置
    const Vector3 rayEnd = UnprojectLevelEditorSceneViewPoint(screenPosition, 1.0f, inverseViewProjectionMatrix, imageMin, imageSize); // Far側のレイ位置
    const Vector3 rayDirection = rayEnd - rayStart; // マウス位置から伸びるワールドレイ
    const float denominator = MathUtil::Dot(rayDirection, planeNormal); // レイと平面の交差判定用分母
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float distance = MathUtil::Dot(planePoint - rayStart, planeNormal) / denominator; // レイ上の交点距離
    outWorldPoint = rayStart + rayDirection * distance;
    return std::isfinite(outWorldPoint.x) && std::isfinite(outWorldPoint.y) && std::isfinite(outWorldPoint.z);
}

/// <summary>
/// Scene Viewのマウスドラッグ量をカメラ平面上のワールド移動量へ変換する。
/// </summary>
bool CalculateLevelEditorViewPlaneWorldDelta(const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, const Vector3& planePoint, Vector3& outWorldDelta)
{
    outWorldDelta = {}; // 変換できない場合の移動量
    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
    if (std::fabs(mouseDelta.x) <= 0.000001f && std::fabs(mouseDelta.y) <= 0.000001f) {
        return false;
    }

    if (!IsLevelEditorSceneViewRectUsable(imageSize)) {
        return false;
    }

    const Matrix4x4 inverseViewProjectionMatrix = MathUtil::Inverse(viewProjectionMatrix); // ワールド座標へ戻す逆ビュー射影行列
    const ImVec2 imageCenter { imageMin.x + imageSize.x * 0.5f, imageMin.y + imageSize.y * 0.5f }; // Scene Viewの中心座標
    const Vector3 centerRayStart = UnprojectLevelEditorSceneViewPoint(imageCenter, 0.0f, inverseViewProjectionMatrix, imageMin, imageSize); // 中心レイのNear位置
    const Vector3 centerRayEnd = UnprojectLevelEditorSceneViewPoint(imageCenter, 1.0f, inverseViewProjectionMatrix, imageMin, imageSize); // 中心レイのFar位置
    const Vector3 planeNormal = MathUtil::SafeNormalize(centerRayEnd - centerRayStart, { 0.0f, 0.0f, 1.0f }); // カメラ平面の法線

    const ImVec2 currentMousePosition = ImGui::GetIO().MousePos; // 現在のマウス座標
    const ImVec2 previousMousePosition { currentMousePosition.x - mouseDelta.x, currentMousePosition.y - mouseDelta.y }; // 前フレームのマウス座標
    Vector3 previousWorldPoint {}; // 前フレームの平面上座標
    Vector3 currentWorldPoint {}; // 現在の平面上座標
    if (!CalculateLevelEditorMousePlanePoint(previousMousePosition, inverseViewProjectionMatrix, imageMin, imageSize, planePoint, planeNormal, previousWorldPoint)
        || !CalculateLevelEditorMousePlanePoint(currentMousePosition, inverseViewProjectionMatrix, imageMin, imageSize, planePoint, planeNormal, currentWorldPoint)) {
        return false;
    }

    outWorldDelta = currentWorldPoint - previousWorldPoint;
    return MathUtil::LengthSquared(outWorldDelta) > 0.0000000001f;
}

/// <summary>
/// BOXコライダー中心の自由移動ハンドルを描画してドラッグ量を返す。
/// </summary>
bool DrawLevelColliderCenterMoveHandle(const Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, const ImVec2& centerScreen, const Vector3& worldCenter, Vector3& outWorldDelta)
{
    outWorldDelta = {}; // ハンドルから得たワールド移動量
    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene Viewへ描画するDrawList
    constexpr float kCenterHandleRadius = 8.0f; // 中心自由移動ハンドルの半径
    drawList->AddCircleFilled(centerScreen, kCenterHandleRadius, IM_COL32(255, 245, 120, 240), 16);
    drawList->AddCircle(centerScreen, kCenterHandleRadius + 2.0f, IM_COL32(20, 20, 20, 210), 16, 2.0f);

    ImGui::SetCursorScreenPos({ centerScreen.x - kCenterHandleRadius, centerScreen.y - kCenterHandleRadius });
    ImGui::PushID("LevelColliderMoveCenter");
    ImGui::InvisibleButton("LevelColliderMoveCenter", { kCenterHandleRadius * 2.0f, kCenterHandleRadius * 2.0f });
    const bool isCenterActive = ImGui::IsItemActive(); // 中心自由移動ハンドルをドラッグ中か
    const bool isCenterHovered = ImGui::IsItemHovered(); // 中心自由移動ハンドルにマウスが乗っているか
    ImGui::PopID();

    if (isCenterHovered || isCenterActive) {
        drawList->AddCircle(centerScreen, kCenterHandleRadius + 5.0f, IM_COL32(255, 255, 255, 255), 18, 2.0f);
    }
    if (!isCenterActive) {
        return false;
    }

    return CalculateLevelEditorViewPlaneWorldDelta(viewProjectionMatrix, imageMin, imageSize, worldCenter, outWorldDelta);
}

/// <summary>
/// BOXコライダー中心の均一拡縮ハンドルを描画して倍率を返す。
/// </summary>
bool DrawLevelColliderUniformScaleHandle(const ImVec2& centerScreen, float* outScaleRate)
{
    if (!outScaleRate) {
        return false;
    }

    *outScaleRate = 1.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene Viewへ描画するDrawList
    constexpr float kScaleHandleHalfSize = 8.0f; // 均一拡縮ハンドルの半分サイズ
    constexpr float kScaleSpeed = 0.01f; // 均一拡縮のドラッグ感度
    const ImVec2 handleMin { centerScreen.x - kScaleHandleHalfSize, centerScreen.y - kScaleHandleHalfSize }; // ハンドル左上
    const ImVec2 handleMax { centerScreen.x + kScaleHandleHalfSize, centerScreen.y + kScaleHandleHalfSize }; // ハンドル右下
    drawList->AddRectFilled(handleMin, handleMax, IM_COL32(255, 245, 120, 240), 2.0f);
    drawList->AddRect(handleMin, handleMax, IM_COL32(20, 20, 20, 210), 2.0f, 0, 2.0f);

    ImGui::SetCursorScreenPos(handleMin);
    ImGui::PushID("LevelColliderScaleUniform");
    ImGui::InvisibleButton("LevelColliderScaleUniform", { kScaleHandleHalfSize * 2.0f, kScaleHandleHalfSize * 2.0f });
    const bool isScaleActive = ImGui::IsItemActive(); // 均一拡縮ハンドルをドラッグ中か
    const bool isScaleHovered = ImGui::IsItemHovered(); // 均一拡縮ハンドルにマウスが乗っているか
    ImGui::PopID();

    if (isScaleHovered || isScaleActive) {
        drawList->AddRect({ handleMin.x - 3.0f, handleMin.y - 3.0f }, { handleMax.x + 3.0f, handleMax.y + 3.0f }, IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
    }
    if (!isScaleActive) {
        return false;
    }

    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
    *outScaleRate = (std::max)(1.0f + (mouseDelta.x - mouseDelta.y) * kScaleSpeed, 0.01f);
    return std::fabs(*outScaleRate - 1.0f) > 0.000001f;
}

/// <summary>
/// BOXコライダーのScene View軸ハンドルを描画してドラッグ量を返す。
/// </summary>
bool DrawLevelColliderSceneGizmoAxis(const char* id, const ImVec2& centerScreen, const ImVec2& handleScreen, ImU32 color, float axisWorldLength, float* outWorldDelta)
{
    if (!outWorldDelta) {
        return false;
    }

    *outWorldDelta = 0.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList(); // Scene View上へ描画するDrawList
    const ImVec2 axisVector = SubtractImVec2(handleScreen, centerScreen); // 画面上の軸ベクトル
    const float axisPixelLength = GetImVec2Length(axisVector); // 画面上の軸長
    if (axisPixelLength <= 8.0f || axisWorldLength <= 0.000001f) {
        return false;
    }

    const ImVec2 axisDirection = { axisVector.x / axisPixelLength, axisVector.y / axisPixelLength }; // 正規化済み画面軸
    drawList->AddLine(centerScreen, handleScreen, IM_COL32(10, 10, 10, 190), 5.0f);
    drawList->AddLine(centerScreen, handleScreen, color, 3.0f);
    drawList->AddCircleFilled(handleScreen, 6.5f, color, 16);
    drawList->AddCircle(handleScreen, 8.5f, IM_COL32(255, 255, 255, 230), 16, 1.5f);

    ImGui::SetCursorScreenPos({ handleScreen.x - 11.0f, handleScreen.y - 11.0f });
    ImGui::PushID(id);
    ImGui::InvisibleButton("LevelColliderAxisHandle", { 22.0f, 22.0f });
    const bool isActive = ImGui::IsItemActive(); // このハンドルをドラッグ中か
    const bool isHovered = ImGui::IsItemHovered(); // このハンドルにマウスが乗っているか
    ImGui::PopID();

    if (isHovered || isActive) {
        drawList->AddCircle(handleScreen, 12.0f, IM_COL32(255, 255, 255, 255), 18, 2.0f);
    }
    if (!isActive) {
        return false;
    }

    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta; // 今フレームのマウス移動量
    const float pixelDelta = DotImVec2(mouseDelta, axisDirection); // 軸方向の画面移動量
    *outWorldDelta = pixelDelta / axisPixelLength * axisWorldLength;
    return std::fabs(*outWorldDelta) > 0.000001f;
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

    const ImVec2 imageMin { imageMinX, imageMinY }; // Scene View画像の左上座標
    const ImVec2 imageSize { imageWidth, imageHeight }; // Scene View画像の表示サイズ
    const Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(objectData->transform.scale, objectData->transform.rotate, objectData->transform.translate); // LevelObjectのワールド行列
    const Vector3 worldCenter = MathUtil::Transform(objectData->collider.center, worldMatrix); // コライダー中心のワールド座標
    ImVec2 centerScreen {}; // コライダー中心のScene View座標
    if (!ProjectLevelEditorWorldToSceneView(worldCenter, viewProjectionMatrix, imageMin, imageSize, centerScreen)) {
        return;
    }

    const Matrix4x4 rotateMatrix = MathUtil::Multiply(
        MathUtil::MakeRotateXMatrix(objectData->transform.rotate.x),
        MathUtil::Multiply(MathUtil::MakeRotateYMatrix(objectData->transform.rotate.y), MathUtil::MakeRotateZMatrix(objectData->transform.rotate.z))); // LevelObjectの回転行列
    const std::array<Vector3, 3> localAxes {
        Vector3 { 1.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 1.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 1.0f },
    }; // コライダー編集用のローカル軸
    const std::array<Vector3, 3> worldAxes {
        TransformLevelEditorDirection(localAxes[0], rotateMatrix),
        TransformLevelEditorDirection(localAxes[1], rotateMatrix),
        TransformLevelEditorDirection(localAxes[2], rotateMatrix),
    }; // コライダー編集用のワールド軸
    const std::array<float, 3> axisScales {
        (std::max)(std::fabs(objectData->transform.scale.x), 0.0001f),
        (std::max)(std::fabs(objectData->transform.scale.y), 0.0001f),
        (std::max)(std::fabs(objectData->transform.scale.z), 0.0001f),
    }; // ローカル量とワールド量の変換に使うスケール
    const std::array<ImU32, 3> axisColors {
        IM_COL32(240, 80, 80, 255),
        IM_COL32(80, 220, 110, 255),
        IM_COL32(90, 150, 255, 255),
    }; // XYZハンドル色
    const std::array<const char*, 3> axisIds { "ColliderX", "ColliderY", "ColliderZ" }; // ImGui ID用の軸名


    bool edited = false; // コライダーが変更されたか
    constexpr float kMinimumColliderSize = 0.001f; // コライダーサイズの最小値
    constexpr float kColliderGizmoAxisWorldLength = 1.0f; // コライダーサイズに影響されないギズモ軸長
    const float centerAxisWorldLength = kColliderGizmoAxisWorldLength; // ハンドルまでの固定ワールド距離

    if (gizmoOperationMode == 2) {
        float uniformScaleRate = 1.0f; // 中心ハンドルから得た均一拡縮倍率
        if (DrawLevelColliderUniformScaleHandle(centerScreen, &uniformScaleRate)) {
            objectData->collider.size.x = (std::max)(objectData->collider.size.x * uniformScaleRate, kMinimumColliderSize);
            objectData->collider.size.y = (std::max)(objectData->collider.size.y * uniformScaleRate, kMinimumColliderSize);
            objectData->collider.size.z = (std::max)(objectData->collider.size.z * uniformScaleRate, kMinimumColliderSize);
            edited = true;
        }
    }

    if (gizmoOperationMode == 0) {
        Vector3 centerWorldDelta {}; // 中心ハンドルから得たワールド移動量
        if (DrawLevelColliderCenterMoveHandle(viewProjectionMatrix, imageMin, imageSize, centerScreen, worldCenter, centerWorldDelta)) {
            const Matrix4x4 inverseRotateMatrix = MathUtil::Inverse(rotateMatrix); // ワールド移動量をローカル方向へ戻す逆回転行列
            const Vector3 centerLocalDelta = TransformLevelEditorVector(centerWorldDelta, inverseRotateMatrix); // スケール適用前のローカル移動量
            if (std::fabs(objectData->transform.scale.x) > 0.0001f) {
                objectData->collider.center.x += centerLocalDelta.x / objectData->transform.scale.x;
            }
            if (std::fabs(objectData->transform.scale.y) > 0.0001f) {
                objectData->collider.center.y += centerLocalDelta.y / objectData->transform.scale.y;
            }
            if (std::fabs(objectData->transform.scale.z) > 0.0001f) {
                objectData->collider.center.z += centerLocalDelta.z / objectData->transform.scale.z;
            }
            edited = true;
        }
    }
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const float handleWorldLength = centerAxisWorldLength; // コライダーサイズに影響されないハンドル距離

        const Vector3 handleWorld = worldCenter + worldAxes[axisIndex] * handleWorldLength; // ハンドルのワールド座標
        ImVec2 handleScreen {}; // ハンドルのScene View座標
        if (!ProjectLevelEditorWorldToSceneView(handleWorld, viewProjectionMatrix, imageMin, imageSize, handleScreen)) {
            continue;
        }

        float worldDelta = 0.0f; // ハンドルドラッグから得たワールド移動量
        if (!DrawLevelColliderSceneGizmoAxis(axisIds[axisIndex], centerScreen, handleScreen, axisColors[axisIndex], handleWorldLength, &worldDelta)) {
            continue;
        }

        const float localDelta = worldDelta / axisScales[axisIndex]; // LevelDataへ反映するローカル量
        if (gizmoOperationMode == 0) {
            if (axisIndex == 0) {
                objectData->collider.center.x += localDelta;
            } else if (axisIndex == 1) {
                objectData->collider.center.y += localDelta;
            } else {
                objectData->collider.center.z += localDelta;
            }
        } else {
            if (axisIndex == 0) {
                objectData->collider.size.x = (std::max)(objectData->collider.size.x + localDelta * 2.0f, kMinimumColliderSize);
            } else if (axisIndex == 1) {
                objectData->collider.size.y = (std::max)(objectData->collider.size.y + localDelta * 2.0f, kMinimumColliderSize);
            } else {
                objectData->collider.size.z = (std::max)(objectData->collider.size.z + localDelta * 2.0f, kMinimumColliderSize);
            }
        }
        edited = true;
    }

    if (edited) {
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

/// <summary>
/// ImGuiでレベルJSONの読み込み状態を表示する。
/// </summary>
void PlayScene::DrawLevelDataImGui()
{
#ifdef USE_IMGUI
    static std::array<char, 256> levelPathBuffer {}; // 編集中の読み込みレベルJSONファイル名
    static std::array<char, 256> levelSavePathBuffer {}; // 編集中の保存レベルJSONファイル名
    static std::array<char, 256> levelPrefabPathBuffer {}; // 編集中のPrefab JSONファイル名
    static std::string bufferedLevelPath; // 読み込みバッファへ反映済みのファイル名
    static std::string bufferedLevelSavePath; // 保存バッファへ反映済みのファイル名
    static std::string bufferedLevelPrefabPath; // Prefabバッファへ反映済みのファイル名
    static std::string levelPrefabFileName = "levels/prefabs/selected_prefab.json"; // Prefab保存と挿入に使うJSONファイル名
    static bool autoApplyEditedLevel = false; // LevelData編集時に即シーンへ反映するか
    static std::vector<LevelData> levelUndoHistory; // LevelData編集のUndo履歴
    static std::vector<LevelData> levelRedoHistory; // LevelData編集のRedo履歴
    static bool pendingLevelEditHistory = false; // 編集終了待ちのUndo履歴があるか
    static LevelData pendingLevelEditSnapshot; // 編集開始時点のLevelData
    static int pendingLevelSaveAction = 0; // 未適用保存確認後に実行する処理
    static std::vector<std::string> selectedLevelObjectPaths; // 複数選択中のLevelObjectパス
    static std::vector<LevelObjectData> levelObjectClipboard; // コピーしたLevelObject群
    SyncTextBuffer(levelDataFileName_, levelPathBuffer, bufferedLevelPath);
    SyncTextBuffer(levelSaveFileName_, levelSavePathBuffer, bufferedLevelSavePath);
    SyncTextBuffer(levelPrefabFileName, levelPrefabPathBuffer, bufferedLevelPrefabPath);

    auto executeReloadLevel = [&]() {
        if (ReloadLevelSceneObjects()) {
            levelUndoHistory.clear();
            levelRedoHistory.clear();
            pendingLevelEditHistory = false;
        }
    }; // レベル再読み込み処理
    auto executeSaveLevel = [&]() {
        SaveLevelSnapshot();
    }; // レベル保存処理
    auto executeSaveAndReloadLevel = [&]() {
        if (SaveLevelSnapshot()) {
            levelDataFileName_ = LevelWriter::ResolveWritableLevelPath(levelSaveFileName_);
            bufferedLevelPath.clear();
            executeReloadLevel();
        }
    }; // レベル保存後の再読み込み処理

    if (ImGui::CollapsingHeader("Load", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawLevelLoadFileSelector(levelDataFileName_, levelPathBuffer, bufferedLevelPath);
        if (ImGui::InputText("Load File", levelPathBuffer.data(), levelPathBuffer.size())) {
            levelDataFileName_ = levelPathBuffer.data();
            bufferedLevelPath = levelDataFileName_;
        }
        if (ImGui::Button("Reload")) {
            if (levelDirty_) {
                ImGui::OpenPopup("Confirm Reload Level");
            } else {
                executeReloadLevel();
            }
        }
    }

    if (ImGui::CollapsingHeader("Save", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::InputText("Save File", levelSavePathBuffer.data(), levelSavePathBuffer.size())) {
            levelSaveFileName_ = levelSavePathBuffer.data();
            bufferedLevelSavePath = levelSaveFileName_;
        }
        const std::string resolvedSaveFilePath = LevelWriter::ResolveWritableLevelPath(levelSaveFileName_); // 実際に書き込むレベルJSONパス
        ImGui::TextWrapped("Resolved Save File: %s", resolvedSaveFilePath.empty() ? "-" : resolvedSaveFilePath.c_str());
        if (ImGui::Button("Save Snapshot")) {
            if (!levelAppliedToScene_) {
                pendingLevelSaveAction = 1;
                ImGui::OpenPopup("Confirm Save Not Applied");
            } else {
                executeSaveLevel();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Use Save As Load")) {
            levelDataFileName_ = resolvedSaveFilePath;
            bufferedLevelPath.clear();
        }
        if (ImGui::Button("Save And Reload")) {
            if (!levelAppliedToScene_) {
                pendingLevelSaveAction = 2;
                ImGui::OpenPopup("Confirm Save Not Applied");
            } else {
                executeSaveAndReloadLevel();
            }
        }
        ImGui::Text("Save Result: %s", levelSaveSucceeded_ ? "Success" : "Failed");
        ImGui::Text("Save Message: %s", levelSaveMessage_.empty() ? "-" : levelSaveMessage_.c_str());
    }


    if (ImGui::CollapsingHeader("Prefab")) {
        if (ImGui::InputText("Prefab File", levelPrefabPathBuffer.data(), levelPrefabPathBuffer.size())) {
            levelPrefabFileName = levelPrefabPathBuffer.data();
            bufferedLevelPrefabPath = levelPrefabFileName;
        }
        ImGui::SeparatorText("Prefab Source");
        const std::string resolvedPrefabFilePath = LevelWriter::ResolveWritableLevelPath(levelPrefabFileName); // 実際に使うPrefab JSONパス
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
                const bool prefabSaveSucceeded = LevelWriter::SaveHierarchySnapshot(levelPrefabFileName, prefabData, &prefabSaveMessage); // Prefab保存結果
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
                PushLevelEditHistory(levelUndoHistory, levelData_);
                levelRedoHistory.clear();
                pendingLevelEditHistory = false;
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
            if (!LevelLoader::Load(levelPrefabFileName, prefabData, &prefabLoadMessage) || prefabData.objects.empty()) {
                SetLevelLoadStatus(false, prefabLoadMessage.empty() ? "Prefab load failed." : prefabLoadMessage);
            } else {
                PushLevelEditHistory(levelUndoHistory, levelData_);
                levelRedoHistory.clear();
                pendingLevelEditHistory = false;
                for (LevelObjectData& prefabObject : prefabData.objects) {
                    if (!prefabObject.name.empty()) {
                        prefabObject.name += "_Instance";
                    }
                    prefabObject.prefabSource = levelPrefabFileName;
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
    if (ImGui::CollapsingHeader("Scene Apply", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Auto Apply Edited Level", &autoApplyEditedLevel);
        if (ImGui::Button("Sync Scene To Level")) {
            PushLevelEditHistory(levelUndoHistory, levelData_);
            levelRedoHistory.clear();
            pendingLevelEditHistory = false;
            if (SyncSceneObjectsToLevelData()) {
                MarkLevelDataDirty("Scene objects synced to level data. Save hierarchy snapshot.", true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply Edited Level")) {
            ApplyLevelDataToScene();
        }
        ImGui::SameLine();
        if (ImGui::Button("Preload Level Models")) {
            PreloadLevelModels();
        }
        if (ImGui::Button("Undo Level Edit")) {
            if (RestoreLevelEditHistory(levelUndoHistory, levelRedoHistory, levelData_)) {
                RefreshLevelDataSummary();
                if (autoApplyEditedLevel) {
                    ApplyLevelDataToScene();
                }
                MarkLevelDataDirty("Undo level edit. Save hierarchy snapshot.", autoApplyEditedLevel);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Redo Level Edit")) {
            if (RestoreLevelEditHistory(levelRedoHistory, levelUndoHistory, levelData_)) {
                RefreshLevelDataSummary();
                if (autoApplyEditedLevel) {
                    ApplyLevelDataToScene();
                }
                MarkLevelDataDirty("Redo level edit. Save hierarchy snapshot.", autoApplyEditedLevel);
            }
        }
        ImGui::Text("Undo: %zu  Redo: %zu", levelUndoHistory.size(), levelRedoHistory.size());
    }

    if (ImGui::CollapsingHeader("Status")) {
        ImGui::Text("Load Result: %s", levelLoadSucceeded_ ? "Success" : "Failed");
        ImGui::Text("Load Message: %s", levelLoadMessage_.empty() ? "-" : levelLoadMessage_.c_str());
        ImGui::Text("Scene Name: %s", levelData_.name.empty() ? "-" : levelData_.name.c_str());
        ImGui::Text("Schema Version: %d", levelData_.schemaVersion);
        ImGui::Text("Root Objects: %zu", levelData_.objects.size());
        ImGui::Text("Total Objects: %zu", levelTotalObjectCount_);
        ImGui::Text("Mesh Objects: %zu", levelMeshObjectCount_);
        ImGui::Text("Colliders: %zu", levelColliderObjectCount_);
        ImGui::Text("Dirty: %s", levelDirty_ ? "Unsaved" : "Saved");
        ImGui::Text("Apply State: %s", levelAppliedToScene_ ? "Applied" : "Not Applied");
        const LevelValidationSummary validationSummary = BuildLevelValidationSummary(levelData_); // LevelDataの検証結果
        ImGui::Text("Invalid Types: %zu", validationSummary.invalidTypeCount);
        ImGui::Text("Missing Model Names: %zu", validationSummary.missingModelFileNameCount);
        ImGui::Text("Missing Model Assets: %zu", validationSummary.missingModelAssetCount);
    }

    if (ImGui::BeginPopupModal("Confirm Reload Level", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Unsaved level edits will be discarded by reload.");
        if (ImGui::Button("Discard And Reload")) {
            executeReloadLevel();
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
            if (pendingLevelSaveAction == 1) {
                executeSaveLevel();
            } else if (pendingLevelSaveAction == 2) {
                executeSaveAndReloadLevel();
            }
            pendingLevelSaveAction = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pendingLevelSaveAction = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (levelData_.objects.empty()) {
            ImGui::TextDisabled("No level objects loaded.");
        } else {
            const LevelData beforeEditLevelData = levelData_; // 編集前のLevelDataスナップショット
            size_t meshObjectIndex = 0; // LevelData内のMESH順に対応するObject3D番号
            LevelLoader::ResolveWorldTransforms(levelData_);
            bool levelEdited = false; // Levelタブでオブジェクト情報が編集されたか
            ImGui::SeparatorText("Selection Tools");
            PruneSelectedLevelObjectPaths(levelData_.objects, selectedLevelObjectPaths);
            ImGui::Text("Selected: %zu", selectedLevelObjectPaths.size());
            const bool hasSelectedObjects = !selectedLevelObjectPaths.empty(); // 複数選択があるか
            if (!hasSelectedObjects) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Copy Selected")) {
                CopySelectedLevelObjects(levelData_.objects, selectedLevelObjectPaths, levelObjectClipboard);
            }
            ImGui::SameLine();
            if (ImGui::Button("Duplicate Selected")) {
                levelEdited |= DuplicateSelectedLevelObjects(levelData_.objects, selectedLevelObjectPaths);
                selectedLevelObjectPaths.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Move Selected To Root")) {
                levelEdited |= MoveSelectedLevelObjectsToRoot(levelData_.objects, selectedLevelObjectPaths);
                selectedLevelObjectPaths.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Selected")) {
                levelEdited |= DeleteSelectedLevelObjects(levelData_.objects, selectedLevelObjectPaths);
                selectedLevelObjectPaths.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Selection")) {
                selectedLevelObjectPaths.clear();
            }
            if (!hasSelectedObjects) {
                ImGui::EndDisabled();
            }
            const bool hasClipboardObjects = !levelObjectClipboard.empty(); // 貼り付け可能なコピーがあるか
            if (!hasClipboardObjects) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Paste To Root")) {
                levelEdited |= PasteLevelObjectsToRoot(levelData_.objects, levelObjectClipboard);
            }
            if (!hasClipboardObjects) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            ImGui::Text("Clipboard: %zu", levelObjectClipboard.size());
            ImGui::SeparatorText("Hierarchy");
            levelEdited |= DrawLevelObjectTree(*this, levelData_.objects, levelData_.objects, "root", meshObjectIndex, selectedLevelObjectPaths);
            if (levelEdited) {
                if (!pendingLevelEditHistory) {
                    pendingLevelEditSnapshot = beforeEditLevelData;
                    pendingLevelEditHistory = true;
                }
                RefreshLevelDataSummary();
                if (autoApplyEditedLevel) {
                    ApplyLevelDataToScene();
                    MarkLevelDataDirty("Level data edited and applied. Save hierarchy snapshot.", true);
                } else {
                    levelAppliedToScene_ = false;
                    MarkLevelDataDirty("Level data edited. Apply or save hierarchy snapshot.", false);
                }
            }
        }
    }

    if (pendingLevelEditHistory && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        PushLevelEditHistory(levelUndoHistory, pendingLevelEditSnapshot);
        levelRedoHistory.clear();
        pendingLevelEditHistory = false;
    }
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

        ImGui::EndTabBar();
    }

    ImGui::End();
#endif
}

