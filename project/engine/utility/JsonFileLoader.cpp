#include "JsonFileLoader.h"

#include "FileUtility.h"
#include "Logger.h"

#include <exception>

namespace MyEngine {

namespace {
/// <summary>
/// 呼び出し元へJSON読み込み失敗理由を設定する。
/// </summary>
void SetLoadError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}
}

/// <summary>
/// 指定したJSONファイルをnlohmann::jsonとして読み込む。
/// </summary>
bool JsonFileLoader::Load(
    const std::string& filePath,
    JsonDocument& jsonDocument,
    std::string* resolvedPath,
    ResourceResolver::Type resourceType,
    std::string* errorMessage)
{
    jsonDocument = JsonDocument::object();
    SetLoadError(errorMessage, std::string());

    std::string resolvedFilePath = ResourceResolver::Resolve(filePath, resourceType); // 解決済みJSONファイルパス
    if (resolvedFilePath.empty()) {
        resolvedFilePath = filePath;
    }
    if (resolvedPath) {
        *resolvedPath = resolvedFilePath;
    }

    std::string jsonText; // JSONファイル本文
    if (!FileUtility::TryReadText(resolvedFilePath, jsonText)) {
        const std::string message = std::string("Could not open JSON file: ") + resolvedFilePath; // 読み込み失敗理由
        Logger::Warn(std::string("Warning: JsonFileLoader ") + message + "\n");
        SetLoadError(errorMessage, message);
        return false;
    }

    try {
        jsonDocument = JsonDocument::parse(jsonText);
    } catch (const std::exception& exception) {
        const std::string message = std::string("Failed to parse JSON: ") + resolvedFilePath + " " + exception.what(); // パース失敗理由
        Logger::Warn(std::string("Warning: JsonFileLoader ") + message + "\n");
        SetLoadError(errorMessage, message);
        jsonDocument = JsonDocument::object();
        return false;
    }

    return true;
}

} // namespace MyEngine