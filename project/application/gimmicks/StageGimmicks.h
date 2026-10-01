#pragma once

#include "../player/PlayerState.h"

#include <cstdint>
#include <memory>
#include <span>
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
/// トグルスイッチギミックの初期化情報
/// </summary>
struct ToggleSwitchGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // スイッチの表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // スイッチの中心座標
    Math::Vector3 volumeCenter { 0.0f, 0.0f, 0.0f }; // 入力判定範囲の中心
    Math::Vector3 volumeHalfSize { 0.5f, 0.5f, 0.5f }; // 入力判定範囲の半サイズ
    Math::Vector4 inactiveColor { 0.18f, 0.18f, 0.22f, 1.0f }; // OFF時の表示色
    Math::Vector4 activeColor { 1.0f, 0.45f, 0.1f, 1.0f }; // ON時の表示色
    Math::Vector4 pressedColor { 1.0f, 0.85f, 0.2f, 1.0f }; // 分身が踏んでいる時の表示色
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
/// 連動橋ギミックの初期化情報
/// </summary>
struct LinkedBridgeGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 橋の表示スケール
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // 橋の中心座標
    Math::Vector4 retractedColor { 0.18f, 0.18f, 0.22f, 0.18f }; // 未展開時の表示色
    Math::Vector4 deployedColor { 1.0f, 0.85f, 0.12f, 1.0f }; // 展開時の表示色
};

/// <summary>
/// 昇降足場ギミックの初期化情報
/// </summary>
struct MovingPlatformGimmickDesc {
    uint32_t objectId = 0; // 表示オブジェクトに割り当てるID
    std::string modelFileName; // 表示に使用するモデルファイル名
    Math::Vector3 scale { 1.0f, 1.0f, 1.0f }; // 足場の表示スケール
    Math::Vector3 lowerTranslate { 0.0f, 0.0f, 0.0f }; // 往復移動の下端座標
    Math::Vector3 upperTranslate { 0.0f, 2.0f, 0.0f }; // 往復移動の上端座標
    Math::Vector4 inactiveColor { 0.18f, 0.18f, 0.22f, 1.0f }; // 停止中の表示色
    Math::Vector4 activeColor { 1.0f, 0.45f, 0.1f, 1.0f }; // 稼働中の表示色
    float moveSpeed = 1.0f; // 1秒あたりの移動距離
    float upperWaitSeconds = 0.0f; // 上端へ到達した後に停止する秒数
    float lowerWaitSeconds = 0.0f; // 下端へ到達した後に停止する秒数
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
    /// プレイヤーと可視分身の位置からスイッチ状態を更新する。
    /// </summary>
    void Update(const PlayerState& playerState, std::span<const PlayerState> cloneStates);

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

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

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
    Math::Vector3 editorScale_ { 1.0f, 1.0f, 1.0f }; // 編集同期に使う直前の表示スケール
    Math::Vector3 editorTranslate_ { 0.0f, 0.0f, 0.0f }; // 編集同期に使う直前の表示座標
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
    /// 可視分身の位置と経過時間からスイッチ状態を更新する。
    /// </summary>
    void Update(float deltaTime, std::span<const PlayerState> cloneStates);

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

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

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
    Math::Vector3 editorScale_ { 1.0f, 1.0f, 1.0f }; // 編集同期に使う直前の表示スケール
    Math::Vector3 editorTranslate_ { 0.0f, 0.0f, 0.0f }; // 編集同期に使う直前の表示座標
    Math::Vector4 inactiveColor_ { 0.18f, 0.18f, 0.22f, 1.0f }; // 未入力時の表示色
    Math::Vector4 activeColor_ { 0.0f, 0.7f, 1.0f, 1.0f }; // 起動中の表示色
    Math::Vector4 triggerColor_ { 0.2f, 1.0f, 0.85f, 1.0f }; // 分身が踏んでいる時の表示色
    float holdSeconds_ = 3.0f; // 入力後に起動を維持する秒数
    float remainingSeconds_ = 0.0f; // 起動維持の残り秒数
    bool cloneOnSwitch_ = false; // 分身が判定内にいるか
};

/// <summary>
/// 分身が判定へ入るたびにONとOFFを切り替えるスイッチギミックを管理するクラス
/// </summary>
class ToggleSwitchGimmick {
public:
    /// <summary>
    /// トグルスイッチの表示、判定範囲、色を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const ToggleSwitchGimmickDesc& desc);

    /// <summary>
    /// トグルスイッチが保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 可視分身の位置からスイッチ状態を更新する。
    /// </summary>
    void Update(std::span<const PlayerState> cloneStates);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// トグルスイッチを描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// トグルスイッチをOFF状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// スイッチがONか取得する。
    /// </summary>
    bool IsActive() const { return active_; }

    /// <summary>
    /// 分身がスイッチ判定内にいるか取得する。
    /// </summary>
    bool IsCloneOnSwitch() const { return cloneOnSwitch_; }

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

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
    Math::Vector3 editorScale_ { 1.0f, 1.0f, 1.0f }; // 編集同期に使う直前の表示スケール
    Math::Vector3 editorTranslate_ { 0.0f, 0.0f, 0.0f }; // 編集同期に使う直前の表示座標
    Math::Vector4 inactiveColor_ { 0.18f, 0.18f, 0.22f, 1.0f }; // OFF時の表示色
    Math::Vector4 activeColor_ { 1.0f, 0.45f, 0.1f, 1.0f }; // ON時の表示色
    Math::Vector4 pressedColor_ { 1.0f, 0.85f, 0.2f, 1.0f }; // 分身が踏んでいる時の表示色
    bool active_ = false; // スイッチがONか
    bool cloneOnSwitch_ = false; // 分身が判定内にいるか
    bool cloneOnSwitchLastFrame_ = false; // 前フレームに分身が判定内にいたか
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
    /// プレイヤーと可視分身の位置からスイッチ状態を更新する。
    /// </summary>
    void Update(const PlayerState& playerState, std::span<const PlayerState> cloneStates);

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

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果をスイッチ判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

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
    Math::Vector3 editorScale_ { 1.0f, 1.0f, 1.0f }; // 編集同期に使う直前の表示スケール
    Math::Vector3 editorTranslate_ { 0.0f, 0.0f, 0.0f }; // 編集同期に使う直前の表示座標
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

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果を扉の衝突判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

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
/// 入力に連動して足場を展開する橋ギミックを管理するクラス
/// </summary>
class LinkedBridgeGimmick {
public:
    /// <summary>
    /// 橋の表示、衝突範囲、色を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const LinkedBridgeGimmickDesc& desc);

    /// <summary>
    /// 橋が保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 入力状態に合わせて橋の展開状態を更新する。
    /// </summary>
    void Update(bool shouldDeploy);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 橋を描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// 橋を未展開状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// 展開中の橋の全面コライダーを取得する。
    /// </summary>
    SolidCollider GetSolidCollider() const;

    /// <summary>
    /// 橋が展開されているか取得する。
    /// </summary>
    bool IsDeployed() const { return deployed_; }

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果を橋の衝突判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

private:
    /// <summary>
    /// 現在状態に応じた表示色を反映する。
    /// </summary>
    void ApplyVisual();

    /// <summary>
    /// 指定した3DオブジェクトをAlphaブレンドで描画する。
    /// </summary>
    void DrawWithAlphaBlend();

    std::unique_ptr<MyEngine::Object3d> object_; // 橋表示用オブジェクト
    Math::Vector3 scale_ { 1.0f, 1.0f, 1.0f }; // 橋の表示スケール
    Math::Vector3 translate_ { 0.0f, 0.0f, 0.0f }; // 橋の中心座標
    Math::Vector4 retractedColor_ { 0.18f, 0.18f, 0.22f, 0.18f }; // 未展開時の表示色
    Math::Vector4 deployedColor_ { 1.0f, 0.85f, 0.12f, 1.0f }; // 展開時の表示色
    bool deployed_ = false; // 橋が展開されているか
};

/// <summary>
/// 入力中に下端と上端を往復する昇降足場ギミックを管理するクラス
/// </summary>
class MovingPlatformGimmick {
public:
    /// <summary>
    /// 昇降足場の表示、移動範囲、速度を初期化する。
    /// </summary>
    void Initialize(MyEngine::Object3dCommon* object3dCommon, MyEngine::ImGuiManager* imguiManager, const MovingPlatformGimmickDesc& desc);

    /// <summary>
    /// 昇降足場が保持する表示用リソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 入力状態と経過時間から足場位置を更新する。
    /// </summary>
    void Update(float deltaTime, bool shouldMove);

    /// <summary>
    /// 表示用オブジェクトを更新する。
    /// </summary>
    void UpdateObject(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 昇降足場を描画する。
    /// </summary>
    void Draw();

    /// <summary>
    /// 昇降足場を下端の停止状態へ戻す。
    /// </summary>
    void Reset();

    /// <summary>
    /// 現在位置の全面コライダーを取得する。
    /// </summary>
    SolidCollider GetSolidCollider() const;

    /// <summary>
    /// 更新前位置の全面コライダーを取得する。
    /// </summary>
    SolidCollider GetPreviousSolidCollider() const;

    /// <summary>
    /// 直近の更新で移動した量を取得する。
    /// </summary>
    Math::Vector3 GetMovementDelta() const;

    /// <summary>
    /// 足場が入力を受けて稼働中か取得する。
    /// </summary>
    bool IsActive() const { return active_; }

    /// <summary>
    /// 端点で待機中か取得する。
    /// </summary>
    bool IsWaitingAtEndpoint() const { return endpointWaitRemainingSeconds_ > 0.0f; }

    /// <summary>
    /// 端点で待機する残り秒数を取得する。
    /// </summary>
    float GetEndpointWaitRemainingSeconds() const { return endpointWaitRemainingSeconds_; }

    /// <summary>
    /// 現在の足場座標を取得する。
    /// </summary>
    const Math::Vector3& GetCurrentTranslate() const { return currentTranslate_; }

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果を昇降範囲と衝突判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

    /// <summary>
    /// ステージ保存用に昇降範囲の下端座標を取得する。
    /// </summary>
    const Math::Vector3& GetEditorLowerTranslate() const { return lowerTranslate_; }

    /// <summary>
    /// ステージ保存用に昇降範囲の上端座標を取得する。
    /// </summary>
    const Math::Vector3& GetEditorUpperTranslate() const { return upperTranslate_; }

    /// <summary>
    /// 保存された表示Transformと昇降範囲を復元する。
    /// </summary>
    void RestoreEditorTransform(const Math::Vector3& scale, const Math::Vector3& rotate, const Math::Vector3& lowerTranslate, const Math::Vector3& upperTranslate);

private:
    /// <summary>
    /// 現在状態に応じた表示色と座標を反映する。
    /// </summary>
    void ApplyVisual();

    std::unique_ptr<MyEngine::Object3d> object_; // 昇降足場表示用オブジェクト
    Math::Vector3 scale_ { 1.0f, 1.0f, 1.0f }; // 足場の表示スケール
    Math::Vector3 lowerTranslate_ { 0.0f, 0.0f, 0.0f }; // 往復移動の下端座標
    Math::Vector3 upperTranslate_ { 0.0f, 2.0f, 0.0f }; // 往復移動の上端座標
    Math::Vector3 currentTranslate_ { 0.0f, 0.0f, 0.0f }; // 現在の足場座標
    Math::Vector3 previousTranslate_ { 0.0f, 0.0f, 0.0f }; // 更新前の足場座標
    Math::Vector4 inactiveColor_ { 0.18f, 0.18f, 0.22f, 1.0f }; // 停止中の表示色
    Math::Vector4 activeColor_ { 1.0f, 0.45f, 0.1f, 1.0f }; // 稼働中の表示色
    float moveSpeed_ = 1.0f; // 1秒あたりの移動距離
    float upperWaitSeconds_ = 0.0f; // 上端へ到達した後に停止する秒数
    float lowerWaitSeconds_ = 0.0f; // 下端へ到達した後に停止する秒数
    float endpointWaitRemainingSeconds_ = 0.0f; // 現在の端点で停止する残り秒数
    bool active_ = false; // 入力を受けて稼働中か
    bool movingToUpper_ = true; // 上端へ向かって移動中か
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

    /// <summary>
    /// ステージ編集で操作する表示オブジェクトを取得する。
    /// </summary>
    MyEngine::Object3d* GetEditorObject() const { return object_.get(); }

    /// <summary>
    /// 表示オブジェクトの編集結果をゲートの衝突判定へ反映する。
    /// </summary>
    void ApplyEditorTransform();

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
    /// ゴール表示の編集量を判定範囲へ反映する。
    /// </summary>
    void ApplyEditorTransform(const Math::Vector3& translateDelta, const Math::Vector3& scaleRatio);

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
