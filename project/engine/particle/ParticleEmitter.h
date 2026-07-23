#pragma once

#include "engine/utility/mathUtility.h"
#include <string>

namespace MyEngine {
class ParticleManager;
class ImGuiManager;
}

/// <summary>
/// パーティクルを発生させるエミッタークラス。
/// </summary>
class ParticleEmitter {
public:
    /// <summary>
    /// 設定されたグループにパーティクルを発生させる。
    /// </summary>
    void Emit();

    /// <summary>
    /// 経過時間を進め、発生間隔を超えた分だけ発生させる。
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// ImGuiでエミッター設定を編集する。
    /// </summary>
    void DrawImGui();

    std::string groupName; // 発生させるパーティクルグループ名
    Math::Transform transform { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }; // 発生位置
    uint32_t count = 1; // 1回あたりの発生数
    float frequency = 0.0f; // 発生間隔
    float elapsed = 0.0f; // 前回発生からの経過時間
    bool useHitEffect = false; // ヒットエフェクト用の発生を使うか
    bool useRingEffect = false; // リングエフェクト用の発生を使うか
    bool useCylinderEffect = false; // 円柱エフェクト用の発生を使うか
    bool showDebugRange = true; // デバッグ表示で発生範囲を表示するか
    Math::Vector3 debugRangeHalfSize = { 1.0f, 0.5f, 1.0f }; // デバッグ表示用の発生範囲半径
    int debugGridHalfLineCount = 4; // デバッグ範囲グリッドの片側ライン数
    float debugGridSpacing = 0.25f; // デバッグ範囲グリッドの線間隔
};
