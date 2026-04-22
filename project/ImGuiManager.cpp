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

    // メインウィンドウの開始
    ImGui::Begin("Settings");

    // 現在のシーン名を表示（あれば）
    if (ctx.currentSceneName) {
        ImGui::Text("Current Scene: %s", ctx.currentSceneName);
    }

    // シーン切替UI（デバッグ用）
    if (ImGui::BeginCombo("Change Scene", "Select")) {
        if (ImGui::Selectable("Title")) {
            if (ctx.requestSceneChange)
                ctx.requestSceneChange("Title");
        }
        if (ImGui::Selectable("Play")) {
            if (ctx.requestSceneChange)
                ctx.requestSceneChange("Play");
        }
        ImGui::EndCombo();
    }

    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4 bg = style.Colors[ImGuiCol_WindowBg];
        if (ImGui::ColorEdit3("UI Window Bg", (float*)&bg)) {
            style.Colors[ImGuiCol_WindowBg] = bg;
        }
    }

    // タイトルシーン用の簡易フルスクリーンUI（デバッグ／見た目確認用）
    if (ctx.currentSceneName && strcmp(ctx.currentSceneName, "Title") == 0) {
        // 中央に大きなウィンドウを表示して Start ボタンを置く
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::Begin("TitleScreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Dummy(ImVec2(0, 10));
        // 中央揃えでタイトルを描画
        float textW = ImGui::CalcTextSize("GAME TITLE").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textW) * 0.5f);
        ImGui::Text("GAME TITLE");
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 80.0f) * 0.5f);
        if (ImGui::Button("Start", ImVec2(80, 40))) {
            if (ctx.requestSceneChange)
                ctx.requestSceneChange("Play");
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    // 描画タイプ選択UI（例: Model, Particle, Sprite など)
    if (ctx.selectedDrawType) {
        const char* drawOptions[] = { "Model", "Particle", "Sprite", "Bunny", "Fence", "Checker", "Sphere", "All" };
        ImGui::Combo("Model", ctx.selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));
    }

    // 現在の描画タイプ選択をローカル変数にコピーして使用
    int sel = ctx.selectedDrawType ? *ctx.selectedDrawType : -1;

    // オブジェクトごとのUI（Model, Bunny, Fence, Checker, Sphere のみ）
    if (ctx.objects3d) {
        // 現在の選択状態に基づいて表示されるべきオブジェクトの数をカウント
        int visibleCount = 0;
        for (size_t i = 0; i < ctx.objects3d->size(); ++i) {
            auto obj = (*ctx.objects3d)[i];
            if (!obj) continue;
            // それぞれのオブジェクトが現在の選択に対して表示されるべきかを判定
            auto isVisibleLocal = [&](int idx) -> bool {
                if (sel == -1) return true;
                if (sel == 7) return true;
                switch (sel) {
                case 0: return idx == 0;
                case 3: return idx == 1;
                case 4: return idx == 3;
                case 5: return idx == 2;
                case 6: return idx == 4;
                default: return false;
                }
            };
            if (isVisibleLocal(static_cast<int>(i))) ++visibleCount;
        }
        if (visibleCount > 0) {
            ImGui::Separator();
            ImGui::Text("Objects");

        // それぞれのオブジェクトが現在の選択に対して表示されるべきかを判定するラムダ関数
        auto isVisible = [&](int idx) -> bool {
            // sel == -1 (None) なら全て表示、sel == 7 (All) なら全て表示、それ以外は idx に対応するオブジェクトのみ表示
            if (sel == -1) {
                return true; // None なら全て表示
            }
            if (sel == 7) {
                return true; // All なら全て表示
            }

            // それ以外は idx に対応するオブジェクトのみ表示 (0=Model, 3=Bunny, 4=Fence, 5=Checker, 6=Sphere)
            switch (sel) {
            case 0:
                return idx == 0; // Model
            case 3:
                return idx == 1; // Bunny
            case 4:
                return idx == 3; // Fence
            case 5:
                return idx == 2; // Checker
            case 6:
                return idx == 4; // Sphere
            default:
                return false; // その他の選択肢ではオブジェクトは表示しない
            }
        };

            // オブジェクトごとにUIを表示
            int idx = 0;
            // それぞれのオブジェクトが現在の選択に対して表示されるべきかを判定するラムダ関数を使用して、表示すべきオブジェクトのみUIを構築する
            for (auto obj : *ctx.objects3d) {
            // obj が nullptr ならスキップ
            if (!obj) {
                ++idx;
                continue;
            }
            // 現在の選択に対してこのオブジェクトが表示されるべきかを判定
            if (!isVisible(idx)) {
                ++idx;
                continue;
            }
                ImGui::PushID(idx);
                // ヘッダにオブジェクトの種類とインデックスを表示 (例: "Object 0", "Object 1", ...)
                char header[64];
                sprintf_s(header, "Object %d", idx);
                // 現在のオブジェクトのUIを表示
                if (ImGui::CollapsingHeader(header)) {
                    obj->DrawImGui(idx);
                }

                ImGui::PopID();
            ++idx;
        }
        }
    }

    // 描画タイプ選択に基づいて、ParticleセクションとSpritesセクションの表示を制御

    // Particleセクション: 表示は選択状態（Particle または All）のときのみ行う
    if ((sel == 1 || sel == 7) && (ctx.particleEmitter || ctx.particleManager)) {
        if (ImGui::CollapsingHeader("Particle")) {
            // パーティクルエミッタのUIを表示
            if (ctx.particleEmitter) {
                ImGui::PushID((void*)ctx.particleEmitter);
                ctx.particleEmitter->DrawImGui();
                ImGui::PopID();
            }

            ImGui::Separator();

            // パーティクルマネージャのUIを表示
            if (ctx.particleManager) {
                ctx.particleManager->DrawImGui();
            }

            // パーティクルグループのテクスチャ選択UIを表示
            if (ctx.particleManager) {
                const auto& groups = ctx.particleManager->GetGroups();
                if (!groups.empty()) {
                    auto loaded = TextureManager::GetInstance()->GetLoadedTextureFilePaths();
                    if (!loaded.empty()) {
                        std::vector<std::string> basenames;
                        basenames.reserve(loaded.size());
                        std::vector<std::string> fullPaths;
                        fullPaths.reserve(loaded.size());
                        std::unordered_set<std::string> seen;
                        for (const auto& p : loaded) {
                            size_t pos = p.find_last_of("/\\");
                            std::string name = (pos == std::string::npos) ? p : p.substr(pos + 1);
                            if (seen.find(name) != seen.end())
                                continue;
                            seen.insert(name);
                            basenames.push_back(name);
                            fullPaths.push_back(p);
                        }

                        if (!basenames.empty()) {
                            ImGui::Separator();
                            ImGui::Text("Particle Group Textures");
                            for (const auto& kv : groups) {
                                const std::string& gname = kv.first;
                                const ParticleGroup& grp = kv.second;
                                std::vector<const char*> items;
                                items.reserve(basenames.size());
                                for (const auto& b : basenames) items.push_back(b.c_str());
                                int cur = 0;
                                std::string curName;
                                if (!grp.texturePath.empty()) {
                                    size_t pos = grp.texturePath.find_last_of("/\\");
                                    curName = (pos == std::string::npos) ? grp.texturePath : grp.texturePath.substr(pos + 1);
                                }
                                for (size_t i = 0; i < basenames.size(); ++i) {
                                    if (basenames[i] == curName) { cur = static_cast<int>(i); break; }
                                }
                                ImGui::PushID(gname.c_str());
                                ImGui::Text("%s", gname.c_str());
                                if (ImGui::Combo("Texture", &cur, items.data(), static_cast<int>(items.size()))) {
                                    ctx.particleManager->SetGroupTexture(gname, fullPaths[cur]);
                                }
                                ImGui::PopID();
                            }
                        }
                    }
                }
            }
        }
    }

    // スプライトセクション
    if ((sel == 2 || sel == 7) && ctx.sprites && ctx.spriteCommon) {
        if (ImGui::CollapsingHeader("Sprites")) {
            int sidx = 0;
            for (auto s : *ctx.sprites) {
                if (!s) { ++sidx; continue; }
                ImGui::PushID(sidx);
                char header[64];
                sprintf_s(header, "Sprite %d", sidx);
                if (ImGui::CollapsingHeader(header)) {
                    s->DrawImGui();
                }
                ImGui::PopID();
                ++sidx;
            }
        }
    }

    // Object3dCommon の UI はクラス側に委譲済み (DrawImGui)
    if (ctx.spriteCommon) {
        ctx.spriteCommon->DrawImGui();
    }

    // Object3dCommon の UI はクラス側に委譲済み (DrawImGui)
    if (ctx.object3dCommon) {
        ctx.object3dCommon->DrawImGui();
    }

    ImGui::End();

    // カメラのUIは常に表示する
    ImGui::Begin("Camera");
    if (ctx.object3dCommon) {
        ctx.object3dCommon->DrawCameraImGui();
    }

    ImGui::End();
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

#else
// ImGuiを使用しない場合は、すべての関数を空実装にする

void ImGuiManager::NewFrame() { }

void ImGuiManager::Initialize(void* /*hwnd*/, SrvManager* /*srvManager*/) { }

void ImGuiManager::Shutdown() { }

void ImGuiManager::BuildUI(Context& /*ctx*/) { }

void ImGuiManager::Render(ID3D12GraphicsCommandList* /*commandList*/) { }

#endif // USE_IMGUI

} // namespace MyEngine
