#pragma once

#include "../../engine/utility/MathTypes.h"

/// <summary>
/// 上面だけを足場として扱うための簡易プラットフォーム情報
/// </summary>
struct StandablePlatform {
    Math::Vector3 center { 0.0f, 0.0f, 0.0f }; // 足場の中心座標
    Math::Vector3 halfSize { 0.5f, 0.5f, 0.5f }; // 足場の半サイズ
    bool enabled = false; // 足場として使うか
};

/// <summary>
/// 全面を衝突対象として扱うための簡易コライダー情報
/// </summary>
struct SolidCollider {
    Math::Vector3 center { 0.0f, 0.0f, 0.0f }; // コライダーの中心座標
    Math::Vector3 halfSize { 0.5f, 0.5f, 0.5f }; // コライダーの半サイズ
    bool enabled = false; // 衝突対象として使うか
};

/// <summary>
/// プレイヤーと分身で共有する再生可能な状態
/// </summary>
struct PlayerState {
    Math::Transform transform {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.5f, 0.0f }
    }; // プレイヤーのTransform
    bool isMoving = false; // 移動入力があるか
    bool isGrounded = true; // 接地しているか
};