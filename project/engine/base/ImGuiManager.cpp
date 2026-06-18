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
