#include "StringUtility.h"
#include <Windows.h> 
#include <stdexcept>
#include <string>

namespace StringUtility {

std::wstring ConvertString(const std::string& str)
{
    int requiredLength = MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.length(),
        nullptr, 0);

    if (requiredLength == 0) {
        return L"";
    }

    std::wstring wstr(requiredLength, 0);

    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.length(),
        wstr.data(), requiredLength);

    return wstr;
}


std::string ConvertString(const std::wstring& wstr)
{
    if (wstr.empty())
        return "";

    int requiredLength = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (requiredLength == 0) {
        return "";
    }

    std::string str(requiredLength, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        str.data(),
        requiredLength,
        nullptr,
        nullptr);

    return str; // ← 不要な resize は絶対にしない
}
} // namespace StringUtility