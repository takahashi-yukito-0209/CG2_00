#pragma once

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "engine/utility/StringUtility.h"
#include "engine/utility/ResourceResolver.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <string>

namespace MyEngine::ImGuiFontUtility {
/// <summary>
/// Fira MonoへWindowsの日本語フォントを合成し、本番UIとテストの表示条件を揃える。
/// </summary>
inline bool LoadJapaneseFont(ImGuiIO& io, const std::string& latinFontFile = "fonts/FiraMono-Regular.ttf")
{
    wchar_t windowsDirectory[MAX_PATH] {}; // Windowsのインストール先
    const UINT length = GetWindowsDirectoryW(windowsDirectory, MAX_PATH); // 取得したパスの文字数
    if (!io.Fonts || length == 0 || length >= MAX_PATH) {
        return false;
    }
    const std::filesystem::path fontPath = std::filesystem::path(windowsDirectory) / "Fonts" / "msgothic.ttc"; // OSに付属する日本語フォント
    std::error_code error; // フォントの存在確認時の失敗理由
    if (!std::filesystem::is_regular_file(fontPath, error)) {
        return false;
    }
    const std::string utf8Path = StringUtility::ConvertString(fontPath.wstring()); // ImGuiへ渡すUTF-8パス
    if (utf8Path.empty()) {
        return false;
    }
    const std::string latinFontPath = ResourceResolver::Resolve(latinFontFile); // 既存の検索規則で解決した英数字フォント
    if (latinFontPath.empty()) {
        return false;
    }
    ImFont* font = io.Fonts->AddFontFromFileTTF(latinFontPath.c_str(), 16.0f); // 英数字・記号の表示を担当する基本フォント
    if (!font) {
        return false;
    }
    static constexpr ImWchar excludedLatinRanges[] = { 0x0020, 0x00FF, 0 }; // 日本語側で上書きしない英数字・記号の範囲
    ImFontConfig japaneseConfig; // 日本語の合成方法
    japaneseConfig.MergeMode = true;
    japaneseConfig.GlyphExcludeRanges = excludedLatinRanges;
    if (!io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), 16.0f, &japaneseConfig,
            io.Fonts->GetGlyphRangesChineseFull())) {
        return false;
    }
    io.FontDefault = font;
    return true;
}
} // namespace MyEngine::ImGuiFontUtility
#endif
