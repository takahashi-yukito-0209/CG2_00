#include "PlaySceneLevelEditorHierarchy.h"
#include "../../engine/level/LevelLoader.h"
#include "../../engine/utility/mathUtility.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

using namespace MyEngine;
using namespace Math;

namespace {

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
/// LevelObjectの階層パスが現在のLevelData上に存在するか判定する。
/// </summary>
bool DoesLevelObjectPathExist(std::vector<LevelObjectData>& rootObjectDataList, const std::string& objectPath)
{
    std::vector<LevelObjectData>* parentList = nullptr; // 親の兄弟配列
    size_t objectIndex = 0; // 兄弟内番号
    return FindLevelObjectParentListByPath(rootObjectDataList, objectPath, &parentList, &objectIndex);
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
    PlaySceneLevelEditorHierarchy::RenameDuplicatedLevelObject(duplicatedObject);
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

} // namespace

namespace PlaySceneLevelEditorHierarchy {

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

} // namespace PlaySceneLevelEditorHierarchy
