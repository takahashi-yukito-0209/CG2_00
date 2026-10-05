#include "application/scenes/PastSelfTutorialState.h"
#include "application/scenes/PlaySceneEditorState.h"
#include <iostream>
#include <stdexcept>

namespace {
/// <summary>
/// テスト条件を確認し、失敗理由を通知する。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// リセット前に多様な状態を作るための決定的な疑似乱数を生成する。
/// </summary>
uint32_t NextRandom(uint32_t& seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

/// <summary>
/// 全保存項目に値を入れ、初期化漏れと保持漏れを検出できる状態を作る。
/// </summary>
PastSelfTutorialState MakeState(uint32_t seed)
{
    PastSelfTutorialState state; // 比較に使用する初期状態
    state.gimmicks.switchActive = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.doorOpen = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.goalReached = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.doorUnlockedByClone = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.playerOnSwitch = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.cloneOnSwitch = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.timedSwitchActive = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.timedSwitchCloneOn = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.timedDoorOpen = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.toggleSwitchActive = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.toggleSwitchCloneOn = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.toggleGateOpen = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.toggleElevatorActive = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.weightSwitchActive = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.weightPlayerOn = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.weightCloneOn = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.goalBridgeDeployed = (NextRandom(seed) & 1u) != 0;
    state.gimmicks.oneWayGateBlocking = (NextRandom(seed) & 1u) != 0;
    state.replay.oneCloneToggleActivated = (NextRandom(seed) & 1u) != 0;
    state.replay.oneCloneElevatorRidden = (NextRandom(seed) & 1u) != 0;
    state.replay.oneCloneBasicsComplete = (NextRandom(seed) & 1u) != 0;
    state.replay.twoCloneReplayPrepared = (NextRandom(seed) & 1u) != 0;
    state.replay.twoCloneSwitchesActivated = (NextRandom(seed) & 1u) != 0;
    state.replay.twoCloneCooperationComplete = (NextRandom(seed) & 1u) != 0;
    state.replay.routeClearFinalized = (NextRandom(seed) & 1u) != 0;
    state.replay.recordingPendingCommit = (NextRandom(seed) & 1u) != 0;
    state.replay.clearTime = static_cast<float>(NextRandom(seed) % 1000u) / 10.0f;
    state.replay.prepareFeedbackSeconds = static_cast<float>(NextRandom(seed) % 1000u) / 10.0f;
    state.progress.doorBlockedBeforeClone = (NextRandom(seed) & 1u) != 0;
    state.progress.clonePlatformUsed = (NextRandom(seed) & 1u) != 0;
    state.progress.doorOpenedByClone = (NextRandom(seed) & 1u) != 0;
    state.progress.goalBridgeUnlocked = (NextRandom(seed) & 1u) != 0;
    state.progress.timedDoorOpened = (NextRandom(seed) & 1u) != 0;
    state.progress.dualCloneSwitchesActivated = (NextRandom(seed) & 1u) != 0;
    state.progress.weightSwitchActivated = (NextRandom(seed) & 1u) != 0;
    state.progress.oneWayGateUsed = (NextRandom(seed) & 1u) != 0;
    state.progress.resetShown = (NextRandom(seed) & 1u) != 0;
    state.progress.recordStarted = (NextRandom(seed) & 1u) != 0;
    state.progress.recordStopped = (NextRandom(seed) & 1u) != 0;
    state.progress.prepareUsed = (NextRandom(seed) & 1u) != 0;
    state.progress.replayStarted = (NextRandom(seed) & 1u) != 0;
    state.progress.elapsedTime = static_cast<float>(NextRandom(seed) % 1000u) / 10.0f;
    state.progress.lastRecordDuration = static_cast<float>(NextRandom(seed) % 1000u) / 10.0f;
    state.progress.recordTakeCount = NextRandom(seed) % 4u;
    state.progress.recentCheckText = "achievement notification";
    state.progress.recentCheckSeconds = static_cast<float>(NextRandom(seed) % 1000u) / 10.0f;
    state.history.oneCloneRouteCleared = (NextRandom(seed) & 1u) != 0;
    state.history.twoCloneRouteCleared = (NextRandom(seed) & 1u) != 0;
    state.history.finalChallengeCleared = (NextRandom(seed) & 1u) != 0;
    return state;
}

/// <summary>
/// 移動した全49項目を比較する。
/// </summary>
bool IsSameState(const PastSelfTutorialState& left, const PastSelfTutorialState& right)
{
    return left.gimmicks.switchActive == right.gimmicks.switchActive
        && left.gimmicks.doorOpen == right.gimmicks.doorOpen
        && left.gimmicks.goalReached == right.gimmicks.goalReached
        && left.gimmicks.doorUnlockedByClone == right.gimmicks.doorUnlockedByClone
        && left.gimmicks.playerOnSwitch == right.gimmicks.playerOnSwitch
        && left.gimmicks.cloneOnSwitch == right.gimmicks.cloneOnSwitch
        && left.gimmicks.timedSwitchActive == right.gimmicks.timedSwitchActive
        && left.gimmicks.timedSwitchCloneOn == right.gimmicks.timedSwitchCloneOn
        && left.gimmicks.timedDoorOpen == right.gimmicks.timedDoorOpen
        && left.gimmicks.toggleSwitchActive == right.gimmicks.toggleSwitchActive
        && left.gimmicks.toggleSwitchCloneOn == right.gimmicks.toggleSwitchCloneOn
        && left.gimmicks.toggleGateOpen == right.gimmicks.toggleGateOpen
        && left.gimmicks.toggleElevatorActive == right.gimmicks.toggleElevatorActive
        && left.gimmicks.weightSwitchActive == right.gimmicks.weightSwitchActive
        && left.gimmicks.weightPlayerOn == right.gimmicks.weightPlayerOn
        && left.gimmicks.weightCloneOn == right.gimmicks.weightCloneOn
        && left.gimmicks.goalBridgeDeployed == right.gimmicks.goalBridgeDeployed
        && left.gimmicks.oneWayGateBlocking == right.gimmicks.oneWayGateBlocking
        && left.replay.oneCloneToggleActivated == right.replay.oneCloneToggleActivated
        && left.replay.oneCloneElevatorRidden == right.replay.oneCloneElevatorRidden
        && left.replay.oneCloneBasicsComplete == right.replay.oneCloneBasicsComplete
        && left.replay.twoCloneReplayPrepared == right.replay.twoCloneReplayPrepared
        && left.replay.twoCloneSwitchesActivated == right.replay.twoCloneSwitchesActivated
        && left.replay.twoCloneCooperationComplete == right.replay.twoCloneCooperationComplete
        && left.replay.routeClearFinalized == right.replay.routeClearFinalized
        && left.replay.recordingPendingCommit == right.replay.recordingPendingCommit
        && left.replay.clearTime == right.replay.clearTime
        && left.replay.prepareFeedbackSeconds == right.replay.prepareFeedbackSeconds
        && left.progress.doorBlockedBeforeClone == right.progress.doorBlockedBeforeClone
        && left.progress.clonePlatformUsed == right.progress.clonePlatformUsed
        && left.progress.doorOpenedByClone == right.progress.doorOpenedByClone
        && left.progress.goalBridgeUnlocked == right.progress.goalBridgeUnlocked
        && left.progress.timedDoorOpened == right.progress.timedDoorOpened
        && left.progress.dualCloneSwitchesActivated == right.progress.dualCloneSwitchesActivated
        && left.progress.weightSwitchActivated == right.progress.weightSwitchActivated
        && left.progress.oneWayGateUsed == right.progress.oneWayGateUsed
        && left.progress.resetShown == right.progress.resetShown
        && left.progress.recordStarted == right.progress.recordStarted
        && left.progress.recordStopped == right.progress.recordStopped
        && left.progress.prepareUsed == right.progress.prepareUsed
        && left.progress.replayStarted == right.progress.replayStarted
        && left.progress.elapsedTime == right.progress.elapsedTime
        && left.progress.lastRecordDuration == right.progress.lastRecordDuration
        && left.progress.recordTakeCount == right.progress.recordTakeCount
        && left.progress.recentCheckText == right.progress.recentCheckText
        && left.progress.recentCheckSeconds == right.progress.recentCheckSeconds
        && left.history.oneCloneRouteCleared == right.history.oneCloneRouteCleared
        && left.history.twoCloneRouteCleared == right.history.twoCloneRouteCleared
        && left.history.finalChallengeCleared == right.history.finalChallengeCleared;
}

// 旧実装の代入順序と保持条件を固定した比較用コード。製品側の共通化には追従させない。
/// <summary>
/// 整理前の通常リセットを状態データだけで再現する。
/// </summary>
void LegacyResetPuzzle(PastSelfTutorialState& state)
{
    state.gimmicks.goalReached = false;
    state.gimmicks.switchActive = false;
    state.gimmicks.doorOpen = false;
    state.gimmicks.doorUnlockedByClone = false;
    state.gimmicks.playerOnSwitch = false;
    state.gimmicks.cloneOnSwitch = false;
    state.progress.doorBlockedBeforeClone = false;
    state.progress.clonePlatformUsed = false;
    state.progress.doorOpenedByClone = false;
    state.gimmicks.timedSwitchActive = false;
    state.gimmicks.timedSwitchCloneOn = false;
    state.gimmicks.timedDoorOpen = false;
    state.gimmicks.toggleSwitchActive = false;
    state.gimmicks.toggleSwitchCloneOn = false;
    state.gimmicks.toggleGateOpen = false;
    state.gimmicks.toggleElevatorActive = false;
    state.replay.oneCloneToggleActivated = false;
    state.replay.oneCloneElevatorRidden = false;
    state.replay.oneCloneBasicsComplete = false;
    state.replay.twoCloneReplayPrepared = false;
    state.replay.twoCloneSwitchesActivated = false;
    state.replay.twoCloneCooperationComplete = false;
    state.replay.routeClearFinalized = false;
    state.gimmicks.weightSwitchActive = false;
    state.gimmicks.weightPlayerOn = false;
    state.gimmicks.weightCloneOn = false;
    state.progress.goalBridgeUnlocked = false;
    state.gimmicks.goalBridgeDeployed = false;
    state.gimmicks.oneWayGateBlocking = false;
    state.progress.timedDoorOpened = false;
    state.progress.dualCloneSwitchesActivated = false;
    state.progress.weightSwitchActivated = false;
    state.progress.oneWayGateUsed = false;
    state.progress.resetShown = true;
    state.progress.recordStarted = false;
    state.progress.recordStopped = false;
    state.progress.prepareUsed = false;
    state.progress.replayStarted = false;
    state.replay.recordingPendingCommit = false;
    state.progress.elapsedTime = 0.0f;
    state.replay.clearTime = 0.0f;
    state.progress.lastRecordDuration = 0.0f;
    state.replay.prepareFeedbackSeconds = 0.0f;
    state.progress.recentCheckText.clear();
    state.progress.recentCheckSeconds = 0.0f;
    state.progress.recordTakeCount = 0;
}

/// <summary>
/// 整理前の再生準備を状態データだけで再現する。
/// </summary>
void LegacyPrepareReplay(PastSelfTutorialState& state, bool registerPrepareAction, size_t cloneCount, float feedbackDuration)
{
    const bool keepDoorBlocked = state.progress.doorBlockedBeforeClone; // 記録前に閉じた扉へ阻まれた実証結果
    const bool keepClonePlatformUsed = state.progress.clonePlatformUsed; // 分身足場を利用した実証結果
    const bool keepDoorOpenedByClone = state.progress.doorOpenedByClone; // 分身で通常扉を開けた実証結果
    const bool keepTimedDoorOpened = state.progress.timedDoorOpened; // 時間差扉を開けた実証結果
    const bool keepDualCloneSwitchesActivated = state.progress.dualCloneSwitchesActivated; // 複数分身で離れたスイッチを同時起動した実証結果
    const bool keepWeightSwitchActivated = state.progress.weightSwitchActivated; // 重さスイッチを起動した実証結果
    const bool keepGoalBridgeUnlocked = state.progress.goalBridgeUnlocked; // 重さスイッチで解放した橋の攻略状態
    const bool keepOneWayGateUsed = state.progress.oneWayGateUsed; // 一方通行ゲートを利用した実証結果
    const bool keepResetShown = state.progress.resetShown; // リセット開始を示す実証結果
    const bool keepRecordStarted = state.progress.recordStarted; // 記録開始を示す実証結果
    const bool keepRecordStopped = state.progress.recordStopped; // 記録停止を示す実証結果
    const bool keepPrepareUsed = state.progress.prepareUsed; // Prepare操作を示す実証結果
    const bool keepReplayStarted = state.progress.replayStarted; // 再生開始を示す実証結果
    const bool keepDoorUnlocked = keepDoorOpenedByClone; // 再生済み分身で開放した通常扉状態
    state.gimmicks.goalReached = false;
    state.gimmicks.switchActive = false;
    state.gimmicks.doorOpen = keepDoorUnlocked;
    state.gimmicks.doorUnlockedByClone = keepDoorUnlocked;
    state.gimmicks.playerOnSwitch = false;
    state.gimmicks.cloneOnSwitch = false;
    state.progress.doorBlockedBeforeClone = keepDoorBlocked;
    state.progress.clonePlatformUsed = keepClonePlatformUsed;
    state.progress.doorOpenedByClone = keepDoorOpenedByClone;
    state.gimmicks.timedSwitchActive = false;
    state.gimmicks.timedSwitchCloneOn = false;
    state.gimmicks.timedDoorOpen = false;
    state.gimmicks.toggleSwitchActive = false;
    state.gimmicks.toggleSwitchCloneOn = false;
    state.gimmicks.toggleGateOpen = false;
    state.gimmicks.toggleElevatorActive = false;
    state.replay.oneCloneToggleActivated = false;
    state.replay.oneCloneElevatorRidden = false;
    state.replay.oneCloneBasicsComplete = false;
    state.replay.twoCloneReplayPrepared = registerPrepareAction &&
        state.progress.recordTakeCount == 2 && cloneCount == 2;
    state.replay.twoCloneSwitchesActivated = false;
    state.replay.twoCloneCooperationComplete = false;
    state.replay.routeClearFinalized = false;
    state.gimmicks.weightSwitchActive = false;
    state.gimmicks.weightPlayerOn = false;
    state.gimmicks.weightCloneOn = false;
    state.progress.goalBridgeUnlocked = keepGoalBridgeUnlocked;
    state.gimmicks.goalBridgeDeployed = keepGoalBridgeUnlocked;
    state.gimmicks.oneWayGateBlocking = false;
    state.progress.timedDoorOpened = keepTimedDoorOpened;
    state.progress.dualCloneSwitchesActivated = keepDualCloneSwitchesActivated;
    state.progress.weightSwitchActivated = keepWeightSwitchActivated;
    state.progress.oneWayGateUsed = keepOneWayGateUsed;
    state.progress.resetShown = keepResetShown;
    state.progress.recordStarted = keepRecordStarted;
    state.progress.recordStopped = keepRecordStopped;
    state.progress.prepareUsed = keepPrepareUsed || registerPrepareAction;
    state.progress.replayStarted = keepReplayStarted;
    state.replay.recordingPendingCommit = false;
    state.replay.prepareFeedbackSeconds = registerPrepareAction ? feedbackDuration : 0.0f;
    state.replay.clearTime = 0.0f;
}

/// <summary>
/// 整理前のHUD条件と集計結果を比較し、元の状態が変化しないことを確認する。
/// </summary>
void CheckStatus(const PastSelfTutorialState& state, size_t cloneCount)
{
    const PastSelfTutorialState before = state; // 集計前の状態
    const PastSelfTutorialStatus actual = state.CalculateStatus(cloneCount); // 共通集計の結果
    const bool checks[] = { state.progress.doorBlockedBeforeClone, state.progress.doorOpenedByClone,
        state.progress.clonePlatformUsed, state.progress.timedDoorOpened, state.progress.weightSwitchActivated,
        state.progress.oneWayGateUsed, state.gimmicks.goalReached }; // 旧HUDの検証項目
    const bool flow[] = { state.progress.resetShown, state.progress.recordStarted, state.progress.recordStopped,
        state.progress.prepareUsed, state.progress.replayStarted }; // 旧HUDの動画操作項目
    int checkCount = 0; // 達成済み検証項目数
    int flowCount = 0; // 達成済み操作項目数
    for (bool completed : checks) { // 各検証項目の達成状態
        checkCount += completed ? 1 : 0;
    }
    for (bool completed : flow) { // 各操作項目の達成状態
        flowCount += completed ? 1 : 0;
    }
    const bool oneStored = state.progress.recordTakeCount == 1 && cloneCount == 1 &&
        state.progress.recordStopped; // 旧HUDの1体用記録条件
    const bool twoStored = state.progress.recordTakeCount == 2 && cloneCount == 2 &&
        state.progress.recordStopped; // 旧HUDの2体用記録条件
    const int oneCount = (oneStored ? 1 : 0) + (state.replay.oneCloneToggleActivated ? 1 : 0) +
        (state.replay.oneCloneElevatorRidden ? 1 : 0) + (state.replay.oneCloneBasicsComplete ? 1 : 0); // 旧HUDの1体達成数
    const int twoCount = (twoStored ? 1 : 0) + (state.replay.twoCloneReplayPrepared ? 1 : 0) +
        (state.replay.twoCloneSwitchesActivated ? 1 : 0) + (state.replay.twoCloneCooperationComplete ? 1 : 0); // 旧HUDの2体達成数
    const bool multiComplete = checkCount == 7 && flowCount == 5 && state.gimmicks.goalReached &&
        cloneCount >= 2 && state.progress.dualCloneSwitchesActivated; // 旧詳細HUDの最終課題検証条件
    Check(actual.completedCheckCount == checkCount && actual.completedFlowCount == flowCount &&
        actual.allChecksComplete == (checkCount == 7) && actual.videoFlowComplete == (flowCount == 5) &&
        actual.enoughStoredClones == (cloneCount >= 2) && actual.multiCloneRouteComplete == multiComplete,
        "Final challenge status differs from legacy HUD");
    Check(actual.oneCloneRecordingStored == oneStored && actual.twoCloneRecordingsStored == twoStored &&
        actual.oneCloneTutorialCheckCount == oneCount && actual.twoCloneTutorialCheckCount == twoCount,
        "Tutorial recording conditions or counts differ from legacy HUD");
    Check(actual.allRoutesComplete == (state.history.oneCloneRouteCleared &&
        state.history.twoCloneRouteCleared && state.history.finalChallengeCleared),
        "Route history summary differs from legacy HUD");
    Check(IsSameState(before, state), "Status calculation changed tutorial state");
}

/// <summary>
/// 検証・操作項目の全組み合わせと、記録数・分身数・再生進捗の境界を確認する。
/// </summary>
void TestStatus()
{
    for (uint32_t mask = 0; mask < (1u << 13); ++mask) { // 検証・操作項目の組み合わせ
        PastSelfTutorialState state; // 検証項目の組み合わせ
        bool* flags[] = { &state.progress.doorBlockedBeforeClone, &state.progress.doorOpenedByClone,
            &state.progress.clonePlatformUsed, &state.progress.timedDoorOpened, &state.progress.weightSwitchActivated,
            &state.progress.oneWayGateUsed, &state.gimmicks.goalReached, &state.progress.resetShown,
            &state.progress.recordStarted, &state.progress.recordStopped, &state.progress.prepareUsed,
            &state.progress.replayStarted, &state.progress.dualCloneSwitchesActivated }; // 検証条件と操作条件の13項目
        for (size_t index = 0; index < 13; ++index) { // 設定する項目番号
            *flags[index] = (mask & (1u << index)) != 0;
        }
        for (uint32_t takes = 0; takes <= 3; ++takes) { // 記録回数の境界
            state.progress.recordTakeCount = takes;
            for (size_t clones = 0; clones <= 3; ++clones) { // 保存済み分身数の境界
                CheckStatus(state, clones);
            }
        }
    }
    for (uint32_t mask = 0; mask < (1u << 10); ++mask) { // 再生進捗と通算履歴の組み合わせ
        PastSelfTutorialState state; // 再生進捗と通算履歴の組み合わせ
        bool* flags[] = { &state.replay.oneCloneToggleActivated, &state.replay.oneCloneElevatorRidden,
            &state.replay.oneCloneBasicsComplete, &state.replay.twoCloneReplayPrepared,
            &state.replay.twoCloneSwitchesActivated, &state.replay.twoCloneCooperationComplete,
            &state.history.oneCloneRouteCleared, &state.history.twoCloneRouteCleared,
            &state.history.finalChallengeCleared, &state.progress.recordStopped }; // 各ルートの集計条件
        for (size_t index = 0; index < 10; ++index) { // 設定する項目番号
            *flags[index] = (mask & (1u << index)) != 0;
        }
        for (uint32_t takes = 0; takes <= 3; ++takes) { // 記録回数の境界
            state.progress.recordTakeCount = takes;
            for (size_t clones = 0; clones <= 3; ++clones) { // 保存済み分身数の境界
                CheckStatus(state, clones);
            }
        }
    }
}
}

/// <summary>
/// リセットとHUD集計の互換性、通算履歴の保持、編集選択の独立性を検証する。
/// </summary>
int main()
{
    try {
        TestStatus();
        for (uint32_t seed = 1; seed <= 256; ++seed) {
            const PastSelfTutorialState initial = MakeState(seed); // リセット前の全状態
            PastSelfTutorialState expected = initial; // 旧実装の結果
            PastSelfTutorialState actual = initial; // 整理後の結果
            LegacyResetPuzzle(expected);
            actual.ResetPuzzle();
            Check(IsSameState(expected, actual), "Puzzle reset differs from legacy behavior");
            for (size_t cloneCount = 0; cloneCount <= 3; ++cloneCount) {
                for (bool registerPrepareAction : { false, true }) {
                    expected = initial;
                    actual = initial;
                    LegacyPrepareReplay(expected, registerPrepareAction, cloneCount, 1.5f);
                    actual.PrepareReplay(registerPrepareAction, cloneCount, 1.5f);
                    Check(IsSameState(expected, actual), "Replay reset differs from legacy behavior");
                }
            }
        }
        PastSelfTutorialState state = MakeState(12); // 明示的な境界条件の確認対象
        state.progress.recordTakeCount = 2;
        state.progress.doorOpenedByClone = true;
        state.progress.goalBridgeUnlocked = true;
        state.PrepareReplay(true, 2, 1.5f);
        Check(state.replay.twoCloneReplayPrepared && state.gimmicks.doorOpen
            && state.gimmicks.doorUnlockedByClone && state.gimmicks.goalBridgeDeployed,
            "Unlocked state or two-clone preparation was lost");
        state.PrepareReplay(false, 2, 1.5f);
        Check(!state.replay.twoCloneReplayPrepared && state.replay.prepareFeedbackSeconds == 0.0f
            && state.progress.prepareUsed, "Silent preparation changed accumulated progress");
        state.history = { true, true, true };
        state.ResetPuzzle();
        Check(state.history.oneCloneRouteCleared && state.history.twoCloneRouteCleared
            && state.history.finalChallengeCleared && state.progress.resetShown,
            "Puzzle reset lost route history");
        state = {};
        Check(IsSameState(state, PastSelfTutorialState{}), "Scene teardown reset failed");

        PlaySceneObjectEditorState first; // 最初のシーンの編集選択
        PlaySceneObjectEditorState second; // 別シーンの編集選択
        first = { 4, 3, 2, 1 };
        Check(second.selectedCreateTextureIndex == 0 && second.selectedDeleteSpriteIndex == 0
            && second.selectedCreateModelIndex == 0 && second.selectedDeleteObjectIndex == 0,
            "Object editor selection leaked between scenes");
        first = {};
        Check(first.selectedCreateTextureIndex == 0 && first.selectedDeleteSpriteIndex == 0
            && first.selectedCreateModelIndex == 0 && first.selectedDeleteObjectIndex == 0,
            "Object editor selection reset failed");
        std::cout << "Tutorial state tests passed: 256 puzzle resets, 2048 replay resets, 147456 HUD summaries\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
