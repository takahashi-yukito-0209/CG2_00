#include "WinApp.h"
#include <cassert>

#pragma comment(lib, "winmm.lib")

#include "DirectXCommon.h"
#include "ImGuiManager.h"

// ImGui のウィンドウプロシージャハンドラの宣言 (存在する場合のみ)
#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif

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
        0, // 拡張スタイルなし
        kWindowClassName, // ウィンドウクラス名
        windowTitle_.c_str(), // ウィンドウタイトル
        WS_OVERLAPPEDWINDOW, // ウィンドウスタイル
        CW_USEDEFAULT, // X座標（デフォルト）
        CW_USEDEFAULT, // Y座標（デフォルト）
        wrc.right - wrc.left, // ウィンドウの幅
        wrc.bottom - wrc.top, // ウィンドウの高さ
        nullptr, // 親ウィンドウなし
        nullptr, // メニューハンドルなし
        hInstance_, // インスタンスハンドル
        nullptr // 追加パラメータなし
    );

    // ウィンドウ生成に失敗した場合はエラー
    assert(hwnd_ != nullptr);

    // ウィンドウを表示
    ShowWindow(hwnd_, nCmdShow);

    // システムタイマーの分解能を上げる
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

    case WM_SIZE:
        // WM_SIZE は連続で来るため短いデバウンスを行い、最終的なサイズだけ通知する
        // 単純化のため WindowProc 内の静的変数で保管する
        {
            static const UINT_PTR kResizeTimerId = 1;
            static UINT pendingWidth = 0;
            static UINT pendingHeight = 0;

            pendingWidth = LOWORD(lparam);
            pendingHeight = HIWORD(lparam);
            // 既存のタイマーをクリアして新たにセット（100ms デバウンス）
            KillTimer(hwnd, kResizeTimerId);
            SetTimer(hwnd, kResizeTimerId, 100, nullptr);
        }
        return 0;

    case WM_TIMER: {
        static const UINT_PTR kResizeTimerId = 1;
        if (wparam == kResizeTimerId) {
            // タイマーが発火したときに、最新のクライアントサイズを取得してリサイズ処理を呼び出す
            RECT rc;
            if (GetClientRect(hwnd, &rc)) {
                UINT w = static_cast<UINT>(rc.right - rc.left);
                UINT h = static_cast<UINT>(rc.bottom - rc.top);
                if (w != 0 && h != 0) {
                    DirectXCommon::GetInstance()->OnWindowResize(w, h);
                }
            }
            KillTimer(hwnd, kResizeTimerId);
        }
    }
        return 0;
    }

    // ImGui のハンドラは最後にする（ImGui 有効時のみ）
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return 1;
    }
#endif

    return DefWindowProc(hwnd, msg, wparam, lparam); // デフォルトのウィンドウプロシージャにメッセージを送る
}
