#pragma once

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include <cfloat>
#include <string>
#endif

namespace MyEngine::ParticleEditorImGuiUtility {
#ifdef USE_IMGUI
/// <summary>
/// 項目名を入力欄の上へ配置し、入力欄をパネル幅に合わせる。
/// </summary>
inline void DrawParticlePropertyLabel(const char* label)
{
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

/// <summary>
/// ImGuiの文字列入力に合わせてバッファを拡張する。
/// </summary>
inline int ResizeParticleTextBuffer(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto& text = *static_cast<std::string*>(data->UserData); // 編集対象の可変長文字列
        text.resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = text.data();
    }
    return 0;
}

/// <summary>
/// 固定長で切り詰めずにパーティクル設定の文字列を編集する。
/// </summary>
inline void DrawParticleTextInput(const char* label, const char* id, std::string& text, bool multiline = false)
{
    DrawParticlePropertyLabel(label);
    if (multiline) {
        ImGui::InputTextMultiline(id, text.data(), text.capacity() + 1,
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.0f),
            ImGuiInputTextFlags_CallbackResize, ResizeParticleTextBuffer, &text);
    } else {
        ImGui::InputText(id, text.data(), text.capacity() + 1,
            ImGuiInputTextFlags_CallbackResize, ResizeParticleTextBuffer, &text);
    }
}
#endif
} // namespace MyEngine::ParticleEditorImGuiUtility
