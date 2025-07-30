#include "Input.h"
#include <cstring>

Input* Input::GetInstance()
{
    static Input instance;
    return &instance;
}

/// <summary>
/// DirectInput デバイスの初期化（キーボード・マウス）
/// </summary>
bool Input::Initialize(IDirectInput8* directInput, HWND hwnd)
{
    HRESULT hr;

    // --- キーボードの初期化 ---
    hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard_, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    keyboard_->SetDataFormat(&c_dfDIKeyboard);
    keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    keyboard_->Acquire(); // 入力開始

    std::memset(keys_, 0, KEY_COUNT);
    std::memset(preKeys_, 0, KEY_COUNT);

    // --- マウスの初期化 ---
    hr = directInput->CreateDevice(GUID_SysMouse, &mouse_, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    mouse_->SetDataFormat(&c_dfDIMouse2); // 拡張マウス情報（ホイール含む）
    mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    mouse_->Acquire(); // 入力開始

    std::memset(&mouseState_, 0, sizeof(mouseState_));
    std::memset(&preMouseState_, 0, sizeof(preMouseState_));

    return true;
}

/// <summary>
/// デバイスの解放（終了処理）
/// </summary>
void Input::Finalize()
{
    if (keyboard_) {
        keyboard_->Unacquire();
        keyboard_->Release();
        keyboard_ = nullptr;
    }

    if (mouse_) {
        mouse_->Unacquire();
        mouse_->Release();
        mouse_ = nullptr;
    }
}

/// <summary>
/// 毎フレーム呼び出して、キーボード・マウス状態を更新
/// </summary>
void Input::Update()
{
    // --- キーボードの状態更新 ---
    std::memcpy(preKeys_, keys_, KEY_COUNT);
    if (FAILED(keyboard_->GetDeviceState(KEY_COUNT, keys_))) {
        keyboard_->Acquire(); // 再取得
        std::memset(keys_, 0, KEY_COUNT);
    }

    // --- マウスの状態更新 ---
    preMouseState_ = mouseState_;
    if (FAILED(mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_))) {
        mouse_->Acquire(); // 再取得
        std::memset(&mouseState_, 0, sizeof(mouseState_));
    }
}

/// <summary>
/// 指定キーが現在押されているか
/// </summary>
bool Input::IsKeyPressed(uint8_t key) const
{
    return keys_[key] & 0x80;
}

/// <summary>
/// 指定キーが現在離されているか
/// </summary>
bool Input::IsKeyReleased(uint8_t key) const
{
    return !(keys_[key] & 0x80);
}

/// <summary>
/// 指定キーが押された瞬間か
/// </summary>
bool Input::IsKeyJustPressed(uint8_t key) const
{
    return !(preKeys_[key] & 0x80) && (keys_[key] & 0x80);
}

/// <summary>
/// 指定キーが離された瞬間か
/// </summary>
bool Input::IsKeyJustReleased(uint8_t key) const
{
    return (preKeys_[key] & 0x80) && !(keys_[key] & 0x80);
}

// ------------------------
// マウス入力関連
// ------------------------

/// <summary>
/// 指定ボタン（0～7）が押されているか
/// </summary>
bool Input::IsMouseButtonPressed(int button) const
{
    return mouseState_.rgbButtons[button] & 0x80;
}

/// <summary>
/// 指定ボタンが離されているか
/// </summary>
bool Input::IsMouseButtonReleased(int button) const
{
    return !(mouseState_.rgbButtons[button] & 0x80);
}

/// <summary>
/// 指定ボタンが押された瞬間か
/// </summary>
bool Input::IsMouseButtonJustPressed(int button) const
{
    return !(preMouseState_.rgbButtons[button] & 0x80) && (mouseState_.rgbButtons[button] & 0x80);
}

/// <summary>
/// 指定ボタンが離された瞬間か
/// </summary>
bool Input::IsMouseButtonJustReleased(int button) const
{
    return (preMouseState_.rgbButtons[button] & 0x80) && !(mouseState_.rgbButtons[button] & 0x80);
}

/// <summary>
/// マウスのX軸移動量（フレーム間差分）
/// </summary>
long Input::GetMouseDeltaX() const
{
    return mouseState_.lX;
}

/// <summary>
/// マウスのY軸移動量（フレーム間差分）
/// </summary>
long Input::GetMouseDeltaY() const
{
    return mouseState_.lY;
}

/// <summary>
/// ホイールの回転量（Z軸）
/// </summary>
long Input::GetMouseDeltaZ() const
{
    return mouseState_.lZ;
}
