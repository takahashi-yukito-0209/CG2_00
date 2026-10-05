#pragma once

#include <string>
#include <vector>

/// <summary>
/// 一般的なファイル操作をまとめたユーティリティ名前空間。
/// </summary>
namespace FileUtility {

/// <summary>
/// 指定したパスが存在するかを確認する。
/// </summary>
bool Exists(const std::string& path);

/// <summary>
/// 指定したパスが通常ファイルとして存在するかを確認する。
/// </summary>
bool IsRegularFile(const std::string& path);

/// <summary>
/// 指定したパスがディレクトリとして存在するかを確認する。
/// </summary>
bool IsDirectory(const std::string& path);

/// <summary>
/// テキストファイルを読み込む。失敗した場合は false を返す。
/// </summary>
bool TryReadText(const std::string& path, std::string& outText);

/// <summary>
/// テキストファイルを読み込む。失敗した場合は空文字を返す。
/// </summary>
std::string ReadText(const std::string& path);

/// <summary>
/// 同じフォルダーの一時ファイルへ書き込み、成功後に保存先を置き換える。
/// 親ディレクトリは必要に応じて作成し、失敗理由を任意で返す。
/// </summary>
bool WriteText(const std::string& path, const std::string& text, std::string* outError = nullptr);

/// <summary>
/// 指定したディレクトリが存在しない場合に作成する。
/// </summary>
bool CreateDirectoryIfNeeded(const std::string& directoryPath);

/// <summary>
/// ファイル名を取得する。
/// </summary>
std::string GetFileName(const std::string& path);

/// <summary>
/// 拡張子を取得する。
/// </summary>
std::string GetExtension(const std::string& path);

/// <summary>
/// 親ディレクトリのパスを取得する。
/// </summary>
std::string GetParentDirectory(const std::string& path);

/// <summary>
/// パス表記を正規化する。
/// </summary>
std::string NormalizePath(const std::string& path);
/// <summary>
/// 指定ディレクトリ内の通常ファイル一覧を取得する。extension が空でない場合は拡張子で絞り込む。
/// </summary>
std::vector<std::string> ListFiles(const std::string& directoryPath, const std::string& extension = "");

/// <summary>
/// 指定ファイルを削除する。
/// </summary>
bool RemoveFile(const std::string& path);

/// <summary>
/// 拡張子を除いたファイル名を取得する。
/// </summary>
std::string GetStem(const std::string& path);

/// <summary>
/// 2つのパスを結合する。
/// </summary>
std::string JoinPath(const std::string& basePath, const std::string& relativePath);

/// <summary>
/// lhs の更新日時が rhs より新しいか判定する。
/// </summary>
bool IsNewerThan(const std::string& lhs, const std::string& rhs);

} // namespace FileUtility
