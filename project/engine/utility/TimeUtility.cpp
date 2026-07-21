#include "TimeUtility.h"

namespace TimeUtility {

namespace {

constexpr float kFpsUpdateInterval = 1.0f; // FPS を更新する間隔秒数
constexpr float kMillisecondsPerSecond = 1000.0f; // 秒からミリ秒へ変換する係数

} // namespace

/// <summary>
/// タイマーを生成し、現在時刻を基準に初期化する。
/// </summary>
FrameTimer::FrameTimer()
{
    Reset();
}

/// <summary>
/// タイマーを現在時刻でリセットする。
/// </summary>
void FrameTimer::Reset()
{
    const auto now = std::chrono::steady_clock::now(); // 現在時刻
    startTime_ = now;
    previousTime_ = now;
    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
}

/// <summary>
/// 前回更新からの経過時間を更新する。
/// </summary>
void FrameTimer::Tick()
{
    const auto now = std::chrono::steady_clock::now(); // 現在時刻
    const std::chrono::duration<float> delta = now - previousTime_; // 前回更新からの経過時間
    const std::chrono::duration<float> total = now - startTime_; // 開始時刻からの経過時間

    deltaTime_ = delta.count();
    totalTime_ = total.count();
    previousTime_ = now;
}

/// <summary>
/// 直近フレームの経過秒数を取得する。
/// </summary>
float FrameTimer::GetDeltaTime() const
{
    return deltaTime_;
}

/// <summary>
/// リセット後からの合計経過秒数を取得する。
/// </summary>
float FrameTimer::GetTotalTime() const
{
    return totalTime_;
}

/// <summary>
/// FPS カウンターを生成する。
/// </summary>
FpsCounter::FpsCounter()
{
    Reset();
}

/// <summary>
/// 集計状態を初期化する。
/// </summary>
void FpsCounter::Reset()
{
    elapsedTime_ = 0.0f;
    fps_ = 0.0f;
    frameTimeMs_ = 0.0f;
    frameCount_ = 0;
}

/// <summary>
/// 1 フレーム分の経過時間を加算して FPS を更新する。
/// </summary>
void FpsCounter::Update(float deltaTime)
{
    if (deltaTime < 0.0f) {
        return;
    }

    elapsedTime_ += deltaTime;
    ++frameCount_;

    if (elapsedTime_ < kFpsUpdateInterval) {
        return;
    }

    fps_ = static_cast<float>(frameCount_) / elapsedTime_;
    frameTimeMs_ = elapsedTime_ / static_cast<float>(frameCount_) * kMillisecondsPerSecond;
    elapsedTime_ = 0.0f;
    frameCount_ = 0;
}

/// <summary>
/// 直近の集計 FPS を取得する。
/// </summary>
float FpsCounter::GetFps() const
{
    return fps_;
}

/// <summary>
/// 直近の平均フレーム時間をミリ秒で取得する。
/// </summary>
float FpsCounter::GetFrameTimeMs() const
{
    return frameTimeMs_;
}

} // namespace TimeUtility