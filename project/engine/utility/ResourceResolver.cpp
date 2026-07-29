#include "ResourceResolver.h"
#include "FileUtility.h"
#include "StringUtility.h"
#include <Windows.h>
#include <filesystem>
#include <unordered_map>

using namespace MyEngine;
namespace fs = std::filesystem;

// グローバルなリゾルバーデータ構造体
struct ResolverData {
    std::vector<std::pair<std::string, std::string>> searchPaths; // name, path
    std::unordered_map<ResourceResolver::Type, std::vector<std::string>> extMap;
    std::unordered_map<std::string, std::string> resolveCache; // Resolveで見つかったパスのキャッシュ
    std::unordered_map<std::string, std::string> relativeResolveCache; // ResolveRelativeで見つかったパスのキャッシュ
};

// グローバルなリゾルバーデータのインスタンス
static ResolverData g_resolverData;

/// <summary>
/// Resolve用キャッシュのキーを作成する
/// </summary>
static std::string MakeResolveCacheKey(const std::string& inputPath, ResourceResolver::Type type)
{
    const std::string currentDirectory = fs::current_path().generic_string(); // 現在の作業ディレクトリ
    return currentDirectory + "|" + std::to_string(static_cast<int>(type)) + "|" + inputPath;
}

/// <summary>
/// ResolveRelative用キャッシュのキーを作成する
/// </summary>
static std::string MakeRelativeResolveCacheKey(const std::string& inputPath, const std::string& baseDir, ResourceResolver::Type type)
{
    const std::string currentDirectory = fs::current_path().generic_string(); // 現在の作業ディレクトリ
    return currentDirectory + "|" + baseDir + "|" + std::to_string(static_cast<int>(type)) + "|" + inputPath;
}

/// <summary>
/// 実行ファイルが配置されているフォルダを作業フォルダに設定する
/// </summary>
bool ResourceResolver::SetWorkingDirectoryToExecutable()
{
    wchar_t executablePath[MAX_PATH] = {}; // 実行ファイルの絶対パス
    const DWORD pathLength = GetModuleFileNameW(
        nullptr,
        executablePath,
        static_cast<DWORD>(std::size(executablePath))); // 取得したパスの文字数
    if (pathLength == 0 || pathLength >= std::size(executablePath)) {
        return false;
    }

    wchar_t* lastSeparator = wcsrchr(executablePath, L'\\'); // ファイル名直前の区切り位置
    if (!lastSeparator) {
        return false;
    }

    *lastSeparator = L'\0';

    wchar_t shortDirectoryPath[MAX_PATH] = {}; // ANSI APIでも扱える短縮フォルダパス
    const DWORD shortPathLength = GetShortPathNameW(
        executablePath,
        shortDirectoryPath,
        static_cast<DWORD>(std::size(shortDirectoryPath))); // 取得した短縮パスの文字数
    if (shortPathLength > 0 && shortPathLength < std::size(shortDirectoryPath)) {
        return SetCurrentDirectoryW(shortDirectoryPath) != FALSE;
    }

    // 短縮パスを取得できない環境では通常パスを使用する
    return SetCurrentDirectoryW(executablePath) != FALSE;
}

/// <summary>
/// 指定された名前とパスをリソースの検索パスとして登録
/// </summary>
void ResourceResolver::RegisterSearchPath(const std::string& name, const std::string& path)
{
    // 登録された検索パスのリストに、指定された名前とパスのペアを追加
    g_resolverData.searchPaths.emplace_back(name, path);
    ClearCache();
}

/// <summary>
/// 登録された検索パスをすべてクリア
/// </summary>
void ResourceResolver::ClearSearchPaths()
{
    // 登録された検索パスのリストをクリアして、すべての検索パスを削除
    g_resolverData.searchPaths.clear();
    ClearCache();
}

/// <summary>
/// 解決済みリソースパスのキャッシュをクリアする。
/// </summary>
void ResourceResolver::ClearCache()
{
    g_resolverData.resolveCache.clear();
    g_resolverData.relativeResolveCache.clear();
}

/// <summary>
/// 指定されたリソースの種類に対応する拡張子のリストを取得。リストがまだ初期化されていない場合は、デフォルトの拡張子リストを設定する
/// </summary>
static const std::vector<std::string>& GetExtList(ResourceResolver::Type type)
{
    // 拡張子のマップが空の場合は、リソースの種類に応じたデフォルトの拡張子リストを設定する
    if (g_resolverData.extMap.empty()) {
        g_resolverData.extMap[ResourceResolver::Type::Texture] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds" };
        g_resolverData.extMap[ResourceResolver::Type::Model] = { ".obj", ".gltf", ".glb" };
        g_resolverData.extMap[ResourceResolver::Type::Shader] = { ".hlsl", ".fx" };
        g_resolverData.extMap[ResourceResolver::Type::Sound] = { ".wav", ".ogg", ".mp3" };
        g_resolverData.extMap[ResourceResolver::Type::Json] = { ".json" };
        g_resolverData.extMap[ResourceResolver::Type::Any] = { "" };
    }

    // 指定されたリソースの種類に対応する拡張子のリストを取得
    auto it = g_resolverData.extMap.find(type);
    // 指定されたリソースの種類に対応する拡張子のリストが見つかった場合は、そのリストを返す
    if (it != g_resolverData.extMap.end()) {
        return it->second;
    }

    // 指定されたリソースの種類に対応する拡張子のリストが見つからない場合は、Any タイプの拡張子リストを返す
    return g_resolverData.extMap[ResourceResolver::Type::Any];
}

/// <summary>
/// パスを正規化させる。存在する場合は正規化されたパスを返し、存在しない場合は元の文字列を返す
/// </summary>
std::string ResourceResolver::Normalize(const std::string& path)
{
    return FileUtility::NormalizePath(path);
}

/// <summary>
/// 既定のリソース検索パスを必要に応じて登録する。
/// </summary>
static void EnsureDefaultSearchPaths()
{
    if (!g_resolverData.searchPaths.empty()) {
        return;
    }

    try {
        g_resolverData.searchPaths.emplace_back("resources", "resources");
        g_resolverData.searchPaths.emplace_back("resources_textures", "resources/textures");
        g_resolverData.searchPaths.emplace_back("resources_models", "resources/models");
        g_resolverData.searchPaths.emplace_back("models", "models");
        g_resolverData.searchPaths.emplace_back("shaders", "resources/shaders");
        g_resolverData.searchPaths.emplace_back("effects", "resources/effects");
    } catch (...) {
    }
}

/// <summary>
/// 候補パスが通常ファイルとして存在する場合、利用しやすい表記のパスへ解決する。
/// </summary>
static std::string TryResolveCandidate(const fs::path& candidate)
{
    try {
        if (!FileUtility::IsRegularFile(candidate.generic_string())) {
            return std::string();
        }

        if (candidate.is_relative()) {
            return candidate.lexically_normal().generic_string();
        }

        std::error_code relativePathError; // 実行フォルダからの相対化エラー
        const fs::path relativePath = fs::relative(
            candidate,
            fs::current_path(),
            relativePathError); // 実行フォルダを基準にした候補パス
        if (!relativePathError && !relativePath.empty()) {
            const std::string relativePathText = relativePath.generic_string(); // 返却する相対パス
            if (relativePathText != ".." && !StringUtility::StartsWith(relativePathText, "../")) {
                return relativePathText;
            }
        }

        return FileUtility::NormalizePath(candidate.generic_string());
    } catch (...) {
    }

    return std::string();
}

/// <summary>
/// 候補パスをそのまま、またはリソース種別に応じた拡張子を補って解決する。
/// </summary>
static std::string TryResolveCandidateWithExtensions(const fs::path& candidate, ResourceResolver::Type type)
{
    if (candidate.has_extension()) {
        return TryResolveCandidate(candidate);
    }

    const std::vector<std::string>& extensions = GetExtList(type); // 種別に対応する拡張子一覧
    for (const std::string& extension : extensions) {
        fs::path extendedCandidate = candidate; // 拡張子を補った候補パス
        extendedCandidate += extension;

        const std::string resolvedPath = TryResolveCandidate(extendedCandidate); // 解決できた候補パス
        if (!resolvedPath.empty()) {
            return resolvedPath;
        }
    }

    return std::string();
}

/// <summary>
/// 再帰検索で比較するファイル名候補を作成する。
/// </summary>
static std::vector<fs::path> BuildCandidateFilenames(const fs::path& inputPath, ResourceResolver::Type type)
{
    std::vector<fs::path> candidateFilenames; // 再帰検索で比較するファイル名一覧

    if (inputPath.has_extension()) {
        candidateFilenames.push_back(inputPath.filename());
        return candidateFilenames;
    }

    const std::vector<std::string>& extensions = GetExtList(type); // 種別に対応する拡張子一覧
    for (const std::string& extension : extensions) {
        fs::path candidateFilename = inputPath; // 拡張子を補完した候補ファイル名
        candidateFilename += extension;
        candidateFilenames.push_back(candidateFilename.filename());
    }

    return candidateFilenames;
}

/// <summary>
/// ファイル名だけが指定された場合に、実行フォルダ配下から一致するリソースを探す。
/// </summary>
static std::string TryResolveRecursiveByFilename(const fs::path& inputPath, ResourceResolver::Type type)
{
    if (inputPath.has_parent_path()) {
        return std::string();
    }

    const std::vector<fs::path> candidateFilenames = BuildCandidateFilenames(inputPath, type); // 比較対象のファイル名一覧
    constexpr size_t kMaxRecursiveCheckCount = 4096; // 再帰検索で確認する通常ファイル数の上限
    size_t checkedFileCount = 0; // 再帰検索で確認済みの通常ファイル数
    std::error_code iteratorError; // ディレクトリ走査時のエラー保持

    fs::recursive_directory_iterator iterator(
        fs::current_path(),
        fs::directory_options::skip_permission_denied,
        iteratorError);
    fs::recursive_directory_iterator end;

    while (!iteratorError && iterator != end && checkedFileCount < kMaxRecursiveCheckCount) {
        const fs::directory_entry& entry = *iterator; // 現在確認しているディレクトリエントリ
        std::error_code entryError; // エントリ種別確認時のエラー保持
        const bool isRegularFile = entry.is_regular_file(entryError); // 通常ファイルかどうか
        if (!entryError && isRegularFile) {
            ++checkedFileCount;
            const fs::path entryFilename = entry.path().filename(); // 比較対象のファイル名
            for (const fs::path& candidateFilename : candidateFilenames) {
                if (entryFilename == candidateFilename) {
                    const std::string resolvedPath = TryResolveCandidate(entry.path()); // 再帰検索で解決した候補
                    if (!resolvedPath.empty()) {
                        return resolvedPath;
                    }
                }
            }
        }

        iterator.increment(iteratorError);
    }

    return std::string();
}

/// <summary>
/// 指定された基準ディレクトリに対して相対的に入力パスを解決させる。モデル内部の相対パスに便利
/// </summary>
std::string ResourceResolver::ResolveRelative(const std::string& inputPath, const std::string& baseDir, Type type)
{
    try {
        if (inputPath.empty()) {
            return std::string();
        }

        const std::string cacheKey = MakeRelativeResolveCacheKey(inputPath, baseDir, type); // 相対解決用キャッシュキー
        auto cacheIt = g_resolverData.relativeResolveCache.find(cacheKey); // 解決済みパスのキャッシュ位置
        if (cacheIt != g_resolverData.relativeResolveCache.end()) {
            return cacheIt->second;
        }

        auto cacheResolvedPath = [&](const std::string& resolvedPath) -> std::string {
            g_resolverData.relativeResolveCache[cacheKey] = resolvedPath;
            return resolvedPath;
        };

        const fs::path inputFilePath(inputPath); // 解決対象の入力パス
        if (inputFilePath.is_absolute()) {
            const std::string resolvedPath = TryResolveCandidateWithExtensions(inputFilePath, type); // 絶対パスとして解決した結果
            if (!resolvedPath.empty()) {
                return cacheResolvedPath(resolvedPath);
            }
        }

        const fs::path baseDirectory(baseDir); // 相対解決の基準ディレクトリ
        const fs::path relativeCandidate = baseDirectory / inputFilePath; // 基準ディレクトリから見た候補
        const std::string resolvedPath = TryResolveCandidateWithExtensions(relativeCandidate, type); // 拡張子補完を含む解決結果
        if (!resolvedPath.empty()) {
            return cacheResolvedPath(resolvedPath);
        }

    } catch (...) {
    }
    return std::string();
}

/// <summary>
/// 入力パスを既存のファイルシステムパスに解決させる。見つからない場合は空文字列を返す
/// </summary>
std::string ResourceResolver::Resolve(const std::string& inputPath, Type type)
{
    EnsureDefaultSearchPaths();

    try {
        if (inputPath.empty()) {
            return std::string();
        }

        const std::string cacheKey = MakeResolveCacheKey(inputPath, type); // 通常解決用キャッシュキー
        auto cacheIt = g_resolverData.resolveCache.find(cacheKey); // 解決済みパスのキャッシュ位置
        if (cacheIt != g_resolverData.resolveCache.end()) {
            return cacheIt->second;
        }

        auto cacheResolvedPath = [&](const std::string& resolvedPath) -> std::string {
            g_resolverData.resolveCache[cacheKey] = resolvedPath;
            return resolvedPath;
        };

        const fs::path inputFilePath(inputPath); // 解決対象の入力パス

        if (inputFilePath.is_absolute() || inputFilePath.has_parent_path()) {
            const std::string resolvedPath = TryResolveCandidateWithExtensions(inputFilePath, type); // 入力パスを直接解決した結果
            if (!resolvedPath.empty()) {
                return cacheResolvedPath(resolvedPath);
            }
        }

        for (const auto& searchPath : g_resolverData.searchPaths) {
            const fs::path searchRoot = searchPath.second; // 登録済みの検索ルート
            const fs::path candidate = searchRoot / inputFilePath; // 検索ルートから見た候補
            const std::string resolvedPath = TryResolveCandidateWithExtensions(candidate, type); // 拡張子補完を含む解決結果
            if (!resolvedPath.empty()) {
                return cacheResolvedPath(resolvedPath);
            }
        }

        const std::string recursiveResolvedPath = TryResolveRecursiveByFilename(inputFilePath, type); // ファイル名だけでの再帰検索結果
        if (!recursiveResolvedPath.empty()) {
            return cacheResolvedPath(recursiveResolvedPath);
        }

    } catch (...) {
    }

    return std::string();
}