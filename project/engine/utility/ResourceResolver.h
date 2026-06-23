#pragma once
#include <string>
#include <vector>

namespace MyEngine {
/// <summary>
/// リソースの種類（テクスチャ、モデル、シェーダー、サウンドなど）に応じて
/// 登録された検索パスから存在するファイルを探すためのクラス
/// </summary>
class ResourceResolver {
public: // メンバ関数
    // リソースの種類を表す列挙型
    enum class Type {
        Any = 0,
        Texture,
        Model,
        Shader,
        Sound
    };

    /// <summary>
    /// 実行ファイルが配置されているフォルダを作業フォルダに設定する
    /// </summary>
    static bool SetWorkingDirectoryToExecutable();

    /// <summary>
    /// 指定された名前とパスをリソースの検索パスとして登録
    /// </summary>
    static void RegisterSearchPath(const std::string& name, const std::string& path);

    /// <summary>
    /// 登録された検索パスをすべてクリア
    /// </summary>
    static void ClearSearchPaths();

    /// <summary>
    /// 入力パスを既存のファイルシステムパスに解決させる。見つからない場合は空文字列を返す
    /// </summary>
    static std::string Resolve(const std::string& inputPath, Type type = Type::Any);

    /// <summary>
    /// 指定された基準ディレクトリに対して相対的に入力パスを解決させる。モデル内部の相対パスに便利
    /// </summary>
    static std::string ResolveRelative(const std::string& inputPath, const std::string& baseDir, Type type = Type::Any);

    /// <summary>
    /// パスを正規化させる。存在する場合は正規化されたパスを返し、存在しない場合は元の文字列を返す
    /// </summary>
    static std::string Normalize(const std::string& path);
};

} // namespace MyEngine
