#pragma once

#include "PlayerState.h"

#include <vector>

/// <summary>
/// 分身再生に使用するプレイヤー状態の記録フレーム
/// </summary>
struct PastSelfFrame {
    float time = 0.0f; // 記録開始からの経過時間
    PlayerState state; // 記録時点のプレイヤー状態
};

/// <summary>
/// プレイヤー状態の記録を管理するクラス
/// </summary>
class PastSelfRecorder {
public:
    /// <summary>
    /// 記録状態と記録済みフレームを初期化する。
    /// </summary>
    void Clear();
    /// <summary>
    /// 記録を開始する。
    /// </summary>
    void Start();

    /// <summary>
    /// 記録を停止する。
    /// </summary>
    void Stop();

    /// <summary>
    /// 記録状態を切り替える。
    /// </summary>
    void Toggle();

    /// <summary>
    /// 記録中であればプレイヤー状態を追加する。
    /// </summary>
    void Update(float deltaTime, const PlayerState& playerState);

    /// <summary>
    /// ImGuiで記録状態を表示する。
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 記録中か取得する。
    /// </summary>
    bool IsRecording() const { return isRecording_; }

    /// <summary>
    /// 記録済みフレームを取得する。
    /// </summary>
    const std::vector<PastSelfFrame>& GetFrames() const { return frames_; }

    /// <summary>
    /// 記録済み時間を取得する。
    /// </summary>
    float GetDuration() const;

private:
    std::vector<PastSelfFrame> frames_; // 記録済みフレーム
    float elapsedTime_ = 0.0f; // 記録開始からの経過時間
    float sampleTimer_ = 0.0f; // 次の記録までの蓄積時間
    float sampleInterval_ = 1.0f / 60.0f; // 記録する時間間隔
    float maxRecordTime_ = 8.0f; // 記録できる最大時間
    bool isRecording_ = false; // 記録中か
};
