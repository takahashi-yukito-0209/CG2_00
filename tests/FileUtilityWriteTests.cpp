#include "engine/utility/FileUtility.h"
#define NOMINMAX
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
/// <summary>
/// 条件が成立しない場合にテストを失敗させる。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// テストで新規作成した専用フォルダーだけを終了時に削除する。
/// </summary>
struct TestDirectory {
    fs::path path; // 既存リソースから分離したテスト専用フォルダー
    bool owned = false; // このテストが作成に成功したか

    /// <summary>
    /// このテストが作成したフォルダーの内容を後片付けする。
    /// </summary>
    ~TestDirectory()
    {
        if (owned) {
            std::error_code error; // 後片付けの失敗は例外にしない
            fs::remove_all(path, error);
        }
    }
};

/// <summary>
/// ロック検証で取得したファイルハンドルを解放する。
/// </summary>
struct TestHandle {
    HANDLE value = INVALID_HANDLE_VALUE; // ロック対象のファイルハンドル

    /// <summary>
    /// テスト失敗時もファイルロックを解除する。
    /// </summary>
    ~TestHandle()
    {
        if (value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }
};

/// <summary>
/// 保存結果を改行変換せずバイト単位で取得する。
/// </summary>
std::string ReadBytes(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary); // 保存結果の読み取りストリーム
    Check(file.is_open(), "Failed to read test file");
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

/// <summary>
/// 保存後にこの処理の一時ファイルが残っていないことを確認する。
/// </summary>
void CheckNoTemporaryFiles(const fs::path& directory, const fs::path& collision)
{
    for (const auto& entry : fs::recursive_directory_iterator(directory)) { // テスト専用フォルダー内のファイル
        Check(entry.path() == collision || entry.path().filename().string().find(".tmp.") == std::string::npos,
            "Temporary output was not cleaned up");
    }
}
}

/// <summary>
/// 保存先の置き換え、改行互換性、保存失敗時の元ファイル保持を確認する。
/// </summary>
int main()
{
    try {
        TestDirectory directory; // 実リソースを書き換えない検証領域
        directory.path = fs::absolute("generated/tests") / ("safe-save-" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(directory.path.parent_path());
        Check(fs::create_directory(directory.path), "Test directory already exists");
        directory.owned = true;
        const fs::path target = directory.path / "stage.json"; // 置き換え対象
        const fs::path collision = target.string() + ".tmp." + std::to_string(GetCurrentProcessId()) + ".0"; // 意図的に占有する一時名
        {
            std::ofstream file(collision, std::ios::binary); // 別処理の一時ファイルを模したデータ
            file << "do not touch";
            Check(file.good(), "Failed to create collision fixture");
        }
        std::string error = "old error"; // 保存処理から返される失敗理由
        const std::string text = "first\nsecond\r\n"; // 従来のテキストモードと比較する内容
        Check(FileUtility::WriteText(target.string(), text, &error) && error.empty(), "New file save failed");
        const fs::path legacy = directory.path / "legacy.txt"; // 従来のテキストモードの比較基準
        {
            std::ofstream file(legacy); // 従来と同じ書き込みモード
            file << text;
        }
        Check(ReadBytes(target) == ReadBytes(legacy), "Text mode newline behavior changed");
        Check(ReadBytes(collision) == "do not touch", "Existing temporary file was overwritten");
        CheckNoTemporaryFiles(directory.path, collision);

        const std::string large(2 * 1024 * 1024 + 17, 'x'); // 複数チャンクの書き込みデータ
        Check(FileUtility::WriteText(target.string(), large, &error), "Large replacement failed");
        Check(ReadBytes(target) == large, "Large output was incomplete");
        Check(FileUtility::WriteText(target.string(), "", &error) && ReadBytes(target).empty(), "Empty replacement failed");
        Check(FileUtility::WriteText(target.string(), "keep original", &error), "Original fixture save failed");

        Check(SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "Readonly setup failed");
        const bool readonlyResult = FileUtility::WriteText(target.string(), "bad replacement", &error); // 読み取り専用ファイルへの保存結果
        SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_NORMAL);
        Check(!readonlyResult && !error.empty() && ReadBytes(target) == "keep original", "Readonly failure damaged original");
        CheckNoTemporaryFiles(directory.path, collision);
        {
            TestHandle lock; // 削除共有を許可しない外部読者
            lock.value = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(lock.value != INVALID_HANDLE_VALUE, "Lock setup failed");
            Check(!FileUtility::WriteText(target.string(), "bad replacement", &error) && !error.empty(), "Locked replacement succeeded");
            Check(ReadBytes(target) == "keep original", "Locked failure damaged original");
        }
        CheckNoTemporaryFiles(directory.path, collision);
        Check(!FileUtility::WriteText((target / "child.json").string(), "bad", &error) && !error.empty(), "Invalid parent succeeded");
        Check(ReadBytes(target) == "keep original", "Parent failure damaged original");
        Check(!FileUtility::WriteText(directory.path.string(), "bad", &error), "Directory replacement succeeded");
        CheckNoTemporaryFiles(directory.path, collision);
        Check(!FileUtility::WriteText("", "bad", &error) && !error.empty(), "Empty path succeeded");
        const fs::path nested = directory.path / "new" / "stage.json"; // 新規親フォルダーの作成対象
        Check(FileUtility::WriteText(nested.string(), "nested", &error) && error.empty() && ReadBytes(nested) == "nested",
            "Nested save failed");
        Check(FileUtility::WriteText(target.string(), "after failures") && ReadBytes(target) == "after failures",
            "Save did not recover after failures");
        CheckNoTemporaryFiles(directory.path, collision);
        std::cout << "FileUtility safe write tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
