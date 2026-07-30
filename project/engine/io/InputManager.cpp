#include "InputManager.h"
#include "engine/utility/DebugUtility.h"
#include "Logger.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

using namespace MyEngine;

namespace {
constexpr uint8_t kInputPressedMask = 0x80; // DirectInputの押下状態を示すビット

/// <summary>
/// XInputのスティック入力をデッドゾーン込みで正規化する
/// </summary>
float NormalizeGamePadStickAxis(SHORT rawValue, SHORT deadZone)
{
    const int absoluteValue = std::abs(static_cast<int>(rawValue)); // 符号を除いた入力値
    if (absoluteValue <= deadZone) {
        return 0.0f;
    }

    const int direction = rawValue < 0 ? -1 : 1; // 入力方向
    const float effectiveValue = static_cast<float>(absoluteValue - deadZone); // デッドゾーンを除いた入力量
    const float effectiveRange = static_cast<float>(32767 - deadZone); // 正規化に使う有効範囲
    return static_cast<float>(direction) * std::clamp(effectiveValue / effectiveRange, 0.0f, 1.0f);
}
} // namespace

/// <summary>
/// シングルトンインスタンスを取得する
/// </summary>
InputManager* InputManager::GetInstance()
{
    static InputManager instance; // 共有する入力管理インスタンス
    return &instance;
}

/// <summary>
/// DirectInput デバイスを初期化する
/// </summary>
bool InputManager::Initialize(IDirectInput8* directInput, HWND hwnd)
{
    Finalize();

    std::memset(keys_, 0, KEY_COUNT); // 現在のキー状態を初期化
    std::memset(preKeys_, 0, KEY_COUNT); // 前フレームのキー状態を初期化
    std::memset(&mouseState_, 0, sizeof(mouseState_)); // 現在のマウス状態を初期化
    std::memset(&preMouseState_, 0, sizeof(preMouseState_)); // 前フレームのマウス状態を初期化
    std::memset(gamePadStates_, 0, sizeof(gamePadStates_)); // 現在のゲームパッド状態を初期化
    std::memset(preGamePadStates_, 0, sizeof(preGamePadStates_)); // 前フレームのゲームパッド状態を初期化
    std::memset(gamePadConnected_, 0, sizeof(gamePadConnected_)); // 現在の接続状態を初期化
    std::memset(preGamePadConnected_, 0, sizeof(preGamePadConnected_)); // 前フレームの接続状態を初期化

    if (!directInput || !hwnd) {
        Logger::Error("InputManager::Initialize failed. directInput or hwnd is null.\n");
        return false;
    }

    HRESULT hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr); // キーボードデバイスを生成
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at CreateDevice Keyboard.")) {
        Finalize();
        return false;
    }

    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard); // キーボードのデータ形式を設定
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at SetDataFormat Keyboard.")) {
        Finalize();
        return false;
    }

    hr = keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE); // キーボードの協調レベルを設定
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at SetCooperativeLevel Keyboard.")) {
        Finalize();
        return false;
    }

    hr = keyboard_->Acquire(); // キーボード入力を取得開始
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at Acquire Keyboard.")) {
        // 初回Acquireの失敗はUpdate内の再取得処理に任せる
    }

    hr = directInput->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr); // マウスデバイスを生成
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at CreateDevice Mouse.")) {
        Finalize();
        return false;
    }

    hr = mouse_->SetDataFormat(&c_dfDIMouse2); // マウスのデータ形式を設定
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at SetDataFormat Mouse.")) {
        Finalize();
        return false;
    }

    hr = mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE); // マウスの協調レベルを設定
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at SetCooperativeLevel Mouse.")) {
        Finalize();
        return false;
    }

    hr = mouse_->Acquire(); // マウス入力を取得開始
    if (!MYENGINE_CHECK_HRESULT(hr, "InputManager::Initialize failed at Acquire Mouse.")) {
        // 初回Acquireの失敗はUpdate内の再取得処理に任せる
    }

    return true;
}

/// <summary>
/// デバイスの解放・終了処理
/// </summary>
void InputManager::Finalize()
{
    if (keyboard_) {
        keyboard_->Unacquire(); // 入力停止
        keyboard_.Reset(); // デバイスを解放
    }

    if (mouse_) {
        mouse_->Unacquire(); // 入力停止
        mouse_.Reset(); // デバイスを解放
    }

    std::memset(keys_, 0, KEY_COUNT); // 現在のキー状態を破棄
    std::memset(preKeys_, 0, KEY_COUNT); // 前フレームのキー状態を破棄
    std::memset(&mouseState_, 0, sizeof(mouseState_)); // 現在のマウス状態を破棄
    std::memset(&preMouseState_, 0, sizeof(preMouseState_)); // 前フレームのマウス状態を破棄
    std::memset(gamePadStates_, 0, sizeof(gamePadStates_)); // 現在のゲームパッド状態を破棄
    std::memset(preGamePadStates_, 0, sizeof(preGamePadStates_)); // 前フレームのゲームパッド状態を破棄
    std::memset(gamePadConnected_, 0, sizeof(gamePadConnected_)); // 現在の接続状態を破棄
    std::memset(preGamePadConnected_, 0, sizeof(preGamePadConnected_)); // 前フレームの接続状態を破棄
}

/// <summary>
/// 毎フレーム呼び出して、キーボード・マウス・ゲームパッド状態を更新する
/// </summary>
void InputManager::Update()
{
    if (!keyboard_ || !mouse_) {
        std::memset(keys_, 0, KEY_COUNT); // 現在のキー状態を初期化
        std::memset(preKeys_, 0, KEY_COUNT); // 前フレームのキー状態を初期化
        std::memset(&mouseState_, 0, sizeof(mouseState_)); // 現在のマウス状態を初期化
        std::memset(&preMouseState_, 0, sizeof(preMouseState_)); // 前フレームのマウス状態を初期化
    } else {
        std::memcpy(preKeys_, keys_, KEY_COUNT); // 現在のキー状態を前フレームの状態にコピー
        if (FAILED(keyboard_->GetDeviceState(KEY_COUNT, keys_))) {
            keyboard_->Acquire(); // 再取得
            std::memset(keys_, 0, KEY_COUNT); // 取得に失敗した場合はキー状態をすべて離された状態にする
        }

        preMouseState_ = mouseState_; // 現在のマウス状態を前フレームの状態にコピー
        if (FAILED(mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_))) {
            mouse_->Acquire(); // 再取得
            std::memset(&mouseState_, 0, sizeof(mouseState_)); // 取得に失敗した場合はマウス状態をすべて初期化する
        }
    }

    for (uint32_t padIndex = 0; padIndex < GAMEPAD_COUNT; ++padIndex) {
        preGamePadStates_[padIndex] = gamePadStates_[padIndex]; // 現在のパッド状態を前フレームに保存
        preGamePadConnected_[padIndex] = gamePadConnected_[padIndex]; // 現在の接続状態を前フレームに保存
        std::memset(&gamePadStates_[padIndex], 0, sizeof(XINPUT_STATE)); // 今フレームの取得先を初期化
        const DWORD result = XInputGetState(static_cast<DWORD>(padIndex), &gamePadStates_[padIndex]); // XInputから現在状態を取得
        gamePadConnected_[padIndex] = result == ERROR_SUCCESS;
    }
}

/// <summary>
/// 指定キーが現在押されているか
/// </summary>
bool InputManager::IsKeyPressed(uint8_t key) const
{
    return keys_[key] & kInputPressedMask;
}

/// <summary>
/// 指定キーが現在離されているか
/// </summary>
bool InputManager::IsKeyReleased(uint8_t key) const
{
    return !(keys_[key] & kInputPressedMask);
}

/// <summary>
/// 指定キーが押された瞬間か
/// </summary>
bool InputManager::IsKeyJustPressed(uint8_t key) const
{
    return !(preKeys_[key] & kInputPressedMask)
        && (keys_[key] & kInputPressedMask);
}

/// <summary>
/// 指定キーが離された瞬間か
/// </summary>
bool InputManager::IsKeyJustReleased(uint8_t key) const
{
    return (preKeys_[key] & kInputPressedMask)
        && !(keys_[key] & kInputPressedMask);
}

/// <summary>
/// マウスボタン番号が有効範囲内か確認する
/// </summary>
bool InputManager::IsValidMouseButton(int button) const
{
    constexpr int kMouseButtonCount = static_cast<int>(sizeof(mouseState_.rgbButtons) / sizeof(mouseState_.rgbButtons[0])); // DirectInputで取得できるマウスボタン数
    return button >= 0 && button < kMouseButtonCount;
}

/// <summary>
/// 指定ボタンが押されているか
/// </summary>
bool InputManager::IsMouseButtonPressed(int button) const
{
    return IsValidMouseButton(button) && (mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// 指定ボタンが離されているか
/// </summary>
bool InputManager::IsMouseButtonReleased(int button) const
{
    return !IsValidMouseButton(button) || !(mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// 指定ボタンが押された瞬間か
/// </summary>
bool InputManager::IsMouseButtonJustPressed(int button) const
{
    return IsValidMouseButton(button)
        && !(preMouseState_.rgbButtons[button] & kInputPressedMask)
        && (mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// 指定ボタンが離された瞬間か
/// </summary>
bool InputManager::IsMouseButtonJustReleased(int button) const
{
    return IsValidMouseButton(button)
        && (preMouseState_.rgbButtons[button] & kInputPressedMask)
        && !(mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// マウスのX軸移動量を取得する
/// </summary>
long InputManager::GetMouseDeltaX() const
{
    return mouseState_.lX;
}

/// <summary>
/// マウスのY軸移動量を取得する
/// </summary>
long InputManager::GetMouseDeltaY() const
{
    return mouseState_.lY;
}

/// <summary>
/// ホイールの回転量を取得する
/// </summary>
long InputManager::GetMouseDeltaZ() const
{
    return mouseState_.lZ;
}

/// <summary>
/// ゲームパッド番号が有効範囲内か確認する
/// </summary>
bool InputManager::IsValidGamePadIndex(uint32_t padIndex) const
{
    return padIndex < GAMEPAD_COUNT;
}

/// <summary>
/// 指定したゲームパッドが接続されているか取得する
/// </summary>
bool InputManager::IsGamePadConnected(uint32_t padIndex) const
{
    return IsValidGamePadIndex(padIndex) && gamePadConnected_[padIndex];
}

/// <summary>
/// 指定したゲームパッドボタンが押されているか取得する
/// </summary>
bool InputManager::IsGamePadButtonPressed(WORD button, uint32_t padIndex) const
{
    return IsGamePadConnected(padIndex) && (gamePadStates_[padIndex].Gamepad.wButtons & button);
}

/// <summary>
/// 指定したゲームパッドボタンが押された瞬間か取得する
/// </summary>
bool InputManager::IsGamePadButtonJustPressed(WORD button, uint32_t padIndex) const
{
    if (!IsGamePadConnected(padIndex)) {
        return false;
    }

    const bool wasPressed = preGamePadConnected_[padIndex] && (preGamePadStates_[padIndex].Gamepad.wButtons & button); // 前フレームの押下状態
    const bool isPressed = gamePadStates_[padIndex].Gamepad.wButtons & button; // 現在の押下状態
    return !wasPressed && isPressed;
}

/// <summary>
/// 左スティックのX軸入力を取得する
/// </summary>
float InputManager::GetGamePadLeftStickX(uint32_t padIndex) const
{
    if (!IsGamePadConnected(padIndex)) {
        return 0.0f;
    }

    return NormalizeGamePadStickAxis(gamePadStates_[padIndex].Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

/// <summary>
/// 左スティックのY軸入力を取得する
/// </summary>
float InputManager::GetGamePadLeftStickY(uint32_t padIndex) const
{
    if (!IsGamePadConnected(padIndex)) {
        return 0.0f;
    }

    return NormalizeGamePadStickAxis(gamePadStates_[padIndex].Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}