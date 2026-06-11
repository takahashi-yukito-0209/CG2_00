#include "InputManager.h"
#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace MyEngine;

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
InputManager* InputManager::GetInstance()
{
    static InputManager instance; // 静的ローカル変数でシングルトンインスタンスを生成
    return &instance; // 以降、GetInstance() を呼ぶたびに同じインスタンスが返される
}

/// <summary>
/// DirectInput デバイスの初期化（キーボード・マウス）
/// </summary>
bool InputManager::Initialize(IDirectInput8* directInput, HWND hwnd)
{
    HRESULT hr; // 結果コード格納用変数

    // --- キーボードの初期化 ---
    // DirectInput デバイスを生成（GUID_SysKeyboard を指定してキーボードデバイスを作成）
    hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
    if (FAILED(hr)) {
        return false; // デバイスの生成に失敗した場合は false を返す
    }

    // データフォーマットの設定と協調レベルの設定
    keyboard_->SetDataFormat(&c_dfDIKeyboard); // キーボードのデータフォーマットを指定
    keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE); // フォアグラウンドで非排他モード
    keyboard_->Acquire(); // 入力開始

    // キー状態の初期化
    std::memset(keys_, 0, KEY_COUNT); // 現在のキー状態を初期化
    std::memset(preKeys_, 0, KEY_COUNT); // 前フレームのキー状態も初期化

    // --- マウスの初期化 ---
    // DirectInput デバイスを生成（GUID_SysMouse を指定してマウスデバイスを作成）
    hr = directInput->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr);
    if (FAILED(hr)) {
        return false; // デバイスの生成に失敗した場合は false を返す
    }

    // データフォーマットの設定と協調レベルの設定
    mouse_->SetDataFormat(&c_dfDIMouse2); // 拡張マウス情報（ホイール含む）
    mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE); // フォアグラウンドで非排他モード
    mouse_->Acquire(); // 入力開始

    // マウス状態の初期化
    std::memset(&mouseState_, 0, sizeof(mouseState_)); // 現在のマウス状態を初期化
    std::memset(&preMouseState_, 0, sizeof(preMouseState_)); // 前フレームのマウス状態も初期化

    return true; // すべての初期化が成功した場合は true を返す
}

/// <summary>
/// デバイスの解放（終了処理）
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
    // キーが押されている場合、最上位ビット（0x80）がセットされる
    return keys_[key] & 0x80;
}

/// <summary>
/// 指定キーが現在離されているか
/// </summary>
bool InputManager::IsKeyReleased(uint8_t key) const
{
    // キーが離されている場合、最上位ビット（0x80）がセットされない
    return !(keys_[key] & 0x80);
}

/// <summary>
/// 指定キーが押された瞬間か
/// </summary>
bool InputManager::IsKeyJustPressed(uint8_t key) const
{
    //// 前フレームでは押されていて、現在は離されている場合に true を返す
    return !(preKeys_[key] & 0x80) && (keys_[key] & 0x80);
}

/// <summary>
/// 指定キーが離された瞬間か
/// </summary>
bool InputManager::IsKeyJustReleased(uint8_t key) const
{
    // 前フレームでは押されていて、現在は離されている場合に true を返す
    return (preKeys_[key] & 0x80) && !(keys_[key] & 0x80);
}

// ------------------------
// マウス入力関連
// ------------------------

/// <summary>
/// 指定ボタン（0～7）が押されているか
/// </summary>
bool InputManager::IsMouseButtonPressed(int button) const
{
    // 指定されたボタンの状態をチェック。最上位ビット（0x80）がセットされている場合は押されている
    return mouseState_.rgbButtons[button] & 0x80;
}

/// <summary>
/// 指定ボタンが離されているか
/// </summary>
bool InputManager::IsMouseButtonReleased(int button) const
{
    // 指定されたボタンの状態をチェック。最上位ビット（0x80）がセットされていない場合は離されている
    return !(mouseState_.rgbButtons[button] & 0x80);
}

/// <summary>
/// 指定ボタンが押された瞬間か
/// </summary>
bool InputManager::IsMouseButtonJustPressed(int button) const
{
    // 前フレームの状態と現在の状態を比較して、前フレームでは押されていなくて、現在は押されている場合に true を返す
    return !(preMouseState_.rgbButtons[button] & 0x80) && (mouseState_.rgbButtons[button] & 0x80);
}

/// <summary>
/// 指定ボタンが離された瞬間か
/// </summary>
bool InputManager::IsMouseButtonJustReleased(int button) const
{
    // 前フレームの状態と現在の状態を比較して、前フレームでは押されていて、現在は離されている場合に true を返す
    return (preMouseState_.rgbButtons[button] & 0x80) && !(mouseState_.rgbButtons[button] & 0x80);
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
