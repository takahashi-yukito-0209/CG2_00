#pragma once
#include <Windows.h>
#include <string>

namespace MyEngine {

/// <summary>
/// Windowsアプリケーション管理クラス
/// </summary>
class WinApp {
public:
    // --- 定数 ---
    static constexpr int kWindowWidth = 1280; // ウィンドウの幅（定数）
    static constexpr int kWindowHeight = 720; // ウィンドウの高さ（定数）

public: // メンバ関数
    /// <summary>
    /// コンストラクタ：メンバ変数を初期化
    /// </summary>
    WinApp() = default;

    /// <summary>
    /// デストラクタ：Finalize()を自動的に呼ぶ
    /// </summary>
    ~WinApp() { Finalize(); }

    // コピー・ムーブは禁止（安全性確保）
    WinApp(const WinApp&) = delete; // コピーコンストラクタを削除してコピーを禁止
    WinApp& operator=(const WinApp&) = delete; // コピー代入演算子を削除してコピーを禁止
    WinApp(WinApp&&) = delete; // ムーブコンストラクタを削除してムーブを禁止
    WinApp& operator=(WinApp&&) = delete; // ムーブ代入演算子を削除してムーブを禁止

public: // メンバ関数
    /// <summary>
    /// アプリケーションの初期化とウィンドウの生成
    /// </summary>
    void Initialize(HINSTANCE hInstance, int nCmdShow, const std::wstring& title = L"MyEngineApp");

    /// <summary>
    /// アプリケーションの終了処理とリソース解放
    /// </summary>
    void Finalize();

    /// <summary>
    /// Windowsメッセージキューを処理。WM_QUITを受け取った場合は false を返す
    /// </summary>
    bool ProcessMessage(); // false を返したらWM_QUIT

    /// <summary>
    /// ウィンドウハンドルのゲッター
    /// </summary>
    HWND GetHwnd() const { return hwnd_; }

    /// <summary>
    /// インスタンスハンドルのゲッター
    /// </summary>
    HINSTANCE GetHInstance() const { return hInstance_; }

private: // メンバ関数
    /// <summary>
    /// ウィンドウクラスの属性を設定し、システムに登録
    /// </summary>
    void RegisterWindowClass();

    /// <summary>
    /// Windowsから呼ばれるウィンドウプロシージャ（コールバック関数）
    /// </summary>
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private: // メンバ変数
    // ウィンドウクラス名（定数）
    static constexpr LPCWSTR kWindowClassName = L"MyEngineWindowClass";

    HINSTANCE hInstance_ = nullptr; // アプリケーションインスタンスハンドル
    HWND hwnd_ = nullptr; // ウィンドウハンドル
    std::wstring windowTitle_; // ウィンドウタイトル（オプションで指定されたものを保存）
};

} // namespace MyEngine
