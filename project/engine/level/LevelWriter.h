#pragma once

#include "LevelData.h"

#include <string>

namespace MyEngine {

/// <summary>
/// エンジン内のレベルデータをJSONへ書き出すクラス。
/// </summary>
class LevelWriter {
public:
    /// <summary>
    /// レベルJSONの保存先として使用する実ファイルパスを解決する。
    /// </summary>
    static std::string ResolveWritableLevelPath(const std::string& filePath);

    /// <summary>
    /// 現在のローカルTransformと親子階層を再ロード可能なJSONとして保存する。
    /// </summary>
    static bool SaveHierarchySnapshot(const std::string& filePath, const LevelData& levelData);

    /// <summary>
    /// 現在のローカルTransformと親子階層を再ロード可能なJSONとして保存し、失敗理由を取得する。
    /// </summary>
    static bool SaveHierarchySnapshot(const std::string& filePath, const LevelData& levelData, std::string* errorMessage);

    /// <summary>
    /// 互換用に階層を維持したJSON保存を呼び出す。
    /// </summary>
    static bool SaveFlatSnapshot(const std::string& filePath, const LevelData& levelData);
};

} // namespace MyEngine
