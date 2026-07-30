#pragma once
#include <cstdint>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800 // DirectInputの使用バージョン
#endif
#include <dinput.h>
#include <Xinput.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace MyEngine {

/// <summary>
/// キーボード & マウス入力管理クラス（シングルトン）
/// </summary>
class InputManager {
public: // メンバ関数
    // インスタンス取得（シングルトン）
    static InputManager* GetInstance();

    // 初期化（DirectInputの生成後に呼ぶ）
    bool Initialize(IDirectInput8* directInput, HWND hwnd);

    // 終了処理（リソース解放）
    void Finalize();

    // 毎フレームの状態更新
    void Update();

    // --- キーボード入力 ---
    bool IsKeyPressed(uint8_t key) const; // 押されている
    bool IsKeyReleased(uint8_t key) const; // 離されている
    bool IsKeyJustPressed(uint8_t key) const; // 押した瞬間
    bool IsKeyJustReleased(uint8_t key) const; // 離した瞬間

    // --- マウス入力 ---
    bool IsMouseButtonPressed(int button) const; // 押されている
    bool IsMouseButtonReleased(int button) const; // 離されている
    bool IsMouseButtonJustPressed(int button) const; // 押した瞬間
    bool IsMouseButtonJustReleased(int button) const; // 離した瞬間

    long GetMouseDeltaX() const; // 前フレームからのX移動量
    long GetMouseDeltaY() const; // 前フレームからのY移動量
    long GetMouseDeltaZ() const; // ホイールの回転量

    // --- ゲームパッド入力 ---
    bool IsGamePadConnected(uint32_t padIndex = 0) const; // 接続されている
    bool IsGamePadButtonPressed(WORD button, uint32_t padIndex = 0) const; // 押されている
    bool IsGamePadButtonJustPressed(WORD button, uint32_t padIndex = 0) const; // 押した瞬間
    float GetGamePadLeftStickX(uint32_t padIndex = 0) const; // 左スティックX軸
    float GetGamePadLeftStickY(uint32_t padIndex = 0) const; // 左スティックY軸

private: // メンバ関数（内部用）
    // シングルトン構成
    InputManager() = default; // デフォルトコンストラクタは private にして外部からのインスタンス化を防止
    ~InputManager() = default; // デストラクタも private にして外部からの破棄を防止
    InputManager(const InputManager&) = delete; // コピーコンストラクタを削除してコピーを禁止
    InputManager& operator=(const InputManager&) = delete; // コピー代入演算子を削除してコピーを禁止

    /// <summary>
    /// マウスボタン番号が有効範囲内か確認する
    /// </summary>
    bool IsValidMouseButton(int button) const;

    /// <summary>
    /// ゲームパッド番号が有効範囲内か確認する
    /// </summary>
    bool IsValidGamePadIndex(uint32_t padIndex) const;

private: // メンバ変数
    static constexpr int KEY_COUNT = 256; // キー数（256固定）
    static constexpr uint32_t GAMEPAD_COUNT = 4; // XInputで確認するゲームパッド数

    // キーボード入力
    BYTE keys_[KEY_COUNT] {}; // 現在のキー状態
    BYTE preKeys_[KEY_COUNT] {}; // 前フレームのキー状態
    ComPtr<IDirectInputDevice8> keyboard_; // キーボードデバイス

    // マウス入力
    ComPtr<IDirectInputDevice8> mouse_; // マウスデバイス
    DIMOUSESTATE2 mouseState_ {}; // 現在のマウス状態
    DIMOUSESTATE2 preMouseState_ {}; // 前フレームのマウス状態

    // ゲームパッド入力
    XINPUT_STATE gamePadStates_[GAMEPAD_COUNT] {}; // 現在のゲームパッド状態
    XINPUT_STATE preGamePadStates_[GAMEPAD_COUNT] {}; // 前フレームのゲームパッド状態
    bool gamePadConnected_[GAMEPAD_COUNT] {}; // ゲームパッドの接続状態
    bool preGamePadConnected_[GAMEPAD_COUNT] {}; // 前フレームのゲームパッド接続状態
};
} // namespace MyEngine
