#include "WinApp.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <cassert>

#pragma comment(lib, "winmm.lib")

// ImGui のウィンドウプロシージャハンドラの宣言
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

using namespace MyEngine;

/// <summary>
/// アプリケーションの初期化を行い、ウィンドウを生成します。
/// </summary>
void WinApp::Initialize(HINSTANCE hInstance, int nCmdShow, const std::wstring& title)
{
    if (hwnd_) {
        return; // すでに初期化済みならスキップ
    }

    // ハンドルとタイトルをメンバ変数に保持
    hInstance_ = hInstance;
    windowTitle_ = title;

    // ウィンドウクラス登録
    RegisterWindowClass();

    // クライアント領域のサイズ指定
    RECT wrc = { 0, 0, kWindowWidth, kWindowHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウ生成
    hwnd_ = CreateWindowEx(
        0,
        kWindowClassName,
        windowTitle_.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        hInstance_,
        nullptr);

    assert(hwnd_ != nullptr);

    // ウィンドウを表示
    ShowWindow(hwnd_, nCmdShow);

    //システムタイマーの分解能を上げる
    timeBeginPeriod(1);
}

/// <summary>
/// ウィンドウクラスの登録
/// </summary>
void WinApp::RegisterWindowClass()
{
    WNDCLASSEX w = {};
    w.cbSize = sizeof(WNDCLASSEX); // 構造体のサイズを指定
    w.lpfnWndProc = WinApp::WindowProc; // ウィンドウプロシージャを指定
    w.lpszClassName = kWindowClassName; // ウィンドウクラス名を指定
    w.hInstance = hInstance_; // インスタンスハンドルを指定
    w.hCursor = LoadCursor(nullptr, IDC_ARROW); // カーソルを指定

    RegisterClassEx(&w); // ウィンドウクラスをシステムに登録
}

/// <summary>
/// メッセージ処理
/// </summary>
bool WinApp::ProcessMessage()
{
    MSG msg = {};
    // メッセージキューにメッセージがある限りループして処理
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }

        TranslateMessage(&msg); // キーボードメッセージの処理
        DispatchMessage(&msg); // ウィンドウプロシージャにメッセージを送る
    }

    return true; // WM_QUITを受け取ったら false を返す
}

/// <summary>
/// 終了処理
/// </summary>
void WinApp::Finalize()
{
    // システムタイマーの分解能を元に戻す
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    // ウィンドウクラスの登録を解除
    if (!windowTitle_.empty()) {
        UnregisterClass(kWindowClassName, hInstance_);
    }
}

/// <summary>
/// ウィンドウプロシージャ
/// </summary>
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // アプリ側のメッセージ処理を先に行う
    switch (msg) {
        // ウィンドウが破棄されたときの処理
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // ImGui のハンドラは最後にする
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return 1;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam); // デフォルトのウィンドウプロシージャにメッセージを送る
}
