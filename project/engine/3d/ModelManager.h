#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MyEngine {

// 前方宣言
class Model;
class ModelCommon;

/// <summary>
/// モデルマネージャクラス
/// </summary>
class ModelManager {
public: // メンバ関数

    /// <summary>
    /// シングルトンインスタンス取得
    /// </summary>
    static ModelManager* GetInstance();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// モデルの読み込みと取得
    /// </summary>
    Model* LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon);

    /// <summary>
    /// ファイルパスに対応するモデルを検索して返す（見つからない場合は nullptr を返す）
    /// </summary>
    Model* FindModel(const std::string& filePath);

private: // メンバ関数（内部用）
    // デフォルトコンストラクタとデストラクタは private にして外部からのインスタンス化と破棄を防止
    ModelManager() = default;
    ~ModelManager() = default;

private: // メンバ変数

    static ModelManager* instance_;
    // ファイルパスをキーにモデルインスタンスを保持するコンテナ
    std::map<std::string, std::unique_ptr<Model>> models_;
};

} // namespace MyEngine
