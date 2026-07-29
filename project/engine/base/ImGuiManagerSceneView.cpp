#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "engine/base/SrvManager.h"
#include "engine/utility/mathUtility.h"
#include <algorithm>
#include <d3d12.h>
#endif

namespace MyEngine {

/// <summary>
/// シーン表示用テクスチャをImGuiウィンドウ内に描画する
/// </summary>
void ImGuiManager::DrawSceneViewWindow(Context& ctx)
{
#ifdef USE_IMGUI
    sceneViewHovered_ = false; // Scene Viewが無効なフレームで前回のhover状態を残さない
    ImGui::Begin("Scene View");

    const bool hasSceneTexture = ctx.srvManager && ctx.sceneViewSrvIndex != UINT32_MAX; // Scene Viewへ表示できるSRVがあるか
    if (!hasSceneTexture) {
        ImGui::TextDisabled("No scene texture");
        ImGui::End();
        return;
    }

    ImVec2 availableSize = ImGui::GetContentRegionAvail(); // Scene View内で画像表示に使える領域
    if (availableSize.x <= 1.0f || availableSize.y <= 1.0f) {
        ImGui::End();
        return;
    }

    const float sourceWidth = ctx.sceneViewWidth > 0.0f ? ctx.sceneViewWidth : availableSize.x; // 元テクスチャの横幅
    const float sourceHeight = ctx.sceneViewHeight > 0.0f ? ctx.sceneViewHeight : availableSize.y; // 元テクスチャの縦幅
    const float sourceAspect = sourceWidth / sourceHeight; // 元テクスチャのアスペクト比
    const float availableAspect = availableSize.x / availableSize.y; // 表示領域のアスペクト比

    ImVec2 imageSize = availableSize; // 実際に表示する画像サイズ
    if (availableAspect > sourceAspect) {
        imageSize.x = imageSize.y * sourceAspect;
    } else {
        imageSize.y = imageSize.x / sourceAspect;
    }

    const float offsetX = (availableSize.x - imageSize.x) * 0.5f; // 中央寄せ用の横オフセット
    const float offsetY = (availableSize.y - imageSize.y) * 0.5f; // 中央寄せ用の縦オフセット
    if (offsetX > 0.0f || offsetY > 0.0f) {
        ImVec2 cursorPosition = ImGui::GetCursorPos(); // 現在の描画開始位置
        cursorPosition.x += (std::max)(offsetX, 0.0f);
        cursorPosition.y += (std::max)(offsetY, 0.0f);
        ImGui::SetCursorPos(cursorPosition);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE sceneSrvHandle = ctx.srvManager->GetGPUDescriptorHandle(ctx.sceneViewSrvIndex); // Scene View用SRVのGPUハンドル
    ImTextureRef sceneTexture(static_cast<ImTextureID>(sceneSrvHandle.ptr)); // ImGuiへ渡すテクスチャ参照
    ImGui::Image(sceneTexture, imageSize);
    const ImVec2 imageMin = ImGui::GetItemRectMin(); // Scene View画像の左上座標
    sceneViewHovered_ = ImGui::IsItemHovered(); // Scene View画像上ならカメラ操作を許可する
    DrawTranslationGizmo(ctx, imageMin, imageSize);
    if (ctx.drawSceneViewOverlay && ctx.sceneViewMatrix && ctx.sceneProjectionMatrix) {
        const Math::Matrix4x4 viewProjectionMatrix = MathUtil::Multiply(*ctx.sceneViewMatrix, *ctx.sceneProjectionMatrix); // Scene固有オーバーレイ用のビュー射影行列
        ctx.drawSceneViewOverlay(viewProjectionMatrix, imageMin.x, imageMin.y, imageSize.x, imageSize.y);
    }

    ImGui::End();
#else
    (void)ctx;
#endif
}

} // namespace MyEngine