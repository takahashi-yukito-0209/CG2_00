#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/// <summary>
/// ギミックの現在状態。通常リセットと再生準備で初期化する。
/// </summary>
struct PastSelfTutorialGimmickState {
    bool switchActive = false; // 分身専用スイッチが押されているか
    bool doorOpen = false; // 通常扉が開いているか
    bool goalReached = false; // ゴールに到達したか
    bool doorUnlockedByClone = false; // 分身入力で通常扉を開放済みか
    bool playerOnSwitch = false; // プレイヤーが分身専用スイッチ上にいるか
    bool cloneOnSwitch = false; // 分身が分身専用スイッチ上にいるか
    bool timedSwitchActive = false; // 時間差スイッチが起動中か
    bool timedSwitchCloneOn = false; // 分身が時間差スイッチ上にいるか
    bool timedDoorOpen = false; // 時間差扉が開いているか
    bool toggleSwitchActive = false; // トグルスイッチがONか
    bool toggleSwitchCloneOn = false; // 分身がトグルスイッチ上にいるか
    bool toggleGateOpen = false; // トグル連動ゲートが開いているか
    bool toggleElevatorActive = false; // 昇降足場が稼働中か
    bool weightSwitchActive = false; // 重さスイッチが起動中か
    bool weightPlayerOn = false; // プレイヤーが重さスイッチ上にいるか
    bool weightCloneOn = false; // 分身が重さスイッチ上にいるか
    bool goalBridgeDeployed = false; // ゴール前の橋が展開されているか
    bool oneWayGateBlocking = false; // 一方通行ゲートが戻りを塞いでいるか
};

/// <summary>
/// 再生ごとの進捗。再生準備で実績とは別に初期化する。
/// </summary>
struct PastSelfTutorialReplayState {
    bool oneCloneToggleActivated = false; // 1体ルートで分身がトグルスイッチを起動したか
    bool oneCloneElevatorRidden = false; // 1体ルートで昇降足場に乗ったか
    bool oneCloneBasicsComplete = false; // 1体ルートを完了したか
    bool twoCloneReplayPrepared = false; // 記録2回・分身2体で再生準備したか
    bool twoCloneSwitchesActivated = false; // 2体ルートで別々の分身がスイッチを起動したか
    bool twoCloneCooperationComplete = false; // 2体ルートで青扉を通過したか
    bool routeClearFinalized = false; // 現在のルートの結果を確定したか
    bool recordingPendingCommit = false; // 現在の記録を分身として保存する必要があるか
    float clearTime = 0.0f; // 今回のクリア時点の経過時間
    float prepareFeedbackSeconds = 0.0f; // Prepare成功表示を残す秒数
};

/// <summary>
/// 挑戦中に蓄積する実績と計測値。通常リセットで消し、再生準備では残す。
/// </summary>
struct PastSelfTutorialProgressState {
    bool doorBlockedBeforeClone = false; // 分身なしで閉じた扉に阻まれたか
    bool clonePlatformUsed = false; // 分身を足場として利用したか
    bool doorOpenedByClone = false; // 分身が通常扉を開けたか
    bool goalBridgeUnlocked = false; // 重さスイッチで橋を解放したか
    bool timedDoorOpened = false; // 時間差扉を開けたか
    bool dualCloneSwitchesActivated = false; // 複数分身で離れたスイッチを同時起動したか
    bool weightSwitchActivated = false; // 重さスイッチを起動したか
    bool oneWayGateUsed = false; // 一方通行ゲートを通過したか
    bool resetShown = false; // リセット開始をHUDで示すか
    bool recordStarted = false; // 記録を開始したか
    bool recordStopped = false; // 記録を停止したか
    bool prepareUsed = false; // Prepare操作を使ったか
    bool replayStarted = false; // 保存分身の再生を開始したか
    float elapsedTime = 0.0f; // 挑戦開始からの経過時間
    float lastRecordDuration = 0.0f; // 最後に確定した分身記録時間
    uint32_t recordTakeCount = 0; // 挑戦中に開始した記録回数
    std::string recentCheckText; // 再生準備後も表示する直近の達成通知
    float recentCheckSeconds = 0.0f; // 達成通知を表示する残り時間
};

/// <summary>
/// シーン滞在中の通算クリア履歴。通常リセットと再生準備では残す。
/// </summary>
struct PastSelfTutorialRouteHistory {
    bool oneCloneRouteCleared = false; // 1体ルートを通算でクリア済みか
    bool twoCloneRouteCleared = false; // 2体ルートを通算でクリア済みか
    bool finalChallengeCleared = false; // 最終課題を通算でクリア済みか
};

/// <summary>
/// HUDと手順案内で共有する達成状況。挑戦状態や通算履歴は変更しない。
/// </summary>
struct PastSelfTutorialStatus {
    int completedCheckCount = 0; // 最終課題の達成済み検証項目数
    int completedFlowCount = 0; // 達成済みの動画操作項目数
    int oneCloneTutorialCheckCount = 0; // 1体ルートの達成済み項目数
    int twoCloneTutorialCheckCount = 0; // 2体ルートの達成済み項目数
    bool allChecksComplete = false; // ゴールを含む7項目をすべて達成したか
    bool videoFlowComplete = false; // 動画操作の5項目をすべて達成したか
    bool enoughStoredClones = false; // 最終課題の検証に必要な分身数があるか
    bool multiCloneRouteComplete = false; // 最終課題の検証条件をすべて満たしたか
    bool allRoutesComplete = false; // 3ルートを通算でクリア済みか
    bool oneCloneRecordingStored = false; // 記録1回かつ分身1体で記録を停止済みか
    bool twoCloneRecordingsStored = false; // 記録2回かつ分身2体で記録を停止済みか
};

/// <summary>
/// チュートリアル状態の寿命とリセット方針を管理する。描画や分身操作は持たない。
/// </summary>
struct PastSelfTutorialState {
    PastSelfTutorialGimmickState gimmicks; // 現在の衝突・起動状態
    PastSelfTutorialReplayState replay; // 再生ごとの進捗
    PastSelfTutorialProgressState progress; // 挑戦中に残す攻略実績
    PastSelfTutorialRouteHistory history; // 通算クリア履歴

    /// <summary>
    /// 現在の状態と保存済み分身数から表示用の達成状況を集計する。
    /// </summary>
    PastSelfTutorialStatus CalculateStatus(size_t storedCloneCount) const
    {
        PastSelfTutorialStatus status; // 呼び出し時点の達成状況
        status.completedCheckCount = (progress.doorBlockedBeforeClone ? 1 : 0) +
            (progress.doorOpenedByClone ? 1 : 0) + (progress.clonePlatformUsed ? 1 : 0) +
            (progress.timedDoorOpened ? 1 : 0) + (progress.weightSwitchActivated ? 1 : 0) +
            (progress.oneWayGateUsed ? 1 : 0) + (gimmicks.goalReached ? 1 : 0);
        status.completedFlowCount = (progress.resetShown ? 1 : 0) +
            (progress.recordStarted ? 1 : 0) + (progress.recordStopped ? 1 : 0) +
            (progress.prepareUsed ? 1 : 0) + (progress.replayStarted ? 1 : 0);
        status.allChecksComplete = status.completedCheckCount == 7;
        status.videoFlowComplete = status.completedFlowCount == 5;
        status.enoughStoredClones = storedCloneCount >= 2;
        status.multiCloneRouteComplete = status.allChecksComplete && status.videoFlowComplete &&
            status.enoughStoredClones && progress.dualCloneSwitchesActivated;
        status.allRoutesComplete = history.oneCloneRouteCleared && history.twoCloneRouteCleared &&
            history.finalChallengeCleared;
        status.oneCloneRecordingStored = progress.recordTakeCount == 1 && storedCloneCount == 1 &&
            progress.recordStopped;
        status.twoCloneRecordingsStored = progress.recordTakeCount == 2 && storedCloneCount == 2 &&
            progress.recordStopped;
        status.oneCloneTutorialCheckCount = (status.oneCloneRecordingStored ? 1 : 0) +
            (replay.oneCloneToggleActivated ? 1 : 0) + (replay.oneCloneElevatorRidden ? 1 : 0) +
            (replay.oneCloneBasicsComplete ? 1 : 0);
        status.twoCloneTutorialCheckCount = (status.twoCloneRecordingsStored ? 1 : 0) +
            (replay.twoCloneReplayPrepared ? 1 : 0) + (replay.twoCloneSwitchesActivated ? 1 : 0) +
            (replay.twoCloneCooperationComplete ? 1 : 0);
        return status;
    }

    /// <summary>
    /// 通算クリア履歴を残し、挑戦を最初からやり直す。
    /// </summary>
    void ResetPuzzle()
    {
        gimmicks = {};
        replay = {};
        progress = {};
        progress.resetShown = true;
    }

    /// <summary>
    /// 実績と計測値を残して再生準備し、解放済みの扉と橋を復元する。
    /// </summary>
    void PrepareReplay(bool registerPrepareAction, size_t cloneCount, float feedbackDuration)
    {
        gimmicks = {};
        replay = {};
        gimmicks.doorOpen = progress.doorOpenedByClone;
        gimmicks.doorUnlockedByClone = progress.doorOpenedByClone;
        gimmicks.goalBridgeDeployed = progress.goalBridgeUnlocked;
        replay.twoCloneReplayPrepared = registerPrepareAction && progress.recordTakeCount == 2 && cloneCount == 2;
        replay.prepareFeedbackSeconds = registerPrepareAction ? feedbackDuration : 0.0f;
        progress.prepareUsed = progress.prepareUsed || registerPrepareAction;
    }
};
