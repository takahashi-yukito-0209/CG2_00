#include "FileUtility.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace FileUtility {

/// <summary>
/// 指定したパスが存在するかを確認する。
/// </summary>
bool Exists(const std::string& path)
{
    std::error_code error; // filesystem API のエラー受け取り
    return fs::exists(fs::path(path), error);
}

/// <summary>
/// 指定したパスが通常ファイルとして存在するかを確認する。
/// </summary>
bool IsRegularFile(const std::string& path)
{
    std::error_code error; // filesystem API のエラー受け取り
    return fs::is_regular_file(fs::path(path), error);
}

/// <summary>
/// 指定したパスがディレクトリとして存在するかを確認する。
/// </summary>
bool IsDirectory(const std::string& path)
{
    std::error_code error; // filesystem API のエラー受け取り
    return fs::is_directory(fs::path(path), error);
}

/// <summary>
/// テキストファイルを読み込む。失敗した場合は false を返す。
/// </summary>
bool TryReadText(const std::string& path, std::string& outText)
{
    outText.clear();

    std::ifstream file(path); // 読み込み対象ファイル
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream stream; // ファイル内容を受け取る文字列ストリーム
    stream << file.rdbuf();
    outText = stream.str();
    return true;
}

/// <summary>
/// テキストファイルを読み込む。失敗した場合は空文字を返す。
/// </summary>
std::string ReadText(const std::string& path)
{
    std::string text; // 読み込み結果
    if (!TryReadText(path, text)) {
        return std::string();
    }
    return text;
}

/// <summary>
/// テキストファイルへ書き込む。必要に応じて親ディレクトリを作成する。
/// </summary>
bool WriteText(const std::string& path, const std::string& text)
{
    const fs::path outputPath(path); // 書き込み先パス
    const fs::path parentDirectory = outputPath.parent_path(); // 書き込み先の親ディレクトリ

    if (!parentDirectory.empty() && !CreateDirectoryIfNeeded(parentDirectory.generic_string())) {
        return false;
    }

    std::ofstream file(path); // 書き込み対象ファイル
    if (!file.is_open()) {
        return false;
    }

    file << text;
    return file.good();
}

/// <summary>
/// 指定したディレクトリが存在しない場合に作成する。
/// </summary>
bool CreateDirectoryIfNeeded(const std::string& directoryPath)
{
    if (directoryPath.empty()) {
        return false;
    }

    std::error_code error; // filesystem API のエラー受け取り
    const fs::path directory(directoryPath); // 作成対象ディレクトリ

    if (fs::exists(directory, error)) {
        return fs::is_directory(directory, error);
    }

    return fs::create_directories(directory, error) && !error;
}

/// <summary>
/// ファイル名を取得する。
/// </summary>
std::string GetFileName(const std::string& path)
{
    return fs::path(path).filename().generic_string();
}

/// <summary>
/// 拡張子を取得する。
/// </summary>
std::string GetExtension(const std::string& path)
{
    return fs::path(path).extension().generic_string();
}

/// <summary>
/// 親ディレクトリのパスを取得する。
/// </summary>
std::string GetParentDirectory(const std::string& path)
{
    return fs::path(path).parent_path().generic_string();
}

/// <summary>
/// パス表記を正規化する。
/// </summary>
std::string NormalizePath(const std::string& path)
{
    if (path.empty()) {
        return std::string();
    }

    std::error_code error; // filesystem API のエラー受け取り
    const fs::path inputPath(path); // 正規化対象パス

    if (fs::exists(inputPath, error)) {
        const fs::path canonicalPath = fs::weakly_canonical(inputPath, error); // 存在するパスの正規化結果
        if (!error) {
            return canonicalPath.generic_string();
        }
    }

    return inputPath.lexically_normal().generic_string();
}


/// <summary>
/// 指定ディレクトリ内の通常ファイル一覧を取得する。extension が空でない場合は拡張子で絞り込む。
/// </summary>
std::vector<std::string> ListFiles(const std::string& directoryPath, const std::string& extension)
{
    std::vector<std::string> files; // 取得したファイルパス一覧
    std::error_code error; // filesystem API のエラー受け取り
    const fs::path directory(directoryPath); // 検索対象ディレクトリ

    if (!fs::exists(directory, error) || !fs::is_directory(directory, error)) {
        return files;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(directory, error)) {
        if (error) {
            break;
        }

        std::error_code entryError; // ファイル種別確認のエラー受け取り
        if (!entry.is_regular_file(entryError)) {
            continue;
        }

        const fs::path filePath = entry.path().lexically_normal(); // 正規化した候補パス
        if (!extension.empty() && filePath.extension().generic_string() != extension) {
            continue;
        }

        files.push_back(filePath.generic_string());
    }

    return files;
}

/// <summary>
/// 指定ファイルを削除する。
/// </summary>
bool RemoveFile(const std::string& path)
{
    std::error_code error; // filesystem API のエラー受け取り
    return fs::remove(fs::path(path), error) && !error;
}

/// <summary>
/// 拡張子を除いたファイル名を取得する。
/// </summary>
std::string GetStem(const std::string& path)
{
    return fs::path(path).stem().generic_string();
}

/// <summary>
/// 2つのパスを結合する。
/// </summary>
std::string JoinPath(const std::string& basePath, const std::string& relativePath)
{
    return (fs::path(basePath) / fs::path(relativePath)).lexically_normal().generic_string();
}

/// <summary>
/// lhs の更新日時が rhs より新しいか判定する。
/// </summary>
bool IsNewerThan(const std::string& lhs, const std::string& rhs)
{
    std::error_code lhsError; // lhs の更新日時取得エラー
    std::error_code rhsError; // rhs の更新日時取得エラー
    const auto lhsWriteTime = fs::last_write_time(fs::path(lhs), lhsError);
    const auto rhsWriteTime = fs::last_write_time(fs::path(rhs), rhsError);

    return !lhsError && (rhsError || lhsWriteTime > rhsWriteTime);
}

} // namespace FileUtility