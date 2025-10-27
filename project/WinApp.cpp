#include "WinApp.h"
#include <cassert>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

using namespace MyEngine;

WinApp* WinApp::GetInstance()
{
    static WinApp instance;
    return &instance;
}

/// <summary>
/// アプリケーションの初期化を行い、ウィンドウを生成します。
/// </summary>
void WinApp::Initialize(HINSTANCE hInstance, int nCmdShow, const std::wstring& title)
{
    // WinMainから渡されたハンドルとタイトルをメンバ変数に保持
    hInstance_ = hInstance;
    windowTitle_ = title;

    // ウィンドウクラス登録
    RegisterWindowClass();

    // クライアント領域のサイズを指定
    // RECT構造体で、希望するクライアント領域のサイズ（1280x720）を指定
    RECT wrc = { 0, 0, kWindowWidth, kWindowHeight };

    // ウィンドウの「枠（タイトルバーなど）」のサイズを計算し、ウィンドウ全体のサイズを調整する
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウ生成
    hwnd_ = CreateWindowEx(
        0, // 拡張スタイル（通常は0）
        kWindowClassName, // ウィンドウクラス名（RegisterClassExで登録したもの）
        windowTitle_.c_str(), // ウィンドウタイトル
        WS_OVERLAPPEDWINDOW, // ウィンドウスタイル（標準的なオーバーラップウィンドウ）
        CW_USEDEFAULT, // 初期X座標（OS任せ）
        CW_USEDEFAULT, // 初期Y座標（OS任せ）
        wrc.right - wrc.left, // 調整後のウィンドウ幅
        wrc.bottom - wrc.top, // 調整後のウィンドウ高さ
        nullptr, // 親ウィンドウハンドル（子ウィンドウではないのでnullptr）
        nullptr, // メニューハンドル
        hInstance_, // インスタンスハンドル
        nullptr); // 追加パラメータ

    // 【要件】デバッグ時に変数の値を観察できる（hwnd_がnullptrでないか確認）
    assert(hwnd_ != nullptr);

    // ウィンドウの表示（nCmdShowは最小化、最大化、通常表示などを指定）
    ShowWindow(hwnd_, nCmdShow);
}

/// <summary>
/// ウィンドウクラスの属性を設定し、システムに登録します。
/// </summary>
void WinApp::RegisterWindowClass()
{
    // WNDCLASSEX構造体の初期化と設定
    WNDCLASSEX w = {};
    w.cbSize = sizeof(WNDCLASSEX);
    w.lpfnWndProc = WindowProc; // ウィンドウプロシージャ（静的メンバ関数）のポインタを登録
    w.lpszClassName = kWindowClassName; // ウィンドウクラス名としてタイトルを使用
    w.hInstance = hInstance_; // インスタンスハンドル
    w.hCursor = LoadCursor(nullptr, IDC_ARROW); // マウスカーソルを設定

    // システムにウィンドウクラスを登録
    RegisterClassEx(&w);
}

/// <summary>
/// Windowsメッセージキューを処理します。WM_QUITを受け取った場合は false を返します。
/// </summary>
bool WinApp::ProcessMessage()
{
    MSG msg = {};
    // メッセージキューからメッセージを取得 (PM_REMOVEで取得後に削除)
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false; // メインループ終了のフラグ
        }
        TranslateMessage(&msg); // キーボード入力メッセージを文字メッセージに変換
        DispatchMessage(&msg); // ウィンドウプロシージャへメッセージを送信
    }
    return true; // ループ継続
}

/// <summary>
/// ウィンドウの破棄とウィンドウクラスの登録解除を行います。
/// </summary>
void WinApp::Finalize()
{
    if (hwnd_) {
        DestroyWindow(hwnd_); // ウィンドウの破棄
        hwnd_ = nullptr;
    }
    // ウィンドウクラスの登録解除
    UnregisterClass(windowTitle_.c_str(), hInstance_);
}

/// <summary>
/// Windowsから呼ばれるウィンドウプロシージャ（コールバック関数）です。
/// </summary>
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // ImGuiのメッセージ処理を先に通す
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true; // ImGuiが処理したメッセージならOSに渡さない
    }
    
    // 【要件】呼び出し履歴をたどれる（デバッグ時にコールバック関数への遷移を確認できる）
    switch (msg) {
    case WM_DESTROY: // ウィンドウがOSによって破壊される直前に呼ばれる
        PostQuitMessage(0); // メインループが終了するようにWM_QUITメッセージをキューに投稿
        return 0; // メッセージを処理したことを示す
    }
    // 処理しないメッセージは、OSのデフォルト処理に任せる
    return DefWindowProc(hwnd, msg, wparam, lparam);
}