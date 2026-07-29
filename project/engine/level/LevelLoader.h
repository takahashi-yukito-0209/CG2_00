#pragma once

#include "LevelData.h"

#include <string>

namespace MyEngine {

/// <summary>
/// Blenderレベルエディタから出力したJSONを読み込むクラス。
/// </summary>
class LevelLoader {
public:
    /// <summary>
    /// 指定したJSONファイルからレベルデータを読み込む。
    /// </summary>
    static bool Load(const std::string& filePath, LevelData& levelData);

    /// <summary>
    /// 指定したJSONファイルからレベルデータを読み込み、失敗理由を取得する。
    /// </summary>
    static bool Load(const std::string& filePath, LevelData& levelData, std::string* errorMessage);

    /// <summary>
    /// レベルデータ内のローカルTransformからワールドTransformを再計算する。
    /// </summary>
    static void ResolveWorldTransforms(LevelData& levelData);
};

} // namespace MyEngine