#pragma once

#include "../../engine/level/LevelData.h"
#include "../../engine/utility/MathTypes.h"

#include <string>
#include <vector>

namespace PlaySceneLevelEditorHierarchy {

/// <summary>
/// ワールドTransformを新しい親基準のローカルTransformへ変換する。
/// </summary>
Math::Transform BuildLevelEditorLocalTransformForParent(const Math::Transform& worldTransform, const Math::Transform& parentTransform);

/// <summary>
/// Level Editorで追加する空の子オブジェクトを作成する。
/// </summary>
MyEngine::LevelObjectData CreateEmptyLevelEditorObject(const std::string& objectName);

/// <summary>
/// 複製したレベルオブジェクトの名前を識別しやすい名前へ変更する。
/// </summary>
void RenameDuplicatedLevelObject(MyEngine::LevelObjectData& objectData);

/// <summary>
/// 階層パスからLevelObjectを取得する。
/// </summary>
MyEngine::LevelObjectData* FindLevelObjectByPath(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::string& objectPath);

/// <summary>
/// 選択済みLevelObjectパスか判定する。
/// </summary>
bool IsLevelObjectPathSelected(const std::vector<std::string>& selectedObjectPaths, const std::string& objectPath);

/// <summary>
/// LevelObjectの複数選択状態を切り替える。
/// </summary>
void SetLevelObjectPathSelected(std::vector<std::string>& selectedObjectPaths, const std::string& objectPath, bool selected);

/// <summary>
/// 存在しなくなったLevelObjectの選択パスを取り除く。
/// </summary>
void PruneSelectedLevelObjectPaths(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, std::vector<std::string>& selectedObjectPaths);

/// <summary>
/// 選択中LevelObjectをまとめて複製する。
/// </summary>
bool DuplicateSelectedLevelObjects(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths);

/// <summary>
/// 選択中LevelObjectをまとめて削除する。
/// </summary>
bool DeleteSelectedLevelObjects(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths);

/// <summary>
/// 選択中LevelObjectをクリップボードへコピーする。
/// </summary>
bool CopySelectedLevelObjects(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths, std::vector<MyEngine::LevelObjectData>& clipboard);

/// <summary>
/// クリップボード内のLevelObjectをルートへ貼り付ける。
/// </summary>
bool PasteLevelObjectsToRoot(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::vector<MyEngine::LevelObjectData>& clipboard);

/// <summary>
/// 選択中LevelObjectをルートへ移動する。
/// </summary>
bool MoveSelectedLevelObjectsToRoot(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths);

/// <summary>
/// 選択中LevelObjectを指定パスの子へ移動する。
/// </summary>
bool MoveSelectedLevelObjectsToParent(std::vector<MyEngine::LevelObjectData>& rootObjectDataList, const std::vector<std::string>& selectedObjectPaths, const std::string& parentPath);

/// <summary>
/// 選択中LevelObjectをPrefab元から再読み込みする。
/// </summary>
bool ReloadSelectedLevelObjectFromPrefab(MyEngine::LevelObjectData& objectData, std::string* outMessage);

} // namespace PlaySceneLevelEditorHierarchy
