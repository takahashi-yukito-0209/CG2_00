#pragma once

#include "../player/PlayerState.h"

#include <cstdint>
#include <memory>
#include <string>

namespace MyEngine {
class ImGuiManager;
class Object3d;
class Object3dCommon;
}

/// <summary>
/// 箱形スイッチギミックの初期化情報
/// </summary>
struct BoxSwitchGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // スイッチの表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // スイッチの中心座標
    Math::Vector3 volumeCenter { 0.0f, 0.0f, 0.0f }; // 入力判定範囲の中心
    Math::Vector3 volumeHalfSize { 0.5f, 0.5f, 0.5f }; // 入力判定範囲の半サイズ
    Math::Vector4 inactiveColor { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 activeColor { 0.0f, 1.0f, 0.45f, 1.0f }; // 分身入力時の表示色
    Math::Vector4 playerOnlyColor { 1.0f, 0.62f, 0.12f, 1.0f }; // プレイヤーだけが乗った時の表示色
};

/// <summary>
/// 時間差スイッチギミックの初期化情報
/// </summary>
struct TimedSwitchGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // スイッチの表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // スイッチの中心座標
    Math::Vector3 volumeCenter { 0.0f, 0.0f, 0.0f }; // 入力判定範囲の中心
    Math::Vector3 volumeHalfSize { 0.5f, 0.5f, 0.5f }; // 入力判定範囲の半サイズ
    Math::Vector4 inactiveColor { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 activeColor { 0.0f, 0.7f, 1.0f, 1.0f }; // 起動中の表示色
    Math::Vector4 triggerColor { 0.2f, 1.0f, 0.85f, 1.0f }; // 分身が踏んでいる時の表示色
    float holdSeconds = 3.0f; // 入力が消えた後に起動を維持する秒数
};

/// <summary>
/// 重さスイッチギミックの初期化情報
/// </summary>
struct WeightSwitchGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // スイッチの表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // スイッチの中心座標
    Math::Vector3 volumeCenter { 0.0f, 0.0f, 0.0f }; // 入力判定範囲の中心
    Math::Vector3 volumeHalfSize { 0.5f, 0.5f, 0.5f }; // 入力判定範囲の半サイズ
    Math::Vector4 inactiveColor { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 partialColor { 1.0f, 0.62f, 0.12f, 1.0f }; // 片方だけが乗った時の表示色
    Math::Vector4 activeColor { 0.65f, 1.0f, 0.1f, 1.0f }; // 両方が乗った時の表示色
};

/// <summary>
/// 連動扉ギミックの初期化情報
/// </summary>
struct LinkedDoorGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 扉の表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // 扉の中心座標
    Math::Vector4 closedColor { 1.0f, 0.12f, 0.12f, 1.0f }; // 閉じている時の表示色
    Math::Vector4 openColor { 0.0f, 1.0f, 0.45f, 0.22f }; // 開いている時の表示色
};

/// <summary>
/// 一方通行ゲートギミックの初期化情報
/// </summary>
struct OneWayGateGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // ゲートの表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // ゲートの中心座標
    Math::Vector4 passableColor { 0.0f, 0.65f, 1.0f, 0.25f }; // 通行できる側にいる時の表示色
    Math::Vector4 blockingColor { 0.15f, 0.25f, 1.0f, 0.85f }; // 戻りを塞ぐ時の表示色
    float allowedDirectionX = 1.0f; // 通行を許すX方向。正なら左から右、負なら右から左
};

/// <summary>
/// 箱形ゴールギミックの初期化情報
/// </summary>
struct BoxGoalGimmickDesc {
    Math::Vector3 center { 0.0f, 0.0f, 0.0f }; // ゴール判定範囲の中心
    Math::Vector3 halfSize { 0.5f, 0.5f, 0.5f }; // ゴール判定範囲の半サイズ
};

/// <summary>
/// 箱形スイッチギミックを管理するクラス
/// </summary>
class BoxSwitchGimmick {
public:
    /// <summary>
    /// スイッチの表示、判定範囲、色を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const BoxSwitchGimmickDesc& desc);

    /// <summary>
    /// スイッチが保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// プレイヤーと分身の位置からスイッチ状態を更新する。
    /// </summary>
    void Update(const PlayerState& playerState, bool cloneVisible, const PlayerState& cloneState);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// スイッチを描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// スイッチ状態を初期状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// スイッチが有効化されているか取得する。
    /// </summary>
    bool IsActive() const { return active_; }

    /// <summary>
    /// プレイヤーがスイッチ判定内にいるか取得する。
    /// </summary>
    bool IsPlayerOnSwitch() const { return playerOnSwitch_; }

    /// <summary>
    /// 分身がスイッチ判定内にいるか取得する。
    /// </summary>
    bool IsCloneOnSwitch() const { return cloneOnSwitch_; }

private:
    /// <summary>
    /// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
    /// </summary>
    bool Contains(const PlayerState& state) const;

    /// <summary>
    /// 現在状態に応じた表示色を反映する。
    /// </summary>
    void ApplyVisual();

    std::unique_ptr<MyEngine::Object3d> object_; // スイッチ表示用オブジェクト
    Math::Vector3 volumeCenter_ { 0.0f, 0.0f, 0.0f }; // スイッチ判定範囲の中心
    Math::Vector3 volumeHalfSize_ { 0.5f, 0.5f, 0.5f }; // スイッチ判定範囲の半サイズ
    Math::Vector4 inactiveColor_ { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 activeColor_ { 0.0f, 1.0f, 0.45f, 1.0f }; // 分身入力時の表示色
    Math::Vector4 playerOnlyColor_ { 1.0f, 0.62f, 0.12f, 1.0f }; // プレイヤーだけが乗った時の表示色
    bool active_ = false; // スイッチがギミックとして有効か
    bool playerOnSwitch_ = false; // プレイヤーが判定内にいるか
    bool cloneOnSwitch_ = false; // 分身が判定内にいるか
};

/// <summary>
/// 分身入力後に一定時間だけ起動を維持するスイッチギミックを管理するクラス
/// </summary>
class TimedSwitchGimmick {
public:
    /// <summary>
    /// 時間差スイッチの表示、判定範囲、色、維持時間を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const TimedSwitchGimmickDesc& desc);

    /// <summary>
    /// 時間差スイッチが保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 分身の位置と経過時間からスイッチ状態を更新する。
    /// </summary>
    void Update(float deltaTime, bool cloneVisible, const PlayerState& cloneState);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 時間差スイッチを描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// 時間差スイッチ状態を初期状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// スイッチが起動中か取得する。
    /// </summary>
    bool IsActive() const { return remainingSeconds_ > 0.0f; }

    /// <summary>
    /// 分身がスイッチ判定内にいるか取得する。
    /// </summary>
    bool IsCloneOnSwitch() const { return cloneOnSwitch_; }

    /// <summary>
    /// 起動維持の残り秒数を取得する。
    /// </summary>
    float GetRemainingSeconds() const { return remainingSeconds_; }

private:
    /// <summary>
    /// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
    /// </summary>
    bool Contains(const PlayerState& state) const;

    /// <summary>
    /// 現在状態に応じた表示色を反映する。
    /// </summary>
    void ApplyVisual();

    std::unique_ptr<MyEngine::Object3d> object_; // スイッチ表示用オブジェクト
    Math::Vector3 volumeCenter_ { 0.0f, 0.0f, 0.0f }; // スイッチ判定範囲の中心
    Math::Vector3 volumeHalfSize_ { 0.5f, 0.5f, 0.5f }; // スイッチ判定範囲の半サイズ
    Math::Vector4 inactiveColor_ { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 activeColor_ { 0.0f, 0.7f, 1.0f, 1.0f }; // 起動中の表示色
    Math::Vector4 triggerColor_ { 0.2f, 1.0f, 0.85f, 1.0f }; // 分身が踏んでいる時の表示色
    float holdSeconds_ = 3.0f; // 入力後に起動を維持する秒数
    float remainingSeconds_ = 0.0f; // 起動維持の残り秒数
    bool cloneOnSwitch_ = false; // 分身が判定内にいるか
};

/// <summary>
/// プレイヤーと分身が同時に乗った時だけ起動する重さスイッチギミックを管理するクラス
/// </summary>
class WeightSwitchGimmick {
public:
    /// <summary>
    /// 重さスイッチの表示、判定範囲、色を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const WeightSwitchGimmickDesc& desc);

    /// <summary>
    /// 重さスイッチが保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// プレイヤーと分身の位置からスイッチ状態を更新する。
    /// </summary>
    void Update(const PlayerState& playerState, bool cloneVisible, const PlayerState& cloneState);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 重さスイッチを描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// 重さスイッチ状態を初期状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// スイッチが起動中か取得する。
    /// </summary>
    bool IsActive() const { return active_; }

    /// <summary>
    /// プレイヤーがスイッチ判定内にいるか取得する。
    /// </summary>
    bool IsPlayerOnSwitch() const { return playerOnSwitch_; }

    /// <summary>
    /// 分身がスイッチ判定内にいるか取得する。
    /// </summary>
    bool IsCloneOnSwitch() const { return cloneOnSwitch_; }

private:
    /// <summary>
    /// 指定したプレイヤー状態がスイッチ判定内にあるか判定する。
    /// </summary>
    bool Contains(const PlayerState& state) const;

    /// <summary>
    /// 現在状態に応じた表示色を反映する。
    /// </summary>
    void ApplyVisual();

    std::unique_ptr<MyEngine::Object3d> object_; // スイッチ表示用オブジェクト
    Math::Vector3 volumeCenter_ { 0.0f, 0.0f, 0.0f }; // スイッチ判定範囲の中心
    Math::Vector3 volumeHalfSize_ { 0.5f, 0.5f, 0.5f }; // スイッチ判定範囲の半サイズ
    Math::Vector4 inactiveColor_ { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 partialColor_ { 1.0f, 0.62f, 0.12f, 1.0f }; // 片方だけが乗った時の表示色
    Math::Vector4 activeColor_ { 0.65f, 1.0f, 0.1f, 1.0f }; // 両方が乗った時の表示色
    bool active_ = false; // スイッチがギミックとして有効か
    bool playerOnSwitch_ = false; // プレイヤーが判定内にいるか
    bool cloneOnSwitch_ = false; // 分身が判定内にいるか
};

/// <summary>
/// スイッチに連動して開閉する扉ギミックを管理するクラス
/// </summary>
class LinkedDoorGimmick {
public:
    /// <summary>
    /// 扉の表示、衝突範囲、色を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const LinkedDoorGimmickDesc& desc);

    /// <summary>
    /// 扉が保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 入力状態に合わせて扉の開閉状態を更新する。
    /// </summary>
    void Update(bool shouldOpen);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 扉を描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// 扉状態を閉じた状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// 閉じている扉の全面コライダーを取得する。
    /// </summary>
    SolidCollider GetSolidCollider() const;

    /// <summary>
    /// 扉が開いているか取得する。
    /// </summary>
    bool IsOpen() const { return open_; }

private:
    /// <summary>
    /// 現在状態に応じた表示色を反映する。
    /// </summary>
    void ApplyVisual();

    /// <summary>
    /// 指定した3DオブジェクトをAlphaブレンドで描画する。
    /// </summary>
    void DrawWithAlphaBlend();

    std::unique_ptr<MyEngine::Object3d> object_; // 扉表示用オブジェクト
    Math::Vector3 scale_ { 1.0f, 1.0f, 1.0f }; // 扉の表示スケール
    Math::Vector3 translate_ { 0.0f, 0.0f, 0.0f }; // 扉の中心座標
    Math::Vector4 closedColor_ { 1.0f, 0.12f, 0.12f, 1.0f }; // 閉じている時の表示色
    Math::Vector4 openColor_ { 0.0f, 1.0f, 0.45f, 0.22f }; // 開いている時の表示色
    bool open_ = false; // 扉が開いているか
};

/// <summary>
/// 戻り方向だけを塞ぐ一方通行ゲートギミックを管理するクラス
/// </summary>
class OneWayGateGimmick {
public:
    /// <summary>
    /// 一方通行ゲートの表示、衝突範囲、通行方向を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const OneWayGateGimmickDesc& desc);

    /// <summary>
    /// 一方通行ゲートが保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// プレイヤー位置からゲートの遮断状態を更新する。
    /// </summary>
    void Update(const PlayerState& playerState);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 一方通行ゲートを描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// ゲート状態を初期状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// 現在のプレイヤー位置で有効な全面コライダーを取得する。
    /// </summary>
    SolidCollider GetSolidCollider() const;

    /// <summary>
    /// 現在プレイヤーの戻りを塞いでいるか取得する。
    /// </summary>
    bool IsBlocking() const { return blocking_; }

private:
    /// <summary>
    /// 現在状態に応じた表示色を反映する。
    /// </summary>
    void ApplyVisual();

    /// <summary>
    /// 指定した3DオブジェクトをAlphaブレンドで描画する。
    /// </summary>
    void DrawWithAlphaBlend();

    std::unique_ptr<MyEngine::Object3d> object_; // ゲート表示用オブジェクト
    Math::Vector3 scale_ { 1.0f, 1.0f, 1.0f }; // ゲートの表示スケール
    Math::Vector3 translate_ { 0.0f, 0.0f, 0.0f }; // ゲートの中心座標
    Math::Vector4 passableColor_ { 0.0f, 0.65f, 1.0f, 0.25f }; // 通行可能状態の表示色
    Math::Vector4 blockingColor_ { 0.15f, 0.25f, 1.0f, 0.85f }; // 戻りを塞ぐ状態の表示色
    float allowedDirectionX_ = 1.0f; // 通行を許すX方向
    bool blocking_ = false; // 戻り方向を塞いでいるか
};

/// <summary>
/// 箱形範囲でゴール到達を判定するギミックを管理するクラス
/// </summary>
class BoxGoalGimmick {
public:
    /// <summary>
    /// ゴール判定範囲を設定する。
    /// </summary>
    void Configure(const BoxGoalGimmickDesc& desc);

    /// <summary>
    /// プレイヤー状態からゴール到達を更新する。
    /// </summary>
    bool Update(const PlayerState& playerState);

    /// <summary>
    /// ゴール到達状態を初期状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// ゴールに到達済みか取得する。
    /// </summary>
    bool IsReached() const { return reached_; }

private:
    /// <summary>
    /// 指定位置がゴール判定範囲内にあるか判定する。
    /// </summary>
    bool Contains(const Math::Vector3& position) const;

    Math::Vector3 center_ { 0.0f, 0.0f, 0.0f }; // ゴール判定範囲の中心
    Math::Vector3 halfSize_ { 0.5f, 0.5f, 0.5f }; // ゴール判定範囲の半サイズ
    bool reached_ = false; // ゴール到達済みか
};