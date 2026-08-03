#include "PastSelfRecorder.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

#include <algorithm>

namespace {
constexpr float kMinimumSampleInterval = 1.0f / 120.0f; // 最小記録間隔
constexpr float kMaximumSampleInterval = 0.2f; // 最大記録間隔
constexpr float kMinimumRecordTime = 0.5f; // 最小記録時間
constexpr float kMaximumRecordTime = 30.0f; // 最大記録時間
constexpr float kImGuiTimeStep = 0.01f; // 時間調整の刻み
}

/// <summary>
/// 記録状態と記録済みフレームを初期化する。
/// </summary>
void PastSelfRecorder::Clear()
{
    frames_.clear();
    elapsedTime_ = 0.0f;
    sampleTimer_ = 0.0f;
    isRecording_ = false;
}

/// <summary>
/// 記録を開始する。
/// </summary>
void PastSelfRecorder::Start()
{
    Clear();
    isRecording_ = true;
}

/// <summary>
/// 記録を停止する。
/// </summary>
void PastSelfRecorder::Stop()
{
    isRecording_ = false;
}

/// <summary>
/// 記録状態を切り替える。
/// </summary>
void PastSelfRecorder::Toggle()
{
    if (isRecording_) {
        Stop();
    } else {
        Start();
    }
}

/// <summary>
/// 記録中であればプレイヤー状態を追加する。
/// </summary>
void PastSelfRecorder::Update(float deltaTime, const PlayerState& playerState)
{
    if (!isRecording_) {
        return;
    }

    elapsedTime_ += deltaTime;
    sampleTimer_ += deltaTime;
    if (elapsedTime_ > maxRecordTime_) {
        Stop();
        return;
    }

    if (!frames_.empty() && sampleTimer_ < sampleInterval_) {
        return;
    }

    PastSelfFrame frame {}; // 追加する記録フレーム
    frame.time = elapsedTime_;
    frame.state = playerState;
    frames_.push_back(frame);
    sampleTimer_ = 0.0f;
}

/// <summary>
/// 記録済み時間を取得する。
/// </summary>
float PastSelfRecorder::GetDuration() const
{
    if (frames_.size() < 2) {
        return 0.0f;
    }

    return frames_.back().time - frames_.front().time;
}

/// <summary>
/// ImGuiで記録状態を表示する。
/// </summary>
void PastSelfRecorder::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Recording: %s", isRecording_ ? "true" : "false");
    ImGui::Text("Frames: %zu", frames_.size());
    ImGui::Text("Duration: %.2f sec", GetDuration());
    ImGui::DragFloat("Max Record Time", &maxRecordTime_, kImGuiTimeStep, kMinimumRecordTime, kMaximumRecordTime, "%.2f sec");
    ImGui::DragFloat("Sample Interval", &sampleInterval_, kImGuiTimeStep, kMinimumSampleInterval, kMaximumSampleInterval, "%.3f sec");
    maxRecordTime_ = (std::clamp)(maxRecordTime_, kMinimumRecordTime, kMaximumRecordTime);
    sampleInterval_ = (std::clamp)(sampleInterval_, kMinimumSampleInterval, kMaximumSampleInterval);
#endif
}
