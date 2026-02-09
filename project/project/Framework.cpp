#include "Framework.h"
#include <chrono>
#include <thread>
#include <windowsx.h>
#include <objbase.h>
#include <atomic>


// 共通のラン処理を実装する
// - Initialize -> ゲームループ(Update/Draw) -> Finalize
int Framework::Run(HINSTANCE hInstance, int nCmdShow)
{
    // 共通初期化: COM をマルチスレッドモデルで初期化しておく
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitialized = SUCCEEDED(hr);

    // 派生クラスの初期化を呼び出す
    if (!Initialize(hInstance, nCmdShow)) {
        if (comInitialized) CoUninitialize();
        return 1;
    }

    // メインループ: 固定タイムステップに近い動作でフレームレートを安定させる
    const double targetSec = 1.0 / targetFPS_;
    using clock = std::chrono::high_resolution_clock;
    auto prev = clock::now();
    double accumulator = 0.0;

    while (!IsEndRequest()) {
        // イベントのポーリングでウィンドウ終了等を検出できるようにする
        if (!PollEvents()) {
            break;
        }

        auto now = clock::now();
        std::chrono::duration<double> frameTime = now - prev;
        prev = now;
        accumulator += frameTime.count();

        // Update/Draw を固定間隔で実行（ループ内で複数回 Update することは稀だが許容）
        bool updatedThisFrame = false;
        while (accumulator >= targetSec && !IsEndRequest()) {
            Update();
            updatedThisFrame = true;
            accumulator -= targetSec;
        }

        // 安全措置: Update が一度も呼ばれなかった場合、アプリ固有で毎フレーム行う処理
        // (例: ImGui::NewFrame を含む処理)。Update を必ず 1 回呼ぶことで
        // Draw 側で ImGui::Render が呼ばれても NewFrame が未実行でクラッシュする
        // リスクを回避する。
        if (!updatedThisFrame && !IsEndRequest()) {
            Update();
        }

        if (IsEndRequest()) break;

        Draw();

        // 残り時間があればスリープで待機し、微小時間はスピンで調整する（精度向上）
        auto frameEnd = clock::now();
        std::chrono::duration<double> elapsed = frameEnd - prev;
        double sleepSec = targetSec - elapsed.count() - accumulator;
        if (sleepSec > 0.002) { // 少なくとも 2ms を超えるならスリープ
            auto sleepMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(sleepSec));
            std::this_thread::sleep_for(sleepMs);
        }
        // スピンで残りを待つ
        while ((clock::now() - prev).count() / static_cast<double>(clock::period::den) < targetSec - accumulator) {
            // busy wait short
            std::this_thread::yield();
            if (IsEndRequest()) break;
        }
    }

    // 終了処理
    Finalize();
    if (comInitialized) CoUninitialize();
    return 0;
}


// 基底でのメッセージループ実装
bool Framework::PollEvents()
{
    MSG msg;
    // ノンブロッキングでメッセージを取得する
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            endRequested_ = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}
