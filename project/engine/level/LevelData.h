#pragma once

#include "../utility/MathTypes.h"

#include <string>
#include <vector>

namespace MyEngine {

constexpr int kCurrentLevelSchemaVersion = 3; // 現在保存するLevel JSONスキーマバージョン

/// <summary>
/// レベルデータ内のコライダー情報を保持する構造体。
/// </summary>
struct LevelColliderData {
    bool enabled = false; // コライダー情報が有効か
    std::string type; // コライダー種別
    Math::Vector3 center { 0.0f, 0.0f, 0.0f }; // ローカル中心座標
    Math::Vector3 size { 1.0f, 1.0f, 1.0f }; // ローカルサイズ
};

/// <summary>
/// レベルデータ内の1オブジェクト分の情報を保持する構造体。
/// </summary>
struct LevelObjectData {
    std::string type; // Blender上のオブジェクト種別
    std::string name; // Blender上のオブジェクト名
    std::string fileName; // 読み込むモデルファイル名
    std::string prefabSource; // 再利用元Prefab JSONパス
    Math::Transform localTransform {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    }; // エンジン座標系へ変換した親基準のローカルTransform
    Math::Transform transform {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    }; // エンジン座標系へ変換し、親子階層を反映したワールドTransform
    LevelColliderData collider; // コライダー情報
    std::vector<LevelObjectData> children; // 子オブジェクト
};

/// <summary>
/// Blenderから出力したレベル全体の情報を保持する構造体。
/// </summary>
struct LevelData {
    int schemaVersion = 1; // 読み込んだLevel JSONスキーマバージョン
    std::string name; // レベルデータ名
    std::vector<LevelObjectData> objects; // ルート直下のオブジェクト一覧
};

} // namespace MyEngine