#include "FileUtility.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace FileUtility {
namespace {
/// <summary>
/// この保存処理で作成した一時ファイルとハンドルだけを終了時に解放する。
/// </summary>
struct TemporaryTextFile {
    fs::path path; // 保存先と同じフォルダーの一時パス
    HANDLE handle = INVALID_HANDLE_VALUE; // 排他的に作成した書き込みハンドル
    bool ownsFile = false; // この処理が作成した一時ファイルか

    /// <summary>
    /// 残ったハンドルを閉じ、置き換え前の一時ファイルを削除する。
    /// </summary>
    ~TemporaryTextFile()
    {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
        if (ownsFile) {
            DeleteFileW(path.c_str());
        }
    }
};

/// <summary>
/// 保存失敗の工程とWindowsエラー番号を任意の出力先へ返す。
/// </summary>
bool ReportWriteFailure(std::string* outError, const char* operation, DWORD error)
{
    if (outError) {
        *outError = std::string(operation) + " (Windows error " + std::to_string(error) + ").";
    }
    return false;
}
} // namespace

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
/// 一時ファイルの書き込み・フラッシュ・クローズ後に保存先を置き換える。
/// </summary>
bool WriteText(const std::string& path, const std::string& text, std::string* outError)
{
    if (outError) {
        outError->clear();
    }
    if (path.empty()) {
        return ReportWriteFailure(outError, "Empty output path", ERROR_INVALID_NAME);
    }
    const fs::path outputPath(path); // 書き込み先パス
    const fs::path parentDirectory = outputPath.parent_path(); // 書き込み先の親ディレクトリ

    if (!parentDirectory.empty()) {
        std::error_code directoryError; // 親ディレクトリ作成の実際の失敗理由
        fs::create_directories(parentDirectory, directoryError);
        if (directoryError) {
            if (outError) {
                *outError = "Failed to create parent directory: " + directoryError.message();
            }
            return false;
        }
    }

    std::string outputText; // 従来のWindowsテキストモードと同じ改行へ変換した内容
    outputText.reserve(text.size());
    for (char character : text) { // 書き込む文字
        if (character == '\n') {
            outputText.push_back('\r');
        }
        outputText.push_back(character);
    }

    static std::atomic<uint64_t> nextTemporaryId { 0 }; // 同一プロセス内の一時ファイル識別番号
    TemporaryTextFile temporary; // 成功・失敗のどちらでも残骸を解放する一時ファイル
    for (size_t attempt = 0; attempt < 128; ++attempt) { // 過去の残骸と衝突した場合の再試行回数
        temporary.path = outputPath;
        temporary.path += ".tmp." + std::to_string(GetCurrentProcessId()) + "." +
            std::to_string(nextTemporaryId.fetch_add(1));
        temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (temporary.handle != INVALID_HANDLE_VALUE) {
            temporary.ownsFile = true;
            break;
        }
        const DWORD createError = GetLastError(); // 一時ファイル作成に失敗した理由
        if (createError != ERROR_FILE_EXISTS && createError != ERROR_ALREADY_EXISTS) {
            return ReportWriteFailure(outError, "Failed to create temporary file", createError);
        }
    }
    if (!temporary.ownsFile) {
        return ReportWriteFailure(outError, "Temporary file names are occupied", ERROR_FILE_EXISTS);
    }

    size_t offset = 0; // 一時ファイルへ書き込み済みのバイト数
    while (offset < outputText.size()) {
        const DWORD chunkSize = static_cast<DWORD>((std::min)(outputText.size() - offset, size_t { 1024 * 1024 })); // 1回の書き込み量
        DWORD written = 0; // 実際に書き込まれたバイト数
        if (!WriteFile(temporary.handle, outputText.data() + offset, chunkSize, &written, nullptr)) {
            return ReportWriteFailure(outError, "Failed to write temporary file", GetLastError());
        }
        if (written == 0) {
            return ReportWriteFailure(outError, "Temporary file write made no progress", ERROR_WRITE_FAULT);
        }
        offset += written;
    }
    if (!FlushFileBuffers(temporary.handle)) {
        return ReportWriteFailure(outError, "Failed to flush temporary file", GetLastError());
    }
    if (!CloseHandle(temporary.handle)) {
        return ReportWriteFailure(outError, "Failed to close temporary file", GetLastError());
    }
    temporary.handle = INVALID_HANDLE_VALUE;

    // 同じフォルダー内で置き換える。失敗時も保存先を削除しない。
    if (!MoveFileExW(temporary.path.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return ReportWriteFailure(outError, "Failed to replace output file", GetLastError());
    }
    temporary.ownsFile = false;
    return true;
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
