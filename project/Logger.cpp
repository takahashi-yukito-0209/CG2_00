#include "Logger.h"
#include <Windows.h>

namespace Logger {
void Log(const std::string& message)
{
    // デバッグ出力ウィンドウにメッセージを出力
    OutputDebugStringA(message.c_str());
}
}