#include "GpuEmitterSettingsUtility.h"

#include "engine/utility/FileUtility.h"
#include "engine/utility/JsonUtility.h"
#include "engine/utility/ResourceResolver.h"

#include <algorithm>
#include <filesystem>

namespace MyEngine::GpuEmitterSettingsUtility {
namespace {

constexpr const char* kDefaultSettingsName = "gpu_particle"; // 空名時に使う既定の設定名
constexpr const char* kSettingsDirectory = "resources/effects"; // GPU Particle設定ファイルの保存フォルダ

/// <summary>
/// GPU Emitter設定ファイルを保存する基準ディレクトリを取得する。
/// </summary>
std::filesystem::path GetSettingsDirectory()
{
    const std::filesystem::path currentPath = std::filesystem::current_path(); // 実行時の作業フォルダ
    const std::filesystem::path projectEffectsPath = (currentPath / "../../../project" / kSettingsDirectory).lexically_normal(); // 生成物フォルダから見た元リソース
    if (FileUtility::Exists(projectEffectsPath.generic_string())) {
        return projectEffectsPath;
    }

    return std::filesystem::path(kSettingsDirectory);
}

/// <summary>
/// 指定ディレクトリ内のGPU Emitter設定JSONを一覧へ追加する。
/// </summary>
void AppendSettingsFiles(const std::string& directory, std::vector<std::string>& files)
{
    const std::vector<std::string> candidateFiles = FileUtility::ListFiles(directory, ".json"); // 指定ディレクトリ内のJSON候補
    for (const std::string& filePath : candidateFiles) {
        const std::string normalizedPath = FileUtility::NormalizePath(filePath); // 候補として保持する正規化パス
        const std::string stemName = FileUtility::GetStem(normalizedPath); // 同名判定用のファイル名
        const auto sameNameIt = std::find_if(files.begin(), files.end(), [&](const std::string& registeredPath) {
            return FileUtility::GetStem(registeredPath) == stemName;
        });

        if (sameNameIt == files.end()) {
            files.push_back(normalizedPath);
            continue;
        }

        if (FileUtility::IsNewerThan(normalizedPath, *sameNameIt)) {
            *sameNameIt = normalizedPath;
        }
    }
}

} // namespace

/// <summary>
/// GPU Emitter設定名をファイル名として使える文字だけに整える。
/// </summary>
std::string SanitizeName(const std::string& name)
{
    std::string sanitized; // JSON保存用の安全なファイル名
    for (char character : name) {
        const bool isAlphabet = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z'); // 英字かどうか
        const bool isNumber = character >= '0' && character <= '9'; // 数字かどうか
        if (isAlphabet || isNumber || character == '_' || character == '-') {
            sanitized.push_back(character);
        }
    }
    return sanitized;
}

/// <summary>
/// GPU Emitter設定ファイルの保存先パスを作成する。
/// </summary>
std::string BuildSettingsPath(const std::string& name)
{
    const std::string sanitizedName = SanitizeName(name); // 入力名を安全化した保存名
    if (sanitizedName.empty()) {
        const std::filesystem::path defaultPath = GetSettingsDirectory() / (std::string(kDefaultSettingsName) + ".json"); // 空名時の既定保存先
        return defaultPath.generic_string();
    }

    const std::filesystem::path settingsPath = GetSettingsDirectory() / (sanitizedName + ".json"); // 入力名から作る保存先
    return settingsPath.generic_string();
}

/// <summary>
/// GPU Emitter設定として読み込めるJSONファイルを収集する。
/// </summary>
std::vector<std::string> CollectSettingsFiles()
{
    std::vector<std::string> files; // ImGuiで選択するJSONファイル一覧
    AppendSettingsFiles(GetSettingsDirectory().generic_string(), files);
    AppendSettingsFiles(kSettingsDirectory, files);

    std::sort(files.begin(), files.end(), [](const std::string& lhs, const std::string& rhs) {
        return FileUtility::GetStem(lhs) < FileUtility::GetStem(rhs);
    });
    return files;
}

/// <summary>
/// 設定名に対応する読み込み対象のJSONパスを取得する。
/// </summary>
std::string ResolveSettingsPath(const std::string& name, const std::vector<std::string>& files)
{
    std::string targetName = SanitizeName(name); // 検索に使う安全化済み設定名
    if (targetName.empty()) {
        targetName = kDefaultSettingsName;
    }

    const auto fileIterator = std::find_if(files.begin(), files.end(), [&](const std::string& filePath) {
        return FileUtility::GetStem(filePath) == targetName;
    });
    if (fileIterator != files.end()) {
        return *fileIterator;
    }

    const std::string resolvedPath = ResourceResolver::Resolve(targetName, ResourceResolver::Type::Json); // Resolverで解決したJSON候補
    if (!resolvedPath.empty()) {
        return resolvedPath;
    }

    return BuildSettingsPath(targetName);
}

/// <summary>
/// プリセット名に対応するGPU Emitter設定ファイルを取得する。
/// </summary>
std::string ResolvePresetPath(const std::string& presetName)
{
    const std::string targetName = SanitizeName(presetName); // 比較に使う安全化済みプリセット名
    const std::vector<std::string> files = CollectSettingsFiles(); // 検索対象のプリセット一覧

    const std::string stemPath = ResolveSettingsPath(targetName, files); // ファイル名一致で解決した候補
    if (FileUtility::Exists(stemPath)) {
        return stemPath;
    }

    for (const std::string& filePath : files) {
        std::string jsonText; // 読み込んだJSON文字列
        if (!FileUtility::TryReadText(filePath, jsonText)) {
            continue;
        }

        std::string effectSection = jsonText; // effectカテゴリの読み取り元
        JsonUtility::ExtractObjectSection(jsonText, "effect", effectSection);

        std::string effectName; // JSON内の表示名
        if (JsonUtility::ExtractString(effectSection, "effectName", effectName)
            && SanitizeName(effectName) == targetName) {
            return filePath;
        }
    }

    return stemPath;
}

} // namespace MyEngine::GpuEmitterSettingsUtility
