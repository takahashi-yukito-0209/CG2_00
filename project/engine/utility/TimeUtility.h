#pragma once

#include <chrono>

/// <summary>
/// 時間計測に関するユーティリティ名前空間。
/// </summary>
namespace TimeUtility {

/// <summary>
/// フレーム単位の経過時間を計測するタイマー。
/// </summary>
class FrameTimer {
public:
    /// <summary>
    /// タイマーを生成し、現在時刻を基準に初期化する。
    /// </summary>
    FrameTimer();

    /// <summary>
    /// タイマーを現在時刻でリセットする。
    /// </summary>
    void Reset();

    /// <summary>
    /// 前回更新からの経過時間を更新する。
    /// </summary>
    void Tick();

    /// <summary>
    /// 直近フレームの経過秒数を取得する。
    /// </summary>
    float GetDeltaTime() const;

    /// <summary>
    /// リセット後からの合計経過秒数を取得する。
    /// </summary>
    float GetTotalTime() const;

private:
    std::chrono::steady_clock::time_point startTime_; // 計測開始時刻
    std::chrono::steady_clock::time_point previousTime_; // 前回更新時刻
    float deltaTime_ = 0.0f; // 直近フレームの経過秒数
    float totalTime_ = 0.0f; // リセット後からの合計経過秒数
};

/// <summary>
/// 一定間隔ごとの FPS とフレーム時間を集計するカウンター。
/// </summary>
class FpsCounter {
public:
    /// <summary>
    /// FPS カウンターを生成する。
    /// </summary>
    FpsCounter();

    /// <summary>
    /// 集計状態を初期化する。
    /// </summary>
    void Reset();

    /// <summary>
    /// 1 フレーム分の経過時間を加算して FPS を更新する。
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// 直近の集計 FPS を取得する。
    /// </summary>
    float GetFps() const;

    /// <summary>
    /// 直近の平均フレーム時間をミリ秒で取得する。
    /// </summary>
    float GetFrameTimeMs() const;

private:
    float elapsedTime_ = 0.0f; // 集計中の経過秒数
    float fps_ = 0.0f; // 直近の FPS
    float frameTimeMs_ = 0.0f; // 直近の平均フレーム時間ミリ秒
    int frameCount_ = 0; // 集計中のフレーム数
};

} // namespace TimeUtility