#include "application/effects/TemporalRiftEffect.h"
#include "application/effects/TimeStopEffect.h"
#include "application/effects/TimeReversalEffect.h"
#include "engine/3d/Object3d.h"
#include "engine/base/PostProcess.h"
#include "engine/base/ImGuiFontUtility.h"
#include "externals/imgui/imgui_internal.h"
#include <iostream>
#include <stdexcept>

namespace {
/// <summary>
/// 検証条件が満たされない場合は理由付きで失敗させる。
/// </summary>
void Require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 本番のエフェクト編集UIを展開・折りたたみ状態で描画する。
/// </summary>
template<class Effect>
float DrawFrame(Effect& effect, float width, bool expanded, bool verifyLayout = false)
{
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 2900.0f));
    ImGui::Begin("Effect Editor Test", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushID(&effect);
    for (const char* section : { "Status", "Timing", "Radial Blur", "Distortion", "Afterimage", "Impact", "Crack", "Burst", "Particles", "Convergence", "Transform Rewind", "GPU Fallback" }) { // 各エフェクトに存在する設定分類
        ImGui::GetStateStorage()->SetInt(ImGui::GetID(section), expanded ? 1 : 0);
    }
    ImGui::PopID();
    effect.DrawImGui();
    ImGui::End();
    ImGui::Render();
    ImGuiWindow* window = ImGui::FindWindowByName("Effect Editor Test"); // 本番の編集UIを配置した検証ウィンドウ
    Require(window != nullptr, "Editor window is missing");
    if (verifyLayout && window->ContentSize.x > window->InnerRect.GetWidth() + 1.0f) {
        std::cerr << "Layout: panel=" << width << " expanded=" << expanded
            << " content=" << window->ContentSize.x << " inner=" << window->InnerRect.GetWidth() << '\n';
    }
    if (verifyLayout) {
        Require(window->ContentSize.x <= window->InnerRect.GetWidth() + 1.0f, "Effect editor requires horizontal scrolling");
    }
    Require(ImGui::GetDrawData()->TotalVtxCount > 0, "Effect editor draw output is blank");
    Require(!effect.IsPlaying(), "Drawing the editor started an effect");
    return window->ContentSize.y;
}

/// <summary>
/// 幅ごとの表示と設定分類の折りたたみ、発生位置の保持を検証する。
/// </summary>
template<class Effect>
void TestEffect(Effect& effect)
{
    const Math::Vector3 position = effect.GetEffectPosition(); // UI描画だけで変化してはいけない発生位置
    for (float width : { 260.0f, 420.0f, 800.0f }) { // 検証するパネル幅
        DrawFrame(effect, width, true);
        const float expandedHeight = DrawFrame(effect, width, true, true); // 全項目展開時の高さ
        DrawFrame(effect, width, false);
        const float collapsedHeight = DrawFrame(effect, width, false, true); // 全項目折りたたみ時の高さ
        Require(collapsedHeight < expandedHeight, "Effect sections did not collapse");
        DrawFrame(effect, width, true);
        Require(DrawFrame(effect, width, true, true) > collapsedHeight, "Effect sections did not reopen");
    }
    const Math::Vector3 after = effect.GetEffectPosition(); // 検証後の発生位置
    Require(after.x == position.x && after.y == position.y && after.z == position.z, "Editor changed effect position without input");
}
/// <summary>
/// GPU描画を行わず、各演出の開始・終了・再起動を検証する。
/// </summary>
void TestEffectPlayback()
{
    MyEngine::PostProcess postProcess; // 演出が操作する未初期化のポストプロセス設定
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects; // 描画対象を持たない検証用オブジェクト一覧
    TemporalRiftEffect rift; // 時空破砕の進行検証対象
    TimeStopEffect stop; // 時間停止の進行検証対象
    TimeReversalEffect reversal; // 時間逆行の進行検証対象
    constexpr float deltaTime = 1.0f / 60.0f; // 演出検証の更新間隔
    constexpr int maximumFrames = 3600; // 終了しない演出を検出する更新上限
    for (int playback = 0; playback < 2; ++playback) { // 終了後の再起動も検証する再生回数
        rift.Start(postProcess, objects, { 0.5f, 0.5f });
        Require(rift.IsPlaying(), "Temporal rift did not start");
        for (int frame = 0; frame < maximumFrames && rift.IsPlaying(); ++frame) { // 終了までの更新回数
            rift.Update(deltaTime, postProcess, nullptr);
            rift.UpdateImpact(deltaTime, nullptr);
        }
        Require(!rift.IsPlaying(), "Temporal rift did not finish");

        stop.Start(postProcess, { 0.5f, 0.5f });
        Require(stop.IsPlaying(), "Time stop did not start");
        for (int frame = 0; frame < maximumFrames && stop.IsPlaying(); ++frame) { // 終了までの更新回数
            stop.Update(deltaTime, postProcess);
        }
        Require(!stop.IsPlaying(), "Time stop did not finish");

        reversal.Start(postProcess, 16);
        Require(reversal.IsPlaying(), "Time reversal did not start");
        for (int frame = 0; frame < maximumFrames && reversal.IsPlaying(); ++frame) { // 終了までの更新回数
            reversal.Update(deltaTime, postProcess, objects);
        }
        Require(!reversal.IsPlaying(), "Time reversal did not finish");
    }
}
} // namespace

/// <summary>
/// GPUを初期化せず、3種類の本番エフェクト編集UIを検証する。
/// </summary>
int main()
{
    try {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); // ヘッドレス検証用の入出力設定
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1200.0f, 3000.0f);
        io.DeltaTime = 1.0f / 60.0f;
        Require(MyEngine::ImGuiFontUtility::LoadJapaneseFont(io, "project/resources/fonts/FiraMono-Regular.ttf"), "Editor fonts could not be loaded");
        Require(io.FontDefault->Sources.Size == 2, "Japanese font was not merged into Fira Mono");
        Require(std::string(io.FontDefault->GetDebugName()).find("FiraMono-Regular") != std::string::npos, "Fira Mono is not the primary font");
        unsigned char* pixels = nullptr; // フォント構築に使う画素参照
        int width = 0; // フォント画像の幅
        int height = 0; // フォント画像の高さ
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        Require(io.FontDefault->GetFontBaked(16.0f)->FindGlyphNoFallback(0x6642) != nullptr, "Japanese glyph is missing");
        TemporalRiftEffect rift; // 時空破砕の検証対象
        TimeStopEffect stop; // 時間停止の検証対象
        TimeReversalEffect reversal; // 時間逆行の検証対象
        TestEffect(rift);
        TestEffect(stop);
        TestEffect(reversal);
        TestEffectPlayback();
        ImGui::DestroyContext();
        std::cout << "Effect editor tests passed: 3 effects, folding and layout at 260/420/800 px, playback and restart\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
