#include "ImGuiManager.h"
#include <cstring>
#include "engine/utility/Logger.h"
#include <cstdint>
#include <d3d12.h>
#include <sstream>
#include <string>


#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/2d/TextureManager.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/PostProcess.h"
#include "engine/base/SrvManager.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace MyEngine;
using namespace Math;

namespace MyEngine {

#ifdef USE_IMGUI
namespace {
constexpr int kKernelIndex3x3 = 0; // 3x3を選択する番号
constexpr int kKernelIndex5x5 = 1; // 5x5を選択する番号
constexpr int kKernelIndex7x7 = 2; // 7x7を選択する番号
constexpr uint32_t kKernelSize3x3 = 3u; // 3x3カーネルサイズ
constexpr uint32_t kKernelSize5x5 = 5u; // 5x5カーネルサイズ
constexpr uint32_t kKernelSize7x7 = 7u; // 7x7カーネルサイズ
constexpr uint32_t kSeparablePassCount = 2u; // 分離フィルタのパス数
constexpr float kPostProcessNormalizedMin = 0.0f; // 正規化値の最小値
constexpr float kPostProcessNormalizedMax = 1.0f; // 正規化値の最大値
constexpr float kGaussianSigmaStep = 0.05f; // Gaussian sigmaの調整幅
constexpr float kGaussianSigmaMin = 0.1f; // Gaussian sigmaの最小値
constexpr float kGaussianSigmaMax = 10.0f; // Gaussian sigmaの最大値
constexpr float kOutlineStrengthStep = 0.1f; // アウトライン強度の調整幅
constexpr float kOutlineStrengthMin = 0.0f; // アウトライン強度の最小値
constexpr float kOutlineStrengthMax = 32.0f; // アウトライン強度の最大値
constexpr float kDepthThresholdStep = 0.001f; // 深度閾値の調整幅
constexpr float kDepthThresholdMax = 0.2f; // 深度閾値の最大値
constexpr float kDepthSoftnessStep = 0.001f; // 深度アウトライン柔らかさの調整幅
constexpr float kDepthSoftnessMin = 0.001f; // 深度アウトライン柔らかさの最小値
constexpr float kDepthSoftnessMax = 0.2f; // 深度アウトライン柔らかさの最大値
constexpr float kEffectCenterStep = 0.005f; // 中心座標の調整幅
constexpr float kRadialBlurWidthStep = 0.0005f; // ラジアルブラー幅の調整幅
constexpr float kRadialBlurWidthMax = 0.1f; // ラジアルブラー幅の最大値
constexpr int kRadialBlurSampleMin = 1; // ラジアルブラーサンプル数の最小値
constexpr int kRadialBlurSampleMax = 32; // ラジアルブラーサンプル数の最大値
constexpr float kDistortionStrengthStep = 0.001f; // 歪み強度の調整幅
constexpr float kDistortionStrengthMin = -0.1f; // 歪み強度の最小値
constexpr float kDistortionStrengthMax = 0.1f; // 歪み強度の最大値
constexpr float kDistortionRadiusStep = 0.01f; // 歪み半径の調整幅
constexpr float kDistortionRadiusMin = 0.01f; // 歪み半径の最小値
constexpr float kDistortionRadiusMax = 1.5f; // 歪み半径の最大値
constexpr float kDistortionWaveStep = 0.1f; // 歪み波数の調整幅
constexpr float kDistortionWaveMax = 12.0f; // 歪み波数の最大値
constexpr float kDissolveEdgeWidthStep = 0.001f; // Dissolve境界幅の調整幅
constexpr float kDissolveEdgeWidthMin = 0.001f; // Dissolve境界幅の最小値
constexpr float kDissolveEdgeWidthMax = 0.25f; // Dissolve境界幅の最大値
constexpr float kRandomScaleStep = 1.0f; // ノイズスケールの調整幅
constexpr float kRandomScaleMin = 1.0f; // ノイズスケールの最小値
constexpr float kRandomScaleMax = 2000.0f; // ノイズスケールの最大値
constexpr float kRandomSpeedStep = 0.05f; // ノイズ速度の調整幅
constexpr float kRandomSpeedMax = 20.0f; // ノイズ速度の最大値
} // namespace

/// <summary>
/// ImGui::NewFrame とバックエンドの NewFrame を呼び出して新しいフレームを開始する。
/// </summary>
void ImGuiManager::NewFrame()
{
    // バックエンドの NewFrame を呼び出す
    ImGui_ImplWin32_NewFrame(); // Win32プラットフォームの新しいフレームを開始
    ImGui_ImplDX12_NewFrame(); // DX12レンダラーの新しいフレームを開始
    ImGuiIO& io = ImGui::GetIO();
    bool rendererHandlesTextures = (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0;
    if (!rendererHandlesTextures) {
        if (io.Fonts && !io.Fonts->TexIsBuilt) {
            Logger::Log("WARN ImGuiManager::NewFrame: Fonts TexID is null; ensure SrvManager initialized ImGui renderer and created device objects.\n");
        }
    }
    ImGui::NewFrame(); // ImGuiの新しいフレームを開始
}

/// <summary>
/// ImGuiとバックエンドの初期化
/// </summary>
void ImGuiManager::Initialize(void* hwnd, SrvManager* srvManager)
{

    // なかったらImGuiコンテキストを作成する（通常はアプリケーションで1回だけ呼び出される想定なので、すでに存在している場合は再利用する）
    if (ImGui::GetCurrentContext() == nullptr) {
        ImGui::CreateContext();
    }

    ImGuiIO& imguiIo = ImGui::GetIO(); // ImGui全体の設定を扱うIO情報
    imguiIo.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // ドッキング機能を有効化する
    imguiIo.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // ImGuiウィンドウをメイン画面外へ出せるようにする

    // ImGuiのスタイルをカスタマイズする
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.22f, 0.28f, 0.95f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.33f, 0.38f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.38f, 0.46f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.42f, 0.50f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.42f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.48f, 0.56f, 1.00f);

    if (imguiIo.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f; // 外部ウィンドウと通常ウィンドウの見た目を揃える
        style.Colors[ImGuiCol_WindowBg].w = 1.0f; // 外部ウィンドウ背景を不透明にする
    }

    // バックエンドの初期化
    ImGui_ImplWin32_Init(hwnd);

    // SrvManagerが提供されている場合は、ImGuiの初期化もSrvManagerに任せる（SRVヒープの設定などを行うため）。提供されていない場合は、ImGuiの初期化は行わない。
    if (srvManager) {
        srvManager->InitImGui(); // SrvManagerにImGuiの初期化を任せる
    }

    ImGuiIO& io = ImGui::GetIO();
    bool rendererHandlesTextures = (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0;
    if (!rendererHandlesTextures) {
        if (io.Fonts) {
            unsigned char* pixels = nullptr;
            int width = 0, height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

            std::ostringstream oss;
            oss << "DEBUG Initialize: Font atlas after GetTexDataAsRGBA32: TexIsBuilt=" << (io.Fonts->TexIsBuilt ? 1 : 0);
            Logger::Log(oss.str());
        }
    } else {
        if (io.Fonts) {
            std::ostringstream oss;
            oss << "DEBUG Initialize: Backend handles textures, TexIsBuilt=" << (io.Fonts->TexIsBuilt ? 1 : 0);
            Logger::Log(oss.str());
        }
    }
}

/// <summary>
/// ImGuiとバックエンドのシャットダウン
/// </summary>
void ImGuiManager::Shutdown()
{
    // バックエンドのシャットダウン
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX12_Shutdown(); // DX12レンダラーのシャットダウン
        ImGui_ImplWin32_Shutdown(); // Win32プラットフォームのシャットダウン
        ImGui::DestroyContext(); // ImGuiコンテキストの破棄
    }
}

/// <summary>
/// ImGui コントロールの構築（Context 構造体を引数にして、必要な情報を渡す）
/// </summary>
void ImGuiManager::BuildUI(Context& ctx)
{
    DrawDockSpace();

    DrawSceneViewWindow(ctx);

    ImGui::Begin("Debug Settings");

    DrawSceneSection(ctx);

    DrawPostProcessSection(ctx);

    DrawParticleSection(ctx);

    DrawObjectSection(ctx);

    DrawSpriteSection(ctx);

    DrawCommonSection(ctx);

    ImGui::End();

    DrawCameraWindow(ctx);
}
/// <summary>
/// ImGui の描画コマンドを発行する。ImGui::Render とバックエンドの Render を呼び出す
/// </summary>
void ImGuiManager::Render(ID3D12GraphicsCommandList* commandList)
{
    // ImGui の描画コマンドを発行する。ImGui::Render とバックエンドの Render を呼び出す
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

    ImGuiIO& imguiIo = ImGui::GetIO(); // ImGui全体の設定を扱うIO情報
    if (imguiIo.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows(); // メイン画面外へ出したImGuiウィンドウを更新する
        ImGui::RenderPlatformWindowsDefault(); // 追加ウィンドウの描画を実行する
    }
}

bool ImGuiManager::IsCapturingInput()
{
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    // ImGuiが現在UIによってマウスやキーボードをキャプチャしているかを判定
    return ImGui::IsAnyItemActive() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || io.WantCaptureMouse || io.WantCaptureKeyboard;
#else
    return false;
#endif
}

/// <summary>
/// メインウィンドウ全体に ImGui のドッキング領域を作成する
/// </summary>
void ImGuiManager::DrawDockSpace()
{
#ifdef USE_IMGUI
    ImGuiViewport* mainViewport = ImGui::GetMainViewport(); // DockSpaceを配置するメインビューポート
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking; // DockSpace用ウィンドウの基本フラグ
    windowFlags |= ImGuiWindowFlags_NoTitleBar;
    windowFlags |= ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize;
    windowFlags |= ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    windowFlags |= ImGuiWindowFlags_NoNavFocus;
    windowFlags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("Main DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(2);

    ImGuiID dockSpaceId = ImGui::GetID("MainDockSpace"); // DockSpaceを識別するID
    ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode; // 中央部分はゲーム画面を見せる
    ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags);

    ImGui::End();
#endif
}
/// <summary>
/// シーン表示用テクスチャをImGuiウィンドウ内に描画する
/// </summary>
void ImGuiManager::DrawSceneViewWindow(Context& ctx)
{
#ifdef USE_IMGUI
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

    ImGui::End();
#else
    (void)ctx;
#endif
}
/// <summary>
/// シーン情報をImGuiで描画する
/// </summary>
void ImGuiManager::DrawSceneSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ctx.currentSceneName) {
            ImGui::Text("Current Scene: %s", ctx.currentSceneName);
        }

        if (ctx.requestSceneChange) {
            const char* sceneNames[] = {
                "Title",
                "Play",
            }; // ImGuiから切り替え可能なシーン名
            constexpr int sceneCount = static_cast<int>(sizeof(sceneNames) / sizeof(sceneNames[0])); // シーン数
            int selectedSceneIndex = 0; // 現在選択されているシーン番号
            for (int sceneIndex = 0; sceneIndex < sceneCount; ++sceneIndex) {
                if (ctx.currentSceneName && std::strcmp(ctx.currentSceneName, sceneNames[sceneIndex]) == 0) {
                    selectedSceneIndex = sceneIndex;
                    break;
                }
            }
            if (ImGui::Combo("Scene##SceneSelector", &selectedSceneIndex, sceneNames, sceneCount)) {
                ctx.requestSceneChange(sceneNames[selectedSceneIndex]);
            }
        }
        if (ctx.selectedDrawType) {
            const char* drawTypeLabels[] = {
                "Model",
                "Particle",
                "Sprite",
                "Bunny",
                "Fence",
                "Checker",
                "Sphere",
                "Simple Skin",
                "Human Sneak Walk",
                "Human Walk",
                "All"
            }; // 描画対象の表示名
            constexpr int drawTypeCount = static_cast<int>(sizeof(drawTypeLabels) / sizeof(drawTypeLabels[0])); // 描画対象数
            *ctx.selectedDrawType = (std::clamp)(*ctx.selectedDrawType, 0, drawTypeCount - 1);
            ImGui::Combo("Draw Target", ctx.selectedDrawType, drawTypeLabels, drawTypeCount);
        }

        const float frameRate = ImGui::GetIO().Framerate; // 現在のImGui計測FPS
        const float frameTimeMs = frameRate > 0.0f ? 1000.0f / frameRate : 0.0f; // 1フレームあたりの表示時間
        ImGui::Text("FPS: %.1f (%.3f ms)", frameRate, frameTimeMs);
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// ポストエフェクトの設定と状態をImGuiへ表示する
/// </summary>
void ImGuiManager::DrawPostProcessSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.postProcess) {
        return;
    }

    if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool enabled = ctx.postProcess->IsEnabled(); // 現在の有効状態
        if (ImGui::Checkbox("Enabled", &enabled)) {
            ctx.postProcess->SetEnabled(enabled);
        }

        int effectIndex = static_cast<int>(ctx.postProcess->GetEffectType()); // 現在のエフェクト番号
        const char* effectNames[] = {
            "Distortion",
            "Copy",
            "Grayscale",
            "Vignette",
            "Box Filter",
            "Gaussian Filter",
            "Luminance Outline",
            "Depth Outline",
            "Radial Blur",
            "Dissolve",
            "Random"
        }; // 選択可能なエフェクト名

        if (ImGui::Combo(
                "Effect",
                &effectIndex,
                effectNames,
                IM_ARRAYSIZE(effectNames))) {
            ctx.postProcess->SetEffectType(
                static_cast<PostEffectType>(effectIndex));
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::BoxFilter) {
            int kernelIndex = ctx.postProcess->GetBoxFilterKernelSize() == kKernelSize5x5
                ? kKernelIndex5x5
                : kKernelIndex3x3; // 現在のカーネル番号
            const char* kernelNames[] = {
                "3x3",
                "5x5"
            }; // 選択可能なカーネルサイズ

            if (ImGui::Combo(
                    "Kernel Size",
                    &kernelIndex,
                    kernelNames,
                    IM_ARRAYSIZE(kernelNames))) {
                uint32_t kernelSize = kernelIndex == kKernelIndex5x5
                    ? kKernelSize5x5
                    : kKernelSize3x3; // 選択されたカーネルサイズ
                ctx.postProcess->SetBoxFilterKernelSize(kernelSize);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::GaussianFilter) {
            uint32_t currentKernelSize = ctx.postProcess->GetGaussianKernelSize(); // 現在のカーネルサイズ
            int kernelIndex = currentKernelSize == kKernelSize3x3
                ? kKernelIndex3x3
                : (currentKernelSize == kKernelSize5x5 ? kKernelIndex5x5 : kKernelIndex7x7);
            const char* kernelNames[] = {
                "3x3",
                "5x5",
                "7x7"
            }; // 選択可能なカーネルサイズ

            if (ImGui::Combo(
                    "Gaussian Kernel",
                    &kernelIndex,
                    kernelNames,
                    IM_ARRAYSIZE(kernelNames))) {
                const uint32_t kernelSizes[] = {
                    kKernelSize3x3,
                    kKernelSize5x5,
                    kKernelSize7x7
                }; // 選択値に対応するサイズ
                ctx.postProcess->SetGaussianKernelSize(kernelSizes[kernelIndex]);
            }

            float sigma = ctx.postProcess->GetGaussianSigma(); // 現在の標準偏差
            if (ImGui::DragFloat(
                    "Sigma",
                    &sigma,
                    kGaussianSigmaStep,
                    kGaussianSigmaMin,
                    kGaussianSigmaMax,
                    "%.2f")) {
                ctx.postProcess->SetGaussianSigma(sigma);
            }

            uint32_t sampleCount = ctx.postProcess->GetGaussianKernelSize() * kSeparablePassCount; // 分離処理の総サンプル数
            ImGui::Text("Samples: %u (separable 2-pass)", sampleCount);
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::LuminanceOutline
            || ctx.postProcess->GetEffectType() == PostEffectType::DepthOutline) {
            float outlineStrength = ctx.postProcess->GetOutlineStrength(); // 現在の輪郭強度
            if (ImGui::DragFloat(
                    "Outline Strength",
                    &outlineStrength,
                    kOutlineStrengthStep,
                    kOutlineStrengthMin,
                    kOutlineStrengthMax,
                    "%.1f")) {
                ctx.postProcess->SetOutlineStrength(outlineStrength);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::DepthOutline) {
            float depthThreshold = ctx.postProcess->GetDepthOutlineThreshold(); // 深度差の判定閾値
            if (ImGui::DragFloat(
                    "Depth Threshold",
                    &depthThreshold,
                    kDepthThresholdStep,
                    kPostProcessNormalizedMin,
                    kDepthThresholdMax,
                    "%.3f")) {
                ctx.postProcess->SetDepthOutlineThreshold(depthThreshold);
            }

            float depthSoftness = ctx.postProcess->GetDepthOutlineSoftness(); // 輪郭の立ち上がり幅
            if (ImGui::DragFloat(
                    "Depth Softness",
                    &depthSoftness,
                    kDepthSoftnessStep,
                    kDepthSoftnessMin,
                    kDepthSoftnessMax,
                    "%.3f")) {
                ctx.postProcess->SetDepthOutlineSoftness(depthSoftness);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::RadialBlur) {
            Math::Vector2 center = ctx.postProcess->GetRadialBlurCenter(); // 現在のブラー中心
            float centerValues[2] = {
                center.x,
                center.y
            }; // ImGui編集用中心座標
            if (ImGui::DragFloat2(
                    "Blur Center",
                    centerValues,
                    kEffectCenterStep,
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.3f")) {
                ctx.postProcess->SetRadialBlurCenter(
                    { centerValues[0], centerValues[1] });
            }

            float blurWidth = ctx.postProcess->GetRadialBlurWidth(); // 現在のブラー幅
            if (ImGui::DragFloat(
                    "Blur Width",
                    &blurWidth,
                    kRadialBlurWidthStep,
                    kPostProcessNormalizedMin,
                    kRadialBlurWidthMax,
                    "%.4f")) {
                ctx.postProcess->SetRadialBlurWidth(blurWidth);
            }

            int sampleCount = static_cast<int>(ctx.postProcess->GetRadialBlurSampleCount()); // サンプル数
            if (ImGui::SliderInt(
                    "Sample Count",
                    &sampleCount,
                    kRadialBlurSampleMin,
                    kRadialBlurSampleMax)) {
                ctx.postProcess->SetRadialBlurSampleCount(
                    static_cast<uint32_t>(sampleCount));
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Distortion) {
            Math::Vector2 center = ctx.postProcess->GetDistortionCenter(); // 現在の歪み中心
            float centerValues[2] = { center.x, center.y }; // ImGui編集用の歪み中心
            if (ImGui::DragFloat2(
                    "Distortion Center",
                    centerValues,
                    kEffectCenterStep,
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.3f")) {
                ctx.postProcess->SetDistortionCenter(
                    { centerValues[0], centerValues[1] });
            }

            float strength = ctx.postProcess->GetDistortionStrength(); // 現在の歪み強度
            if (ImGui::DragFloat(
                    "Distortion Strength",
                    &strength,
                    kDistortionStrengthStep,
                    kDistortionStrengthMin,
                    kDistortionStrengthMax,
                    "%.3f")) {
                ctx.postProcess->SetDistortionStrength(strength);
            }

            float radius = ctx.postProcess->GetDistortionRadius(); // 現在の歪み半径
            if (ImGui::DragFloat(
                    "Distortion Radius",
                    &radius,
                    kDistortionRadiusStep,
                    kDistortionRadiusMin,
                    kDistortionRadiusMax,
                    "%.2f")) {
                ctx.postProcess->SetDistortionRadius(radius);
            }

            float waveCount = ctx.postProcess->GetDistortionWaveCount(); // 現在の歪み波数
            if (ImGui::DragFloat(
                    "Distortion Wave Count",
                    &waveCount,
                    kDistortionWaveStep,
                    kPostProcessNormalizedMin,
                    kDistortionWaveMax,
                    "%.1f")) {
                ctx.postProcess->SetDistortionWaveCount(waveCount);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Dissolve) {
            float threshold = ctx.postProcess->GetDissolveThreshold(); // 現在のDissolve閾値
            if (ImGui::SliderFloat(
                    "Threshold",
                    &threshold,
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.3f")) {
                ctx.postProcess->SetDissolveThreshold(threshold);
            }

            float edgeWidth = ctx.postProcess->GetDissolveEdgeWidth(); // 現在の境界幅
            if (ImGui::DragFloat(
                    "Edge Width",
                    &edgeWidth,
                    kDissolveEdgeWidthStep,
                    kDissolveEdgeWidthMin,
                    kDissolveEdgeWidthMax,
                    "%.3f")) {
                ctx.postProcess->SetDissolveEdgeWidth(edgeWidth);
            }

            Math::Vector3 edgeColor = ctx.postProcess->GetDissolveEdgeColor(); // 現在の境界色
            float edgeColorValues[3] = {
                edgeColor.x,
                edgeColor.y,
                edgeColor.z
            }; // ImGui編集用の境界色
            if (ImGui::ColorEdit3("Edge Color", edgeColorValues)) {
                ctx.postProcess->SetDissolveEdgeColor(
                    { edgeColorValues[0],
                        edgeColorValues[1],
                        edgeColorValues[2] });
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Random) {
            float randomStrength = ctx.postProcess->GetRandomStrength(); // 現在のノイズ強度
            if (ImGui::SliderFloat(
                    "Noise Strength",
                    &randomStrength,
                    kPostProcessNormalizedMin,
                    kPostProcessNormalizedMax,
                    "%.2f")) {
                ctx.postProcess->SetRandomStrength(randomStrength);
            }

            float randomScale = ctx.postProcess->GetRandomScale(); // 現在のノイズの細かさ
            if (ImGui::DragFloat(
                    "Noise Scale",
                    &randomScale,
                    kRandomScaleStep,
                    kRandomScaleMin,
                    kRandomScaleMax,
                    "%.0f")) {
                ctx.postProcess->SetRandomScale(randomScale);
            }

            float randomSpeed = ctx.postProcess->GetRandomSpeed(); // 現在の変化速度
            if (ImGui::DragFloat(
                    "Noise Speed",
                    &randomSpeed,
                    kRandomSpeedStep,
                    kPostProcessNormalizedMin,
                    kRandomSpeedMax,
                    "%.2f")) {
                ctx.postProcess->SetRandomSpeed(randomSpeed);
            }
        }

        ImGui::SeparatorText("Status");
        ImGui::Text(
            "PostProcess: %s",
            ctx.postProcess->IsReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Copy PSO: %s",
            ctx.postProcess->IsCopyReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Grayscale PSO: %s",
            ctx.postProcess->IsGrayscaleReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Vignette PSO: %s",
            ctx.postProcess->IsVignetteReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Box Filter PSO: %s",
            ctx.postProcess->IsBoxFilterReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Gaussian Filter PSO: %s",
            ctx.postProcess->IsGaussianFilterReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Luminance Outline PSO: %s",
            ctx.postProcess->IsLuminanceOutlineReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Depth Outline PSO: %s",
            ctx.postProcess->IsDepthOutlineReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Radial Blur PSO: %s",
            ctx.postProcess->IsRadialBlurReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Dissolve PSO: %s",
            ctx.postProcess->IsDissolveReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Random PSO: %s",
            ctx.postProcess->IsRandomReady() ? "Ready" : "Not Ready");
        ImGui::Text(
            "Distortion PSO: %s",
            ctx.postProcess->IsDistortionReady() ? "Ready" : "Not Ready");

        uint32_t srvIndex = ctx.postProcess->GetLastSrvIndex(); // 最後に描画した入力SRV
        if (srvIndex == UINT32_MAX) {
            ImGui::Text("Input SRV: Not Drawn");
        } else {
            ImGui::Text("Input SRV: %u", srvIndex);
        }

        ImGui::TextDisabled("Disabled uses the Copy pipeline.");
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// パーティクル関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawParticleSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.particleEmitter && !ctx.particleManager) {
        return;
    }

    if (ImGui::CollapsingHeader("Particle")) {
        if (ctx.particleEmitter) {
            ImGui::PushID(ctx.particleEmitter);
            ctx.particleEmitter->DrawImGui();
            ImGui::PopID();
        }

        ImGui::Separator();

        if (ctx.particleManager) {
            ctx.particleManager->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// 3Dオブジェクト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawObjectSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.objects3d || ctx.objects3d->empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Objects")) {
        static int selectedObjectIndex = 0; // 編集対象の3Dオブジェクト番号
        const int objectCount = static_cast<int>(ctx.objects3d->size()); // 表示可能な3Dオブジェクト数
        selectedObjectIndex = (std::clamp)(selectedObjectIndex, 0, objectCount - 1);

        Object3d* previewObject = (*ctx.objects3d)[selectedObjectIndex]; // コンボで現在選択している3Dオブジェクト
        std::string preview = "Object " + std::to_string(selectedObjectIndex); // コンボの現在表示名
        if (previewObject && !previewObject->GetDebugName().empty()) {
            preview += " : " + previewObject->GetDebugName();
        }

        if (ImGui::BeginCombo("Target", preview.c_str())) {
            for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
                Object3d* object = (*ctx.objects3d)[objectIndex]; // 表示名を取得する3Dオブジェクト
                std::string label = "Object " + std::to_string(objectIndex); // 選択候補の表示名
                if (object && !object->GetDebugName().empty()) {
                    label += " : " + object->GetDebugName();
                }

                const bool isSelected = selectedObjectIndex == objectIndex; // 現在選択中か
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedObjectIndex = objectIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Object3d* selectedObject = (*ctx.objects3d)[selectedObjectIndex]; // 編集対象の3Dオブジェクト
        if (selectedObject) {
            ImGui::Separator();
            selectedObject->DrawImGui(selectedObjectIndex);
        }
    }
#else
    (void)ctx;
#endif
}
/// <summary>
/// スプライト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawSpriteSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.sprites || ctx.sprites->empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Sprites")) {
        static int selectedSpriteIndex = 0; // 編集対象のスプライト番号
        const int spriteCount = static_cast<int>(ctx.sprites->size()); // 表示可能なスプライト数
        selectedSpriteIndex = (std::clamp)(selectedSpriteIndex, 0, spriteCount - 1);

        Sprite* previewSprite = (*ctx.sprites)[selectedSpriteIndex]; // コンボで現在選択しているスプライト
        std::string preview = "Sprite " + std::to_string(selectedSpriteIndex); // コンボの現在表示名
        if (previewSprite && !previewSprite->GetTextureFilePath().empty()) {
            preview += " : " + previewSprite->GetTextureFilePath();
        }

        if (ImGui::BeginCombo("Target", preview.c_str())) {
            for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
                Sprite* sprite = (*ctx.sprites)[spriteIndex]; // 表示名を取得するスプライト
                std::string label = "Sprite " + std::to_string(spriteIndex); // 選択候補の表示名
                if (sprite && !sprite->GetTextureFilePath().empty()) {
                    label += " : " + sprite->GetTextureFilePath();
                }

                const bool isSelected = selectedSpriteIndex == spriteIndex; // 現在選択中か
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedSpriteIndex = spriteIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Sprite* selectedSprite = (*ctx.sprites)[selectedSpriteIndex]; // 編集対象のスプライト
        if (selectedSprite) {
            ImGui::Separator();
            selectedSprite->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}
/// <summary>
/// 共通設定関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCommonSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Common")) {
        if (ctx.spriteCommon || ctx.object3dCommon) {
            if (ImGui::CollapsingHeader("Blend Mode")) {
                const char* blendNames[] = {
                    "None",
                    "Alpha",
                    "Add",
                    "Subtract",
                    "Multiply",
                    "Screen"
                }; // 選択可能なブレンドモード名

                if (ctx.object3dCommon) {
                    int objectBlendIndex = static_cast<int>(ctx.object3dCommon->GetBlendMode()); // 3Dオブジェクトのブレンドモード
                    if (ImGui::Combo("1. Object3D Blend", &objectBlendIndex, blendNames, IM_ARRAYSIZE(blendNames))) {
                        ctx.object3dCommon->SetBlendMode(static_cast<BlendMode>(objectBlendIndex));
                    }
                }

                if (ctx.spriteCommon) {
                    int spriteBlendIndex = static_cast<int>(ctx.spriteCommon->GetBlendMode()); // スプライトのブレンドモード
                    if (ImGui::Combo("2. Sprite Blend", &spriteBlendIndex, blendNames, IM_ARRAYSIZE(blendNames))) {
                        ctx.spriteCommon->SetBlendMode(static_cast<BlendMode>(spriteBlendIndex));
                    }
                }
            }
        }

        if (ctx.object3dCommon) {
            ctx.object3dCommon->DrawImGui();
        }

        if (ctx.spriteCommon) {
            ctx.spriteCommon->DrawImGui();
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// カメラ関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCameraWindow(Context& ctx)
{
#ifdef USE_IMGUI
    ImGui::Begin("Camera");

    if (ctx.object3dCommon) {
        ctx.object3dCommon->DrawCameraImGui();
    }

    ImGui::End();
#else
    (void)ctx;
#endif
}

#else
// ImGuiを使用しない場合は、すべての関数を空実装にする

void ImGuiManager::NewFrame() { }

void ImGuiManager::Initialize(void* /*hwnd*/, SrvManager* /*srvManager*/) { }

void ImGuiManager::Shutdown() { }

void ImGuiManager::BuildUI(Context& /*ctx*/) { }

void ImGuiManager::Render(ID3D12GraphicsCommandList* /*commandList*/) { }

bool ImGuiManager::IsCapturingInput() { return false; }

#endif // USE_IMGUI

} // namespace MyEngine




