#include "ResourceResolver.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

using namespace MyEngine;
namespace fs = std::filesystem;

// グローバルなリゾルバーデータ構造体
struct ResolverData {
    std::vector<std::pair<std::string, std::string>> searchPaths; // name, path
    std::unordered_map<ResourceResolver::Type, std::vector<std::string>> extMap;
};

// グローバルなリゾルバーデータのインスタンス
static ResolverData g_resolverData;

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
}

/// <summary>
/// 登録された検索パスをすべてクリア
/// </summary>
void ResourceResolver::ClearSearchPaths()
{
    // 登録された検索パスのリストをクリアして、すべての検索パスを削除
    g_resolverData.searchPaths.clear();
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
    try {
        // パスを正規化するために、std::filesystem を使用してパスを処理する
        fs::path p(path);
        // パスが存在する場合は、正規化されたパスを返す
        if (fs::exists(p)) {
            return fs::canonical(p).string();
        }

        // パスが存在しない場合は、lexically_normal を使用してパスを正規化し、元の文字列を返す
        return p.lexically_normal().string();

    } catch (...) {
        return path;
    }
}

/// <summary>
/// 指定された基準ディレクトリに対して相対的に入力パスを解決させる。モデル内部の相対パスに便利
/// </summary>
static std::string tryCandidate(const fs::path& candidate)
{
    try {
        // 候補が実行フォルダ内にある場合は、日本語を含む絶対パスへ変換せず相対パスで返す
        if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
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
                if (relativePathText != ".." && !relativePathText.starts_with("../")) {
                    return relativePathText;
                }
            }

            return fs::canonical(candidate).string();
        }
    } catch (...) {
    }

    // 候補のパスが存在しない場合や、通常のファイルでない場合は、空文字列を返す
    return std::string();
}

/// <summary>
/// 指定された基準ディレクトリに対して相対的に入力パスを解決させる。モデル内部の相対パスに便利
/// </summary>
std::string ResourceResolver::ResolveRelative(const std::string& inputPath, const std::string& baseDir, Type type)
{

    try {
        // 入力パスが空の場合は、空文字列を返す
        fs::path p(inputPath);
        if (p.is_absolute()) {
            auto r = tryCandidate(p);
            if (!r.empty())
                return r;
        }

        // 基準ディレクトリに対して相対的に解決を試みる
        fs::path base(baseDir);
        fs::path candidate = base / p;
        // 拡張子がない場合は、リソースの種類に応じた拡張子のリストを試す
        if (!p.has_extension()) {
            const auto& exts = GetExtList(type);

            // 候補のパスに対して、リソースの種類に応じた拡張子のリストを試す
            for (const auto& e : exts) {
                fs::path c2 = candidate;
                c2 += e;
                auto r = tryCandidate(c2);
                if (!r.empty())
                    return r;
            }

        } else {
            auto r = tryCandidate(candidate);
            if (!r.empty())
                return r;
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
    // 登録された検索パスが空の場合は、デフォルトの検索パスを設定する
    if (g_resolverData.searchPaths.empty()) {
        try {
            g_resolverData.searchPaths.emplace_back("resources", "resources");
            g_resolverData.searchPaths.emplace_back("resources_textures", "resources/textures");
            g_resolverData.searchPaths.emplace_back("resources_models", "resources/models");
            g_resolverData.searchPaths.emplace_back("models", "models");
            g_resolverData.searchPaths.emplace_back("shaders", "resources/shaders");
        } catch (...) {
        }
    }

    try {
        if (inputPath.empty())
            return std::string();
        fs::path p(inputPath);

        if (p.is_absolute()) {
            auto r = tryCandidate(p);
            if (!r.empty())
                return r;
        }

        if (p.has_parent_path()) {

            auto r = tryCandidate(p);
            if (!r.empty())
                return r;
        }

        const auto& exts = GetExtList(type);
        for (const auto& sp : g_resolverData.searchPaths) {
            fs::path root = sp.second;
            fs::path candidate = root / p;
            if (!p.has_extension()) {
                for (const auto& e : exts) {
                    fs::path c2 = candidate;
                    c2 += e;
                    auto r = tryCandidate(c2);
                    if (!r.empty())
                        return r;
                }
            } else {
                auto r = tryCandidate(candidate);
                if (!r.empty())
                    return r;
            }
        }

        if (!p.has_parent_path()) {
            std::vector<fs::path> candidateFilenames; // 再帰探索で比較する候補ファイル名
            if (!p.has_extension()) {
                for (const auto& e : exts) {
                    fs::path candidateFilename = p; // 拡張子を補完した候補ファイル名
                    candidateFilename += e;
                    candidateFilenames.push_back(candidateFilename.filename());
                }
            } else {
                candidateFilenames.push_back(p.filename());
            }

            constexpr size_t kMaxRecursiveCheckCount = 4096; // フォールバック探索で確認する通常ファイル数の上限
            size_t checkedFileCount = 0; // 再帰探索で確認済みの通常ファイル数
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
                            auto r = tryCandidate(entry.path());
                            if (!r.empty())
                                return r;
                        }
                    }
                }

                iterator.increment(iteratorError);
            }
        }

    } catch (...) {
    }

    return std::string();
}
