#pragma once

#include "../../engine/level/LevelData.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

/// <summary>
/// シーンごとに所有する表示オブジェクトの生成・削除選択。
/// </summary>
struct PlaySceneObjectEditorState {
    int selectedCreateTextureIndex = 0; // 生成に使用するテクスチャ番号
    int selectedDeleteSpriteIndex = 0; // 削除対象のスプライト番号
    int selectedCreateModelIndex = 0; // 生成に使用するモデル番号
    int selectedDeleteObjectIndex = 0; // 削除対象のオブジェクト番号
};

/// <summary>
/// シーンごとに所有するLevelData編集状態。描画処理やゲーム状態は持たない。
/// </summary>
struct PlaySceneLevelEditorState {
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
    std::filesystem::file_time_type autoReloadLastWriteTime {}; // 最後に確認した更新日時
    std::vector<MyEngine::LevelData> levelUndoHistory; // LevelData編集のUndo履歴
    std::vector<MyEngine::LevelData> levelRedoHistory; // LevelData編集のRedo履歴
    bool pendingLevelEditHistory = false; // 編集終了待ちのUndo履歴があるか
    MyEngine::LevelData pendingLevelEditSnapshot; // 編集開始時点のLevelData
    int pendingLevelSaveAction = 0; // 未適用保存確認後に実行する処理
    std::vector<std::string> selectedLevelObjectPaths; // 複数選択中のLevelObjectパス
    std::vector<MyEngine::LevelObjectData> levelObjectClipboard; // コピーしたLevelObject群
};
