#include "ImGuiManager.h"
#include "engine/utility/Logger.h"
#include <cstdint>
#include <d3d12.h>
#include <sstream>

#include "ImGuiManager.h"

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
#include <cmath>
#include <unordered_set>

using namespace MyEngine;

namespace MyEngine {

#ifdef USE_IMGUI

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

    ImGui::Begin("Settings");

    DrawSceneSection(ctx);
    DrawViewFilterSection(ctx);
    DrawPostProcessSection(ctx);

    int selectedDrawType = ctx.selectedDrawType ? *ctx.selectedDrawType : -1; // 現在選択されている表示対象

    DrawObjectSection(ctx, selectedDrawType);
    DrawParticleSection(ctx, selectedDrawType);
    DrawSpriteSection(ctx, selectedDrawType);
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
/// シーン情報をImGuiで描画する
/// </summary>
void ImGuiManager::DrawSceneSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ctx.currentSceneName) {
            ImGui::Text("Current Scene: %s", ctx.currentSceneName);
        }
    }
#else
    (void)ctx;
#endif
}

/// <summary>
/// 表示対象の選択UIを描画する
/// </summary>
void ImGuiManager::DrawViewFilterSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (!ctx.selectedDrawType) {
        return;
    }

    if (ImGui::CollapsingHeader("View Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* drawOptions[] = {
            "Model",
            "Particle",
            "Sprite",
            "Bunny",
            "Fence",
            "Checker",
            "Sphere",
            "All"
        };

        ImGui::Combo("Draw Type", ctx.selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));
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
            int kernelIndex = ctx.postProcess->GetBoxFilterKernelSize() == 5 ? 1 : 0; // 現在のカーネル番号
            const char* kernelNames[] = {
                "3x3",
                "5x5"
            }; // 選択可能なカーネルサイズ

            if (ImGui::Combo(
                    "Kernel Size",
                    &kernelIndex,
                    kernelNames,
                    IM_ARRAYSIZE(kernelNames))) {
                uint32_t kernelSize = kernelIndex == 1 ? 5u : 3u; // 選択されたカーネルサイズ
                ctx.postProcess->SetBoxFilterKernelSize(kernelSize);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::GaussianFilter) {
            uint32_t currentKernelSize = ctx.postProcess->GetGaussianKernelSize(); // 現在のカーネルサイズ
            int kernelIndex = currentKernelSize == 3 ? 0 : (currentKernelSize == 5 ? 1 : 2);
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
                const uint32_t kernelSizes[] = { 3u, 5u, 7u }; // 選択値に対応するサイズ
                ctx.postProcess->SetGaussianKernelSize(kernelSizes[kernelIndex]);
            }

            float sigma = ctx.postProcess->GetGaussianSigma(); // 現在の標準偏差
            if (ImGui::DragFloat(
                    "Sigma",
                    &sigma,
                    0.05f,
                    0.1f,
                    10.0f,
                    "%.2f")) {
                ctx.postProcess->SetGaussianSigma(sigma);
            }

            uint32_t sampleCount = ctx.postProcess->GetGaussianKernelSize() * 2; // 分離処理の総サンプル数
            ImGui::Text("Samples: %u (separable 2-pass)", sampleCount);
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::LuminanceOutline
            || ctx.postProcess->GetEffectType() == PostEffectType::DepthOutline) {
            float outlineStrength = ctx.postProcess->GetOutlineStrength(); // 現在の輪郭強度
            if (ImGui::DragFloat(
                    "Outline Strength",
                    &outlineStrength,
                    0.1f,
                    0.0f,
                    32.0f,
                    "%.1f")) {
                ctx.postProcess->SetOutlineStrength(outlineStrength);
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::DepthOutline) {
            float depthThreshold = ctx.postProcess->GetDepthOutlineThreshold(); // 深度差の判定閾値
            if (ImGui::DragFloat(
                    "Depth Threshold",
                    &depthThreshold,
                    0.001f,
                    0.0f,
                    0.2f,
                    "%.3f")) {
                ctx.postProcess->SetDepthOutlineThreshold(depthThreshold);
            }

            float depthSoftness = ctx.postProcess->GetDepthOutlineSoftness(); // 輪郭の立ち上がり幅
            if (ImGui::DragFloat(
                    "Depth Softness",
                    &depthSoftness,
                    0.001f,
                    0.001f,
                    0.2f,
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
                    0.005f,
                    0.0f,
                    1.0f,
                    "%.3f")) {
                ctx.postProcess->SetRadialBlurCenter(
                    { centerValues[0], centerValues[1] });
            }

            float blurWidth = ctx.postProcess->GetRadialBlurWidth(); // 現在のブラー幅
            if (ImGui::DragFloat(
                    "Blur Width",
                    &blurWidth,
                    0.0005f,
                    0.0f,
                    0.1f,
                    "%.4f")) {
                ctx.postProcess->SetRadialBlurWidth(blurWidth);
            }

            int sampleCount = static_cast<int>(ctx.postProcess->GetRadialBlurSampleCount()); // サンプル数
            if (ImGui::SliderInt(
                    "Sample Count",
                    &sampleCount,
                    1,
                    32)) {
                ctx.postProcess->SetRadialBlurSampleCount(
                    static_cast<uint32_t>(sampleCount));
            }
        }

        if (ctx.postProcess->GetEffectType() == PostEffectType::Dissolve) {
            float threshold = ctx.postProcess->GetDissolveThreshold(); // 現在のDissolve閾値
            if (ImGui::SliderFloat(
                    "Threshold",
                    &threshold,
                    0.0f,
                    1.0f,
                    "%.3f")) {
                ctx.postProcess->SetDissolveThreshold(threshold);
            }

            float edgeWidth = ctx.postProcess->GetDissolveEdgeWidth(); // 現在の境界幅
            if (ImGui::DragFloat(
                    "Edge Width",
                    &edgeWidth,
                    0.001f,
                    0.001f,
                    0.25f,
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
                    0.0f,
                    1.0f,
                    "%.2f")) {
                ctx.postProcess->SetRandomStrength(randomStrength);
            }

            float randomScale = ctx.postProcess->GetRandomScale(); // 現在のノイズの細かさ
            if (ImGui::DragFloat(
                    "Noise Scale",
                    &randomScale,
                    1.0f,
                    1.0f,
                    2000.0f,
                    "%.0f")) {
                ctx.postProcess->SetRandomScale(randomScale);
            }

            float randomSpeed = ctx.postProcess->GetRandomSpeed(); // 現在の変化速度
            if (ImGui::DragFloat(
                    "Noise Speed",
                    &randomSpeed,
                    0.05f,
                    0.0f,
                    20.0f,
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
void ImGuiManager::DrawParticleSection(Context& ctx, int selectedDrawType)
{
#ifdef USE_IMGUI
    if (selectedDrawType != 1 && selectedDrawType != 7) {
        return;
    }

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
    (void)selectedDrawType;
#endif
}

/// <summary>
/// 3Dオブジェクト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawObjectSection(Context& ctx, int selectedDrawType)
{
#ifdef USE_IMGUI
    if (!ctx.objects3d) {
        return;
    }

    auto IsVisibleObject = [](int selectedDrawType, int objectIndex) -> bool {
        // 未選択またはAllの場合はすべて表示
        if (selectedDrawType == -1 || selectedDrawType == 7) {
            return true;
        }

        switch (selectedDrawType) {
        case 0:
            return objectIndex == 0; // Model
        case 3:
            return objectIndex == 1; // Bunny
        case 4:
            return objectIndex == 3; // Fence
        case 5:
            return objectIndex == 2; // Checker
        case 6:
            return objectIndex == 4 || objectIndex == 6; // Sphere / reflected cube
        default:
            return false;
        }
    };

    bool hasVisibleObject = false; // 表示対象のオブジェクトがあるか
    for (int objectIndex = 0; objectIndex < static_cast<int>(ctx.objects3d->size()); ++objectIndex) {
        Object3d* object = (*ctx.objects3d)[objectIndex]; // 確認対象の3Dオブジェクト
        if (object && IsVisibleObject(selectedDrawType, objectIndex)) {
            hasVisibleObject = true;
            break;
        }
    }

    if (!hasVisibleObject) {
        return;
    }

    if (ImGui::CollapsingHeader("Objects")) {
        for (int objectIndex = 0; objectIndex < static_cast<int>(ctx.objects3d->size()); ++objectIndex) {
            Object3d* object = (*ctx.objects3d)[objectIndex]; // 表示対象の3Dオブジェクト
            if (!object) {
                continue;
            }

            if (!IsVisibleObject(selectedDrawType, objectIndex)) {
                continue;
            }

            ImGui::PushID(objectIndex);

            char header[64] = {};
            sprintf_s(header, "Object %d", objectIndex);

            if (ImGui::CollapsingHeader(header)) {
                object->DrawImGui(objectIndex);
            }

            ImGui::PopID();
        }
    }
#else
    (void)ctx;
    (void)selectedDrawType;
#endif
}

/// <summary>
/// スプライト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawSpriteSection(Context& ctx, int selectedDrawType)
{
#ifdef USE_IMGUI
    if (selectedDrawType != 2 && selectedDrawType != 7) {
        return;
    }

    if (!ctx.sprites) {
        return;
    }

    if (ImGui::CollapsingHeader("Sprites")) {
        int spriteIndex = 0; // 表示中のスプライト番号

        for (auto* sprite : *ctx.sprites) {
            if (!sprite) {
                ++spriteIndex;
                continue;
            }

            ImGui::PushID(spriteIndex);

            char header[64] = {};
            sprintf_s(header, "Sprite %d", spriteIndex);

            if (ImGui::CollapsingHeader(header)) {
                sprite->DrawImGui();
            }

            ImGui::PopID();
            ++spriteIndex;
        }
    }
#else
    (void)ctx;
    (void)selectedDrawType;
#endif
}

/// <summary>
/// 3Dオブジェクト関連のImGuiを描画する
/// </summary>
void ImGuiManager::DrawCommonSection(Context& ctx)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Common")) {
        if (ctx.spriteCommon) {
            ctx.spriteCommon->DrawImGui();
        }

        if (ctx.object3dCommon) {
            ctx.object3dCommon->DrawImGui();
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
