#include "engine/particle/ParticleManager.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/GpuEmitterSettingsUtility.h"
#include "engine/base/PostProcess.h"
#include "engine/utility/FileUtility.h"
#include "externals/imgui/imgui_internal.h"
#include <Windows.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace {
std::unordered_map<ImGuiID, ImRect> itemRects; // ImGuiのテストフックで記録した項目領域
std::unordered_map<std::string, ImRect> labeledRects; // 操作名から参照する項目領域
float panelWidth = 420.0f; // 検証するパネル幅
bool openHeaders = true; // 初期検証で全項目を展開するか

/// <summary>
/// 検証条件を満たさない場合は理由付きでテストを終了する。
/// </summary>
void Require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// GPU描画なしで本番の編集UIを1フレーム実行する。
/// </summary>
void Frame()
{
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, 2700.0f));
    ImGui::Begin("Particle Editor Test", nullptr, ImGuiWindowFlags_NoSavedSettings);
    itemRects.clear();
    labeledRects.clear();
    MyEngine::ParticleManager::GetInstance()->DrawImGui(nullptr);
    ImGuiTabBar* tabBar = ImGui::GetCurrentContext()->TabBars.GetByKey(ImGui::GetID("ParticleEditorTabs")); // タブの配置情報
    if (tabBar) {
        for (ImGuiTabItem& tab : tabBar->Tabs) { // 操作可能な編集タブ
            const float left = tabBar->BarRect.Min.x + tab.Offset; // タブの左端
            labeledRects[ImGui::TabBarGetTabName(tabBar, &tab)] = ImRect(left, tabBar->BarRect.Min.y, left + tab.Width, tabBar->BarRect.Max.y);
        }
    }
    ImGui::End();
    ImGui::Render();
}

/// <summary>
/// テストフックで特定した項目をマウスの押下・解放で操作する。
/// </summary>
void Click(const std::string& label)
{
    const auto found = labeledRects.find(label); // 操作対象の領域
    if (found == labeledRects.end()) {
        for (const auto& [availableLabel, rect] : labeledRects) { // 失敗時の操作候補
            (void)rect;
            std::cerr << "Available item: " << availableLabel << '\n';
        }
    }
    Require(found != labeledRects.end(), ("Missing item: " + label).c_str());
    ImVec2 center = found->second.GetCenter(); // 操作対象の中央座標
    ImGuiIO& io = ImGui::GetIO(); // テスト用の入力状態
    io.AddMousePosEvent(center.x, center.y);
    Frame();
    const auto refreshed = labeledRects.find(label); // ポップアップの初回配置後の領域
    if (refreshed != labeledRects.end()) {
        center = refreshed->second.GetCenter();
        io.AddMousePosEvent(center.x, center.y);
        Frame();
    }
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
    Frame();
}

/// <summary>
/// 保存済みファイルを読み取り、比較用の文字列を取得する。
/// </summary>
std::string Read(const std::string& path)
{
    std::string text; // 保存済みのJSON文字列
    Require(FileUtility::TryReadText(path, text), "Cannot read test preset");
    return text;
}

/// <summary>
/// プリセット保存と確認ダイアログを本番のUI経由で検証する。
/// </summary>
void TestPresetSave()
{
    auto* manager = MyEngine::ParticleManager::GetInstance(); // GPUを初期化しない検証対象
    const std::string path = MyEngine::GpuEmitterSettingsUtility::BuildSettingsPath("gpu_particle"); // 検証用作業フォルダー内の保存先
    Require(!FileUtility::Exists(path), "Test preset already exists");
    Frame();
    Frame();
    Click("Save Preset");
    const std::string original = Read(path); // 保存失敗時に保持されるべき内容
    const auto document = nlohmann::json::parse(original); // 本番のシリアライズ結果
    Require(document.at("version") == 2, "Preset version changed");
    Require(document.at("emitter").at("count") == manager->GetGpuEmitterState()->count, "Emitter count was not saved");

    manager->GetMutableGpuEmitterState()->radius = 4.25f;
    Click("Save Preset");
    Require(Read(path) == original, "Preset changed before overwrite confirmation");
    Click("Cancel");
    Require(Read(path) == original, "Cancel changed the preset");

    manager->GetMutableGpuEmitterState()->lifeTime = (std::numeric_limits<float>::quiet_NaN)();
    Click("Save Preset");
    Click("Overwrite");
    Require(Read(path) == original, "Non-finite settings destroyed the original preset");
    manager->GetMutableGpuEmitterState()->lifeTime = 2.5f;

    Require(SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "Cannot protect the test preset");
    Click("Save Preset");
    Click("Overwrite");
    Require(Read(path) == original, "Write failure destroyed the original preset");
    Require(SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL) != 0, "Cannot restore test file attributes");
    Click("Save Preset");
    Click("Overwrite");
    const auto changed = nlohmann::json::parse(Read(path)); // 正常な上書き後の内容
    Require(changed.at("emitter").at("radius") == 4.25f, "Valid overwrite failed");
    Click("Delete Preset");
    Click("Cancel");
    Require(FileUtility::Exists(path), "Cancel deleted the preset");

    const std::string alternatePath = MyEngine::GpuEmitterSettingsUtility::BuildSettingsPath("selection_test"); // 保存名と切り離して選択するプリセット
    const std::string alternateContent = Read(path); // 選択だけでは変化しない内容
    Require(FileUtility::WriteText(alternatePath, alternateContent), "Cannot create selection test preset");
    Click("##LoadFile");
    std::string alternateLabel; // コンボ内の選択対象ラベル
    for (const auto& [label, rect] : labeledRects) { // 本番UIの項目一覧
        (void)rect;
        if (label.starts_with("selection_test##")) {
            alternateLabel = label;
        }
    }
    Require(!alternateLabel.empty(), "Preset list is missing the test preset");
    Click(alternateLabel);
    manager->GetMutableGpuEmitterState()->radius = 6.5f;
    Click("Save Preset");
    Click("Overwrite");
    Require(Read(alternatePath) == alternateContent, "Preset selection changed the save target");
    Require(nlohmann::json::parse(Read(path)).at("emitter").at("radius") == 6.5f, "Save target name was not preserved");

    Click("##EffectName");
    ImGuiIO& io = ImGui::GetIO(); // 長文を入力するためのIO
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    Frame();
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    Frame();
    std::string longName; // 固定長バッファを超える日本語のエフェクト名
    for (int index = 0; index < 40; ++index) { // UTF-8の複数バイト文字を含む入力
        longName += "調整用エフェクト";
    }
    io.AddInputCharactersUTF8(longName.c_str());
    Frame();
    Click("Save Preset");
    Click("Overwrite");
    Require(nlohmann::json::parse(Read(path)).at("effect").at("effectName") == longName, "UTF-8 input was truncated");
}

/// <summary>
/// 狭いパネルと広いパネルの両方で横方向のはみ出しを検証する。
/// </summary>
void TestPanelWidths()
{
    for (float width : { 260.0f, 420.0f, 800.0f }) { // 検証するパネル幅
        panelWidth = width;
        for (const char* tab : { "GPU", "CPU" }) { // 検証する編集タブ
            Frame();
            Click(tab);
            Frame();
            ImGuiWindow* window = ImGui::FindWindowByName("Particle Editor Test"); // 本番UIを配置した検証ウィンドウ
            Require(window != nullptr, "Editor window was not created");
            Require(window->ContentSize.x <= window->InnerRect.GetWidth() + 1.0f, "Editor requires horizontal scrolling");
            Require(ImGui::GetDrawData()->TotalVtxCount > 0, "Editor draw output is blank");
        }
    }
    openHeaders = false;
    Click("GPU");
    const float expandedGpuHeight = ImGui::FindWindowByName("Particle Editor Test")->ContentSize.y; // 展開中のGPU設定の高さ
    Click("Spawn");
    const float collapsedGpuHeight = ImGui::FindWindowByName("Particle Editor Test")->ContentSize.y; // 折りたたみ後のGPU設定の高さ
    Require(collapsedGpuHeight < expandedGpuHeight, "GPU section cannot be collapsed");
    Click("Spawn");
    Require(ImGui::FindWindowByName("Particle Editor Test")->ContentSize.y > collapsedGpuHeight, "GPU section cannot be reopened");
    Click("CPU");
    const float expandedCpuHeight = ImGui::FindWindowByName("Particle Editor Test")->ContentSize.y; // 展開中のCPU設定の高さ
    Click("Lifetime");
    const float collapsedCpuHeight = ImGui::FindWindowByName("Particle Editor Test")->ContentSize.y; // 折りたたみ後のCPU設定の高さ
    Require(collapsedCpuHeight < expandedCpuHeight, "CPU section cannot be collapsed");
    Click("Lifetime");
    Require(ImGui::FindWindowByName("Particle Editor Test")->ContentSize.y > collapsedCpuHeight, "CPU section cannot be reopened");
}

/// <summary>
/// CPUエミッターの全項目を展開してパネル幅への収まりを確認する。
/// </summary>
void TestCpuEmitterWidths()
{
    ParticleEmitter emitter; // GPUを使わない編集対象
    emitter.groupName = "cpu_emitter_test";
    for (float width : { 260.0f, 420.0f, 800.0f }) { // 検証するパネル幅
        for (int frame = 0; frame < 2; ++frame) { // 初回レイアウト後のサイズも確認する
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2(width, 1800.0f));
            ImGui::Begin("CPU Emitter Test", nullptr, ImGuiWindowFlags_NoSavedSettings);
            for (const char* header : { "Emission", "Transform", "Effect Type", "Debug Range" }) { // 展開するCPUエミッター設定
                ImGui::GetStateStorage()->SetInt(ImGui::GetID(header), 1);
            }
            emitter.DrawImGui();
            ImGui::End();
            ImGui::Render();
        }
        ImGuiWindow* window = ImGui::FindWindowByName("CPU Emitter Test"); // エミッター設定を配置した検証ウィンドウ
        Require(window->ContentSize.x <= window->InnerRect.GetWidth() + 1.0f, "CPU emitter requires horizontal scrolling");
        Require(emitter.groupName == "cpu_emitter_test" && emitter.count == 1 && emitter.frequency == 0.0f, "CPU editor changed settings without input");
    }
}
} // namespace

/// <summary>
/// 本番ImGuiが通知する項目領域を検証用に記録する。
/// </summary>
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* context, ImGuiID id, const ImRect& rect, const ImGuiLastItemData*)
{
    itemRects[id] = rect;
    if (context->CurrentWindow && context->CurrentWindow->IDStack.Size > 0
        && id == context->CurrentWindow->GetID("##LoadFile")) {
        labeledRects["##LoadFile"] = rect;
    }
}

/// <summary>
/// 項目名と領域を関連付け、UI操作を座標の固定値に依存させない。
/// </summary>
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags)
{
    const auto found = itemRects.find(id); // 項目名に対応する領域
    if (found != itemRects.end()) {
        labeledRects[label] = found->second;
    }
    for (const char* header : { "Spawn", "Size / Lifetime", "Motion", "Color", "Effect / Texture", "Presets", "PostProcess", "GPU Status", "Lifetime", "Spawn Random", "Velocity / Physics", "Field", "Groups" }) { // タブの内部IDを含む展開対象
        if (openHeaders && std::string(label) == header) {
            ImGui::GetStateStorage()->SetInt(id, 1);
        }
    }
}

/// <summary>
/// このテストではImGui内部ログの出力は不要。
/// </summary>
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}

/// <summary>
/// このテストではImGui内部のデバッグラベル検索は不要。
/// </summary>
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

namespace MyEngine {
// GPU転送だけを代替する。本番のImGui・JSON変換・ファイル保存は代替しない。
/// <summary>
/// GPU未初期化のテストで使用する粒子上限。
/// </summary>
uint32_t ParticleManager::GetParticleLimit() const { return 1024; }
/// <summary>
/// GPU未初期化のため、テクスチャのGPU転送は行わない。
/// </summary>
void ParticleManager::ApplyGpuEmitterTextureToDrawGroup() {}
/// <summary>
/// GPU未初期化のため、生存数のGPU読み戻しは行わない。
/// </summary>
void ParticleManager::UpdateGpuAliveCountEstimate() {}
/// <summary>
/// GPUリソースを作らず、UIが参照する生存数だけを初期化する。
/// </summary>
void ParticleManager::ClearGpuEmitterRuntimeParticleState()
{
    gpuEmitterVisibleCount_ = 0;
    gpuAliveCountEstimate_ = 0;
}
/// <summary>
/// GPUリソースを作らず、UIが参照する再生状態だけを初期化する。
/// </summary>
void ParticleManager::ResetGpuEmitterParticles()
{
    ClearGpuEmitterRuntimeParticleState();
    gpuEmitterState_.frequencyTime = 0.0f;
    gpuEmitterState_.emit = 0;
}
/// <summary>ヘッドレス検証ではCPU粒子を発生させない。</summary>
void ParticleManager::Emit(const std::string&, const Math::Vector3&, uint32_t) { throw std::runtime_error("Unexpected CPU emit"); }
/// <summary>ヘッドレス検証ではCPU粒子を発生させない。</summary>
void ParticleManager::EmitHitEffect(const std::string&, const Math::Vector3&, uint32_t) { throw std::runtime_error("Unexpected CPU emit"); }
/// <summary>ヘッドレス検証ではCPU粒子を発生させない。</summary>
void ParticleManager::EmitRingEffect(const std::string&, const Math::Vector3&, uint32_t) { throw std::runtime_error("Unexpected CPU emit"); }
/// <summary>ヘッドレス検証ではCPU粒子を発生させない。</summary>
void ParticleManager::EmitCylinderEffect(const std::string&, const Math::Vector3&, uint32_t) { throw std::runtime_error("Unexpected CPU emit"); }

// PostProcessはこのテストの対象外。誤って呼ばれた場合は失敗させる。
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetRadialBlurCenter(const Math::Vector2&) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetRadialBlurWidth(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetRadialBlurSampleCount(uint32_t) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDistortionCenter(const Math::Vector2&) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDistortionStrength(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDistortionRadius(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDistortionWaveCount(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDistortionProgress(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDissolveThreshold(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDissolveEdgeWidth(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetDissolveEdgeColor(const Math::Vector3&) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetRandomStrength(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetRandomScale(float) { throw std::runtime_error("Unexpected PostProcess call"); }
/// <summary>ヘッドレス検証ではPostProcessを適用しない。</summary>
void PostProcess::SetRandomSpeed(float) { throw std::runtime_error("Unexpected PostProcess call"); }
} // namespace MyEngine

/// <summary>
/// 検証専用の作業フォルダーとImGuiコンテキストで編集UIを検証する。
/// </summary>
int main()
{
    namespace fs = std::filesystem;
    const fs::path originalDirectory = fs::current_path(); // テスト前の作業フォルダー
    const fs::path testRoot = originalDirectory / "generated/tests" / ("particle-editor-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())); // この実行だけの出力先
    try {
        Require(!fs::exists(testRoot), "Test directory already exists");
        fs::create_directories(testRoot / "work/nested/resources/effects");
        fs::current_path(testRoot / "work/nested");
        const fs::path savePath = fs::absolute(MyEngine::GpuEmitterSettingsUtility::BuildSettingsPath("gpu_particle")); // 実際の保存先
        Require(savePath.parent_path() == fs::current_path() / "resources/effects", "Save path escaped the test directory");
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); // GPUバックエンドを持たない検証用IO
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1200.0f, 2800.0f);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels = nullptr; // フォントの構築に使用する画素参照
        int width = 0; // フォント画像の幅
        int height = 0; // フォント画像の高さ
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ImGui::GetCurrentContext()->TestEngineHookItems = true;
        TestPresetSave();
        TestPanelWidths();
        TestCpuEmitterWidths();
        ImGui::DestroyContext();
        fs::current_path(originalDirectory);
        std::cout << "Particle editor tests passed: safe save, confirmations, UTF-8 input, preset selection, folding, GPU/CPU at 260/420/800 px\n";
        return 0;
    } catch (const std::exception& exception) {
        fs::current_path(originalDirectory);
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
