#include "WinApp.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <cassert>

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
}

/// <summary>
/// ウィンドウクラスの登録
/// </summary>
void WinApp::RegisterWindowClass()
{
    WNDCLASSEX w = {};
    w.cbSize = sizeof(WNDCLASSEX);
    w.lpfnWndProc = WindowProc;
    w.lpszClassName = kWindowClassName;
    w.hInstance = hInstance_;
    w.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&w);
}

/// <summary>
/// メッセージ処理
/// </summary>
bool WinApp::ProcessMessage()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

/// <summary>
/// 終了処理
/// </summary>
void WinApp::Finalize()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (!windowTitle_.empty()) {
        UnregisterClass(kWindowClassName, hInstance_);
    }
}

/// <summary>
/// ウィンドウプロシージャ
/// </summary>
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
