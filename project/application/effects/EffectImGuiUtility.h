#pragma once

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include <cfloat>
#endif

namespace EffectImGuiUtility {
#ifdef USE_IMGUI
/// <summary>
/// 項目名を入力欄の上へ配置し、エフェクト設定をパネル幅に合わせる。
/// </summary>
inline void DrawPropertyLabel(const char* label)
{
    ImGui::TextWrapped("%s", label);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

/// <summary>
/// 項目名をIDとして、実数の調整欄を表示する。
/// </summary>
inline void DrawFloat(const char* label, float* value, float step, float minimum, float maximum, const char* format = "%.3f")
{
    ImGui::PushID(label);
    DrawPropertyLabel(label);
    ImGui::DragFloat("##Value", value, step, minimum, maximum, format);
    ImGui::PopID();
}

/// <summary>
/// 整数の範囲選択をパネル幅に合わせて表示する。
/// </summary>
inline void DrawIntSlider(const char* label, int* value, int minimum, int maximum)
{
    ImGui::PushID(label);
    DrawPropertyLabel(label);
    ImGui::SliderInt("##Value", value, minimum, maximum);
    ImGui::PopID();
}

/// <summary>
/// 実数の範囲選択をパネル幅に合わせて表示する。
/// </summary>
inline void DrawFloatSlider(const char* label, float* value, float minimum, float maximum, const char* format)
{
    ImGui::PushID(label);
    DrawPropertyLabel(label);
    ImGui::SliderFloat("##Value", value, minimum, maximum, format);
    ImGui::PopID();
}

/// <summary>
/// 3成分の実数調整欄をパネル幅に合わせて表示する。
/// </summary>
inline void DrawVector3(const char* label, float* value, float step)
{
    ImGui::PushID(label);
    DrawPropertyLabel(label);
    ImGui::DragFloat3("##Value", value, step);
    ImGui::PopID();
}

/// <summary>
/// RGBまたはRGBAの色編集欄をパネル幅に合わせて表示する。
/// </summary>
inline void DrawColor(const char* label, float* value, bool hasAlpha = true)
{
    ImGui::PushID(label);
    DrawPropertyLabel(label);
    if (hasAlpha) {
        ImGui::ColorEdit4("##Value", value);
    } else {
        ImGui::ColorEdit3("##Value", value);
    }
    ImGui::PopID();
}

/// <summary>
/// 設定の初期化を確認し、実行が選ばれたフレームだけtrueを返す。
/// </summary>
inline bool DrawResetSettingsButton()
{
    bool confirmed = false; // 初期化が確認されたか
    if (ImGui::Button("Reset Settings", ImVec2(-FLT_MIN, 0.0f))) {
        ImGui::OpenPopup("Reset Settings?");
    }
    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Reset Settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Restore default settings?");
        if (ImGui::Button("Reset", ImVec2(-FLT_MIN, 0.0f))) {
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel", ImVec2(-FLT_MIN, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return confirmed;
}
#endif
} // namespace EffectImGuiUtility
