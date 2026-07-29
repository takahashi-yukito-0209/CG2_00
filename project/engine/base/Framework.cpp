#include "engine/base/Framework.h"
#include "engine/2d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/WinApp.h"
#include "engine/utility/Logger.h"
#include "engine/utility/ResourceResolver.h"
#include "engine/utility/TimeUtility.h"
#include <atomic>
#include <chrono>
#include <objbase.h>
#include <thread>
#include <windowsx.h>

using namespace MyEngine;

/// <summary>
/// エンジンの初期化処理をまとめて行うユーティリティ関数
/// </summary>
bool Framework::InitializeEngine(HINSTANCE hInstance, WinApp* winApp, HWND hwnd,
    std::unique_ptr<SpriteCommon>& spriteCommonOut,
    SrvManager& srvManagerOut,
    std::unique_ptr<Object3dCommon>& object3dCommonOut)
{

    // 引数の未使用警告を抑制
    (void)hInstance;
    (void)hwnd;

    // WinAppの有効性を確認
    if (!winApp) {
        return false;
    }

    // DirectXCommonの初期化
    DirectXCommon::GetInstance()->Initialize(winApp);

    // SrvManagerの初期化
    srvManagerOut.Initialize(DirectXCommon::GetInstance());

    // DirectXCommon に SrvManager を登録しておく（RenderTarget の破棄時に SRV を解放するため）
    DirectXCommon::GetInstance()->SetSrvManager(&srvManagerOut);

    // SpriteCommonのインスタンスを作成
    spriteCommonOut = std::make_unique<SpriteCommon>();
    // SpriteCommonの初期化
    spriteCommonOut->Initialize(DirectXCommon::GetInstance());

    // TextureManagerの初期化
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), &srvManagerOut);

    // Object3dCommonのインスタンスを作成
    object3dCommonOut = std::make_unique<Object3dCommon>();
    // Object3dCommonの初期化
    object3dCommonOut->Initialize(DirectXCommon::GetInstance(), &srvManagerOut);

    // すべての初期化が成功した場合は true を返す
    return true;
}

/// <summary>
/// エンジンの終了処理をまとめて行うユーティリティ関数
/// </summary>
void Framework::FinalizeEngine()
{
    // TextureManagerの終了処理を呼び出す
    TextureManager::GetInstance()->Finalize();
    // DirectXCommonの終了処理を呼び出す
    DirectXCommon::GetInstance()->Finalize();
}

/// <summary>
/// アプリケーションの実行: 初期化、メインループ、終了処理をまとめて行う
/// </summary>
int Framework::Run(HINSTANCE hInstance, int nCmdShow)
{
    ResourceResolver::SetWorkingDirectoryToExecutable();
    // このスレッドでCOMを初期化し、成功した場合だけ終了責務を持つ
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED); // COM初期化結果
    const bool ownsComInitialization = SUCCEEDED(comResult); // CoUninitializeを呼ぶ責務の有無
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        Logger::Error("Framework::Run: CoInitializeEx failed.\n");
        return 1;
    }

    // 派生クラスの初期化を呼び出す
    if (!Initialize(hInstance, nCmdShow)) {
        // 初期化失敗: 終了処理を行ってからアプリケーションを終了する
        if (ownsComInitialization) {
            CoUninitialize();
        }
        return 1; // 終了コード 1 を返して異常終了を示す
    }

    // 目標FPSに基づいてフレーム時間を計算
    const double targetSec = 1.0 / targetFPS_; // フレーム時間の計算に高精度なクロックを使用
    using clock = std::chrono::high_resolution_clock; // 高精度なクロックを使用してフレーム時間を計測
    TimeUtility::FrameTimer frameTimer; // フレーム間の経過時間を計測するタイマー
    auto frameStart = clock::now(); // 現在フレームの開始時間
    double accumulator = 0.0; // フレーム時間の累積

    // メインループ: 終了要求が出るまで続ける
    while (!IsEndRequest()) {
        // イベントのポーリングでウィンドウ終了等を検出できるようにする
        if (!PollEvents()) {
            break;
        }

        // フレーム時間の計測と累積
        frameTimer.Tick();
        frameStart = clock::now();
        accumulator += static_cast<double>(frameTimer.GetDeltaTime());

        // フレーム時間が目標を超えている場合、Update を呼び出してゲームロジックを更新する
        bool updatedThisFrame = false;
        // 累積時間が目標フレーム時間を超えている限り Update を呼び出す（複数回呼ぶ可能性もある）
        while (accumulator >= targetSec && !IsEndRequest()) {
            Update();
            updatedThisFrame = true;
            accumulator -= targetSec;
        }

        // Update が呼ばれなかった場合でも、終了要求が出ていないなら Update を呼び出して状態を更新する
        if (!updatedThisFrame && !IsEndRequest()) {
            Update();
        }

        // 描画処理を呼び出す前に終了要求が出ていないか確認する
        if (IsEndRequest()) {
            break;
        }

        // 描画処理を呼び出す
        Draw();

        // フレーム時間が目標を下回っている場合、スリープやスピンで待機してフレームレートを制御する
        auto frameEnd = clock::now();
        std::chrono::duration<double> elapsed = frameEnd - frameStart;
        double sleepSec = targetSec - elapsed.count() - accumulator;
        // スリープで待機する時間が十分にある場合はスリープする（ただし、スリープの精度を考慮して少し余裕を持たせる）
        if (sleepSec > 0.002) { // 少なくとも 2ms を超えるならスリープ
            auto sleepMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(sleepSec));
            std::this_thread::sleep_for(sleepMs);
        }

        // スリープ後もフレーム時間が目標を下回っている場合は、スピンで待機して精度を高める
        while (std::chrono::duration<double>(clock::now() - frameStart).count() < targetSec - accumulator) {
            // スピンで待機: CPUを占有して正確なフレーム時間を維持する
            std::this_thread::yield();
            if (IsEndRequest()) {
                break;
            }
        }
    }

    Finalize(); // 派生クラスの終了処理を呼び出す

    // COMの終了処理
    if (ownsComInitialization) {
        CoUninitialize();
    }

    return 0;
}

/// <summary>
/// Windows メッセージをポーリングし、WM_QUIT を検出すると終了要求フラグを立てて false を返す
/// </summary>
bool Framework::PollEvents()
{
    MSG msg;
    // メッセージキューからメッセージを取得して処理する
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        // WM_QUIT メッセージが来たら終了要求とみなしてフラグを立て、false を返す
        if (msg.message == WM_QUIT) {
            endRequested_ = true;
            return false;
        }
        // それ以外のメッセージは通常通り処理する
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true; // メッセージキューに WM_QUIT がなければ true を返して続行可能とする
}
