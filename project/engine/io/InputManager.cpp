#include "InputManager.h"
#include "engine/utility/DebugUtility.h"
#include "Logger.h"
#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace MyEngine;

namespace {
constexpr uint8_t kInputPressedMask = 0x80; // DirectInputの押下状態を示すビット
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
    // ComPtr を使用しているため、Reset() を呼ぶだけでリソースが解放される
    if (keyboard_) {
        keyboard_->Unacquire(); // 入力停止
        keyboard_.Reset(); // デバイスを解放
    }

    // マウスデバイスも同様に解放
    if (mouse_) {
        mouse_->Unacquire(); // 入力停止
        mouse_.Reset(); // デバイスを解放
    }
}

/// <summary>
/// 毎フレーム呼び出して、キーボード・マウス状態を更新
/// </summary>
void InputManager::Update()
{
    if (!keyboard_ || !mouse_) {
        std::memset(keys_, 0, KEY_COUNT); // 現在のキー状態を初期化
        std::memset(preKeys_, 0, KEY_COUNT); // 前フレームのキー状態を初期化
        std::memset(&mouseState_, 0, sizeof(mouseState_)); // 現在のマウス状態を初期化
        std::memset(&preMouseState_, 0, sizeof(preMouseState_)); // 前フレームのマウス状態を初期化
        return;
    }

    // --- キーボードの状態更新 ---
    // 前フレームのキー状態を保存
    std::memcpy(preKeys_, keys_, KEY_COUNT); // 現在のキー状態を前フレームの状態にコピー
    // デバイスから現在のキー状態を取得
    if (FAILED(keyboard_->GetDeviceState(KEY_COUNT, keys_))) {
        keyboard_->Acquire(); // 再取得
        std::memset(keys_, 0, KEY_COUNT); // 取得に失敗した場合はキー状態をすべて離された状態にする
    }

    // --- マウスの状態更新 ---
    // 前フレームのマウス状態を保存
    preMouseState_ = mouseState_; // 現在のマウス状態を前フレームの状態にコピー
    // デバイスから現在のマウス状態を取得
    if (FAILED(mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_))) {
        mouse_->Acquire(); // 再取得
        std::memset(&mouseState_, 0, sizeof(mouseState_)); // 取得に失敗した場合はマウス状態をすべて初期化（移動なし、ボタン離された状態）にする
    }
}

/// <summary>
/// 指定キーが現在押されているか
/// </summary>
bool InputManager::IsKeyPressed(uint8_t key) const
{
    // キーが押されている場合、押下状態ビットがセットされる
    return keys_[key] & kInputPressedMask;
}

/// <summary>
/// 指定キーが現在離されているか
/// </summary>
bool InputManager::IsKeyReleased(uint8_t key) const
{
    // キーが離されている場合、押下状態ビットがセットされない
    return !(keys_[key] & kInputPressedMask);
}

/// <summary>
/// 指定キーが押された瞬間か
/// </summary>
bool InputManager::IsKeyJustPressed(uint8_t key) const
{
    // 前フレームでは押されていなくて、現在は押されている場合に true を返す
    return !(preKeys_[key] & kInputPressedMask)
        && (keys_[key] & kInputPressedMask);
}

/// <summary>
/// 指定キーが離された瞬間か
/// </summary>
bool InputManager::IsKeyJustReleased(uint8_t key) const
{
    // 前フレームでは押されていて、現在は離されている場合に true を返す
    return (preKeys_[key] & kInputPressedMask)
        && !(keys_[key] & kInputPressedMask);
}

// ------------------------
// マウス入力関連
// ------------------------

/// <summary>
/// マウスボタン番号が有効範囲内か確認する
/// </summary>
bool InputManager::IsValidMouseButton(int button) const
{
    constexpr int kMouseButtonCount = static_cast<int>(sizeof(mouseState_.rgbButtons) / sizeof(mouseState_.rgbButtons[0])); // DirectInputで取得できるマウスボタン数
    return button >= 0 && button < kMouseButtonCount;
}

/// <summary>
/// 指定ボタン（0～7）が押されているか
/// </summary>
bool InputManager::IsMouseButtonPressed(int button) const
{
    // 指定されたボタンの状態をチェック。押下状態ビットがセットされている場合は押されている
    return IsValidMouseButton(button) && (mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// 指定ボタンが離されているか
/// </summary>
bool InputManager::IsMouseButtonReleased(int button) const
{
    // 指定されたボタンの状態をチェック。押下状態ビットがセットされていない場合は離されている
    return !IsValidMouseButton(button) || !(mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// 指定ボタンが押された瞬間か
/// </summary>
bool InputManager::IsMouseButtonJustPressed(int button) const
{
    // 前フレームの状態と現在の状態を比較して、前フレームでは押されていなくて、現在は押されている場合に true を返す
    return IsValidMouseButton(button)
        && !(preMouseState_.rgbButtons[button] & kInputPressedMask)
        && (mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// 指定ボタンが離された瞬間か
/// </summary>
bool InputManager::IsMouseButtonJustReleased(int button) const
{
    // 前フレームの状態と現在の状態を比較して、前フレームでは押されていて、現在は離されている場合に true を返す
    return IsValidMouseButton(button)
        && (preMouseState_.rgbButtons[button] & kInputPressedMask)
        && !(mouseState_.rgbButtons[button] & kInputPressedMask);
}

/// <summary>
/// マウスのX軸移動量（フレーム間差分）
/// </summary>
long InputManager::GetMouseDeltaX() const
{
    // マウスのX軸移動量は、DIMOUSESTATE2構造体のlXメンバに格納されている
    return mouseState_.lX;
}

/// <summary>
/// マウスのY軸移動量（フレーム間差分）
/// </summary>
long InputManager::GetMouseDeltaY() const
{
    // マウスのY軸移動量は、DIMOUSESTATE2構造体のlYメンバに格納されている
    return mouseState_.lY;
}

/// <summary>
/// ホイールの回転量（Z軸）
/// </summary>
long InputManager::GetMouseDeltaZ() const
{
    // ホイールの回転量は、DIMOUSESTATE2構造体のlZメンバに格納されている
    return mouseState_.lZ;
}
