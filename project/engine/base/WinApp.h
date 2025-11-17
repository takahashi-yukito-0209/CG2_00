#pragma once
#include <Windows.h>
#include <string>

namespace MyEngine {

/// <summary>
/// Windowsアプリケーション管理クラス
/// （ウィンドウ生成・メッセージ処理・終了処理）
/// </summary>
class WinApp {
public:
    // --- 定数 ---
    static constexpr int kWindowWidth = 1280;
    static constexpr int kWindowHeight = 720;

public:
    // --- コンストラクタ / デストラクタ ---
    /// <summary>
    /// コンストラクタ：メンバ変数を初期化
    /// </summary>
    WinApp() = default;

    /// <summary>
    /// デストラクタ：Finalize()を自動的に呼ぶ
    /// </summary>
    ~WinApp() { Finalize(); }

    // コピー・ムーブは禁止（安全性確保）
    WinApp(const WinApp&) = delete;
    WinApp& operator=(const WinApp&) = delete;
    WinApp(WinApp&&) = delete;
    WinApp& operator=(WinApp&&) = delete;

public:
    // --- 初期化 / 終了 ---
    /// <summary>
    /// アプリケーションの初期化とウィンドウの生成
    /// </summary>
    void Initialize(HINSTANCE hInstance, int nCmdShow, const std::wstring& title = L"MyEngineApp");

    /// <summary>
    /// アプリケーションの終了処理とリソース解放
    /// </summary>
    void Finalize();

    // --- メッセージ処理 ---
    /// <summary>
    /// Windowsメッセージキューを処理。WM_QUITを受け取った場合は false を返す
    /// </summary>
    bool ProcessMessage(); // false を返したらWM_QUIT

    // --- Getter ---
    HWND GetHwnd() const { return hwnd_; }
    HINSTANCE GetHInstance() const { return hInstance_; }

private:
    /// <summary>
    /// ウィンドウクラスの属性を設定し、システムに登録
    /// </summary>
    void RegisterWindowClass();

    /// <summary>
    /// Windowsから呼ばれるウィンドウプロシージャ（コールバック関数）
    /// </summary>
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
    static constexpr LPCWSTR kWindowClassName = L"MyEngineWindowClass";

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    std::wstring windowTitle_;
};

} // namespace MyEngine
