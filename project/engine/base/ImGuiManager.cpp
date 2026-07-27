#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "engine/base/SrvManager.h"
#include "engine/utility/Logger.h"
#include <algorithm>
#include <limits>
#include <sstream>
#endif

namespace MyEngine {

#ifdef USE_IMGUI
namespace {
/// <summary>
/// ImGuiの標準スタイルへエディター用の配色を適用する。
/// </summary>
void ApplyEditorStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle(); // ImGui全体の見た目設定
    ImVec4* colors = style.Colors; // ImGuiの色設定配列
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
}

/// <summary>
/// Viewport有効時に追加ウィンドウの見た目をメインウィンドウへ合わせる。
/// </summary>
void ApplyViewportStyleIfEnabled(const ImGuiIO& imguiIo)
{
    if (imguiIo.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle(); // Viewport時に補正するImGuiスタイル
        style.WindowRounding = 0.0f; // 外部ウィンドウと通常ウィンドウの見た目を揃える
        style.Colors[ImGuiCol_WindowBg].w = 1.0f; // 外部ウィンドウ背景を不透明にする
    }
}

/// <summary>
/// フォントアトラスの構築状態をログへ出力する。
/// </summary>
void LogFontAtlasState(const char* message, ImFontAtlas* fonts)
{
    if (!message || !fonts) {
        return;
    }

    std::ostringstream oss; // フォントアトラス状態のログ文字列
    oss << message << (fonts->TexIsBuilt ? 1 : 0);
    Logger::Log(oss.str());
}

/// <summary>
/// ImGui選択番号を削除後の要素数に合わせて補正する。
/// </summary>
int ResolveSelectionIndexAfterDelete(int selectedIndex, size_t deletedIndex, size_t remainingCount)
{
    if (remainingCount == 0) {
        return 0;
    }

    const int maxIndex = static_cast<int>(remainingCount - 1); // 削除後に選択できる最大番号
    if (deletedIndex > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return (std::clamp)(selectedIndex, 0, maxIndex);
    }

    const int removedIndex = static_cast<int>(deletedIndex); // 削除された要素番号
    if (selectedIndex == removedIndex) {
        return (std::min)(removedIndex, maxIndex);
    }

    if (selectedIndex > removedIndex) {
        selectedIndex--;
    }
    return (std::clamp)(selectedIndex, 0, maxIndex);
}
} // namespace

/// <summary>
/// ImGui::NewFrame とバックエンドの NewFrame を呼び出して新しいフレームを開始する。
/// </summary>
void ImGuiManager::NewFrame()
{
    // バックエンドの NewFrame を呼び出す
    ImGui_ImplWin32_NewFrame(); // Win32プラットフォームの新しいフレームを開始
    ImGui_ImplDX12_NewFrame(); // DX12レンダラーの新しいフレームを開始
    ImGuiIO& io = ImGui::GetIO(); // ImGui全体のIO状態
    const bool rendererHandlesTextures = (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0; // レンダラー側でテクスチャを扱うか
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

    ApplyEditorStyle();
    ApplyViewportStyleIfEnabled(imguiIo);

    // バックエンドの初期化
    ImGui_ImplWin32_Init(hwnd);

    // SrvManagerが提供されている場合は、ImGuiの初期化もSrvManagerに任せる（SRVヒープの設定などを行うため）。提供されていない場合は、ImGuiの初期化は行わない。
    if (srvManager) {
        srvManager->InitImGui(); // SrvManagerにImGuiの初期化を任せる
    }

    ImGuiIO& io = ImGui::GetIO(); // 初期化後のImGui IO状態
    const bool rendererHandlesTextures = (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0; // レンダラー側でテクスチャを扱うか
    if (!rendererHandlesTextures && io.Fonts) {
        unsigned char* pixels = nullptr; // フォント画像のピクセル先頭
        int width = 0; // フォント画像の横幅
        int height = 0; // フォント画像の縦幅
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        LogFontAtlasState("DEBUG Initialize: Font atlas after GetTexDataAsRGBA32: TexIsBuilt=", io.Fonts);
    } else {
        LogFontAtlasState("DEBUG Initialize: Backend handles textures, TexIsBuilt=", io.Fonts);
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

/// <summary>
/// 3Dオブジェクト削除後の選択状態を補正する。
/// </summary>
void ImGuiManager::NotifyObjectDeleted(size_t deletedObjectIndex, size_t remainingObjectCount)
{
    activeGizmoOperationMode_ = -1;
    activeGizmoAxisIndex_ = -1;
    selectedObjectIndex_ = ResolveSelectionIndexAfterDelete(selectedObjectIndex_, deletedObjectIndex, remainingObjectCount);
}

/// <summary>
/// スプライト削除後の選択状態を補正する。
/// </summary>
void ImGuiManager::NotifySpriteDeleted(size_t deletedSpriteIndex, size_t remainingSpriteCount)
{
    activeGizmoOperationMode_ = -1;
    activeGizmoAxisIndex_ = -1;
    selectedSpriteIndex_ = ResolveSelectionIndexAfterDelete(selectedSpriteIndex_, deletedSpriteIndex, remainingSpriteCount);
}

/// <summary>
/// パーティクルエミッター削除後の選択状態を補正する。
/// </summary>
void ImGuiManager::NotifyParticleEmitterDeleted(size_t deletedEmitterIndex, size_t remainingEmitterCount)
{
    activeGizmoOperationMode_ = -1;
    activeGizmoAxisIndex_ = -1;
    selectedEmitterIndex_ = ResolveSelectionIndexAfterDelete(selectedEmitterIndex_, deletedEmitterIndex, remainingEmitterCount);
}

bool ImGuiManager::IsCapturingInput()
{
    ImGuiIO& io = ImGui::GetIO();
    // ImGuiが現在UIによってマウスやキーボードをキャプチャしているかを判定
    return ImGui::IsAnyItemActive() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || io.WantCaptureMouse || io.WantCaptureKeyboard;
}

/// <summary>
/// メインウィンドウ全体に ImGui のドッキング領域を作成する
/// </summary>
void ImGuiManager::DrawDockSpace()
{
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
}

#else
// ImGuiを使用しない場合は、すべての関数を空実装にする

void ImGuiManager::NewFrame() { }

void ImGuiManager::Initialize(void* /*hwnd*/, SrvManager* /*srvManager*/) { }

void ImGuiManager::Shutdown() { }

void ImGuiManager::BuildUI(Context& /*ctx*/) { }

void ImGuiManager::Render(ID3D12GraphicsCommandList* /*commandList*/) { }

/// <summary>
/// ImGui無効時の3Dオブジェクト削除通知を受け取る。
/// </summary>
void ImGuiManager::NotifyObjectDeleted(size_t /*deletedObjectIndex*/, size_t /*remainingObjectCount*/) { }

/// <summary>
/// ImGui無効時のスプライト削除通知を受け取る。
/// </summary>
void ImGuiManager::NotifySpriteDeleted(size_t /*deletedSpriteIndex*/, size_t /*remainingSpriteCount*/) { }

/// <summary>
/// ImGui無効時のパーティクルエミッター削除通知を受け取る。
/// </summary>
void ImGuiManager::NotifyParticleEmitterDeleted(size_t /*deletedEmitterIndex*/, size_t /*remainingEmitterCount*/) { }

bool ImGuiManager::IsCapturingInput() { return false; }

#endif // USE_IMGUI

} // namespace MyEngine





