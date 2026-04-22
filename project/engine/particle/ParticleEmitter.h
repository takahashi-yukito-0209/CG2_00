#pragma once

#include "engine/utility/mathUtility.h"
#include <string>

// 前方宣言: MyEngine 名前空間内のクラスを宣言
namespace MyEngine {
class ParticleManager;
}
// 前方宣言: ImGuiManager in MyEngine
namespace MyEngine { class ImGuiManager; }

/// <summary>
/// パーティクルエミッタクラス
/// </summary>
class ParticleEmitter {
public: // メンバ関数
    /// <summary>
    /// 発生させるパーティクルグループの名前を指定してエミッタを作成
    /// </summary>
    void Emit();

    /// <summary>
    /// 経過時間を加算し、発生間隔を超えたら発生させる
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// ImGuiコントロールを描画（プロパティ編集）
    /// </summary>
    void DrawImGui();

    // エミッタ設定
    std::string groupName; // どのパーティクルグループに発生させるか
    Math::Transform transform { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    uint32_t count = 1; // 1回あたりの発生数
    float frequency = 0.0f; // 発生間隔（秒）。0で毎フレーム
    float elapsed = 0.0f; // 前回発生からの経過

private: // メンバ関数
     
};