#pragma once
#include <cstdint>
#include <dinput.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace MyEngine {

/// <summary>
/// キーボード & マウス入力管理クラス（シングルトン）
/// </summary>
class InputManager {
public:
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

private:
    // シングルトン構成
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

private:
    static constexpr int KEY_COUNT = 256; // キー数（256固定）

    // キーボード入力
    BYTE keys_[KEY_COUNT] {}; // 現在のキー状態
    BYTE preKeys_[KEY_COUNT] {}; // 前フレームのキー状態
    ComPtr<IDirectInputDevice8> keyboard_;

    // マウス入力
    ComPtr<IDirectInputDevice8> mouse_; // マウスデバイス
    DIMOUSESTATE2 mouseState_ {}; // 現在のマウス状態
    DIMOUSESTATE2 preMouseState_ {}; // 前フレームのマウス状態
};
} // namespace MyEngine
