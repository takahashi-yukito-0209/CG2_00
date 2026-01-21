#pragma once

#include <string>
#include "engine/utility/mathUtility.h"

namespace MyEngine { class ParticleManager; }

// パーティクル発生器
class ParticleEmitter {
public:
    // エミッタ設定
    std::string groupName; // どのパーティクルグループに発生させるか
    Transform transform { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    uint32_t count = 1;      // 1回あたりの発生数
    float frequency = 0.0f;  // 発生間隔（秒）。0で毎フレーム
    float elapsed = 0.0f;    // 前回発生からの経過

    // すぐ発生
    void Emit();

    // 時間を進め、必要なら発生
    void Update(float deltaTime);

    // Draw ImGui controls for this emitter (edit its properties)
    void DrawImGui();
};
