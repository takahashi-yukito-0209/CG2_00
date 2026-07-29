#pragma once

#include "ResourceResolver.h"
#include "externals/nlohmann/json.hpp"

#include <string>

namespace MyEngine {

using JsonDocument = nlohmann::json;

/// <summary>
/// JSONファイルの解決、読み込み、パースをまとめて扱う汎用ローダー。
/// </summary>
class JsonFileLoader {
public:
    /// <summary>
    /// 指定したJSONファイルをnlohmann::jsonとして読み込む。
    /// </summary>
    static bool Load(
        const std::string& filePath,
        JsonDocument& jsonDocument,
        std::string* resolvedPath = nullptr,
        ResourceResolver::Type resourceType = ResourceResolver::Type::Json,
        std::string* errorMessage = nullptr);
};

} // namespace MyEngine