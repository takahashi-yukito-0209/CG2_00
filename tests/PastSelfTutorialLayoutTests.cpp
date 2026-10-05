#include "application/scenes/PastSelfTutorialLayoutUtility.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
/// <summary>
/// テスト条件が成立しない場合に失敗理由を通知する。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 注視点が指定した配置と一致することを確認する。
/// </summary>
void CheckPosition(const Math::Vector3& actual, const Math::Vector3& expected)
{
    Check(actual.x == expected.x && actual.y == expected.y && actual.z == expected.z, "Camera target mismatch");
}

/// <summary>
/// ゴールのゼロ縮小後の復元と、負のスケール・繰り返し編集時の判定サイズを確認する。
/// </summary>
void TestGoalBounds()
{
    using PastSelfTutorialLayoutUtility::CalculateGoalHalfSize;
    const Math::Vector3 initialMarkerScale { 2, 4, 8 }; // 変更されない初期表示スケール
    const Math::Vector3 initialHalfSize { 1, 3, 2 }; // 変更されない初期判定半サイズ
    for (int iteration = 0; iteration < 100; ++iteration) { // ゼロ縮小と復元の繰り返し回数
        CheckPosition(CalculateGoalHalfSize({ 0, 0, 0 }, initialMarkerScale, initialHalfSize), { 0, 0, 0 });
        CheckPosition(CalculateGoalHalfSize(initialMarkerScale, initialMarkerScale, initialHalfSize), initialHalfSize);
        CheckPosition(CalculateGoalHalfSize({ -4, 2, -16 }, initialMarkerScale, initialHalfSize), { 2, 1.5f, 4 });
        CheckPosition(CalculateGoalHalfSize({ 0, 8, 4 }, initialMarkerScale, initialHalfSize), { 0, 6, 1 });
    }
}

/// <summary>
/// 昇降範囲の変更と、横方向も移動する足場の到達判定を確認する。
/// </summary>
void TestElevatorEndpoint()
{
    using PastSelfTutorialLayoutUtility::IsAtUpperEndpoint;
    const Math::Vector3 originalUpper { -13.2f, 2.46f, 0.0f }; // 従来の初期上端
    const Math::Vector3 editedUpper { -20.0f, 6.0f, 2.0f }; // 編集後の上端
    Check(IsAtUpperEndpoint(originalUpper, originalUpper, 0.05f), "Default endpoint failed");
    Check(IsAtUpperEndpoint(editedUpper, editedUpper, 0.05f), "Edited endpoint failed");
    Check(!IsAtUpperEndpoint(originalUpper, editedUpper, 0.05f), "Old endpoint was accepted after editing");
    Check(!IsAtUpperEndpoint({ -21.0f, 6.0f, 2.0f }, editedUpper, 0.05f), "X mismatch was ignored");
    Check(!IsAtUpperEndpoint({ -20.0f, 6.0f, 3.0f }, editedUpper, 0.05f), "Z mismatch was ignored");
    Check(IsAtUpperEndpoint({ -20.0f, 6.0625f, 2.0f }, editedUpper, 0.0625f), "Tolerance boundary failed");
    Check(!IsAtUpperEndpoint({ -20.0f, 6.125f, 2.0f }, editedUpper, 0.0625f), "Outside tolerance was accepted");
}

/// <summary>
/// 扉の移動・拡縮後も、プレイヤー全体の通過で完了することを確認する。
/// </summary>
void TestDoorPassage()
{
    using PastSelfTutorialLayoutUtility::HasPassedDoor;
    const Math::Vector3 playerHalfSize { 0.5f, 0.5f, 0.5f }; // プレイヤーの半サイズ
    const Math::Vector3 originalCenter { 7.0f, 3.52f, 0.0f }; // 従来の青扉位置
    const Math::Vector3 originalHalfSize { 0.15f, 1.1f, 1.5f }; // 従来の青扉判定半サイズ
    const Math::Vector3 editedCenter { 12.0f, 5.0f, 1.0f }; // 編集後の青扉位置
    const Math::Vector3 editedHalfSize { 2.0f, 1.1f, 1.5f }; // 横に拡大した判定半サイズ
    Check(HasPassedDoor({ 8.0f, 0.0f, 0.0f }, playerHalfSize, originalCenter, originalHalfSize), "Default door passage failed");
    Check(!HasPassedDoor({ 8.0f, 0.0f, 0.0f }, playerHalfSize, editedCenter, editedHalfSize), "Original passage line survived editing");
    Check(!HasPassedDoor({ 14.25f, 0.0f, 0.0f }, playerHalfSize, editedCenter, editedHalfSize), "Partial passage was accepted");
    Check(!HasPassedDoor({ 14.5f, 0.0f, 0.0f }, playerHalfSize, editedCenter, editedHalfSize), "Door edge equality was accepted");
    Check(HasPassedDoor({ 14.75f, 0.0f, 0.0f }, playerHalfSize, editedCenter, editedHalfSize), "Edited door passage failed");
}

/// <summary>
/// 初期配置の注視順、編集後の各座標、候補なしと通過境界を確認する。
/// </summary>
void TestCameraTargets()
{
    using PastSelfTutorialLayoutUtility::SelectCameraTarget;
    const std::array<Math::Vector3, 4> originalTargets { { // 従来の攻略順に並べた初期配置
        { 3.25f, 2.75f, 0.0f }, { 7.0f, 3.52f, 0.0f },
        { 11.4f, 2.76f, 0.0f }, { 13.2f, 3.55f, 0.0f },
    } };
    const Math::Vector3 originalGoal { 15.55f, 3.7f, 0.0f }; // 従来のゴール中心
    for (float playerX = -20.0f; playerX <= 20.0f; playerX += 0.25f) {
        const Math::Vector3 expected = playerX < 3.25f ? originalTargets[0]
            : playerX < 7.0f ? originalTargets[1]
            : playerX < 11.4f ? originalTargets[2]
            : playerX < 13.2f ? originalTargets[3] : originalGoal; // 整理前の選択条件
        CheckPosition(SelectCameraTarget(playerX, originalTargets, originalGoal), expected);
    }
    const std::array<Math::Vector3, 4> editedTargets { { // 編集後の攻略対象の配置
        { -10.0f, 6.0f, 1.0f }, { 2.0f, 8.0f, 2.0f },
        { 20.0f, 10.0f, 3.0f }, { 30.0f, 12.0f, 4.0f },
    } };
    const Math::Vector3 editedGoal { 40.0f, 14.0f, 5.0f }; // 移動したゴール中心
    CheckPosition(SelectCameraTarget(-11.0f, editedTargets, editedGoal), editedTargets[0]);
    for (size_t index = 0; index < editedTargets.size(); ++index) {
        CheckPosition(SelectCameraTarget(editedTargets[index].x, editedTargets, editedGoal),
            index + 1 < editedTargets.size() ? editedTargets[index + 1] : editedGoal);
    }
    CheckPosition(SelectCameraTarget(50.0f, editedTargets, editedGoal), editedGoal);
    CheckPosition(SelectCameraTarget(0.0f, {}, editedGoal), editedGoal);
    const std::array<Math::Vector3, 2> reorderedTargets { { { 20, 0, 0 }, { 10, 0, 0 } } }; // 配置が前後しても維持する攻略順
    CheckPosition(SelectCameraTarget(0.0f, reorderedTargets, editedGoal), reorderedTargets[0]);
}
}

/// <summary>
/// 配置変更後の判定とカメラ選択をDirectX初期化なしで検証する。
/// </summary>
int main()
{
    try {
        TestGoalBounds();
        TestElevatorEndpoint();
        TestDoorPassage();
        TestCameraTargets();
        std::cout << "Tutorial layout tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
