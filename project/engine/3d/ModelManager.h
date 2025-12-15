#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

namespace MyEngine {

// 前方宣言
class Model;
class ModelCommon;

class ModelManager {
public: // メンバ関数
    static ModelManager* GetInstance();
    void Finalize();

    // モデルの読み込み
    // ディレクトリとファイル名を指定してモデルを読み込む
    Model* LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon);

    // ファイルパスをキーにして格納済みモデルを検索して返す
    Model* FindModel(const std::string& filePath);

private: // メンバ変数
    ModelManager() = default;
    ~ModelManager() = default;

    static ModelManager* instance_;
    // ファイルパスをキーにモデルインスタンスを保持するコンテナ
    std::map<std::string, std::unique_ptr<Model>> models_;
};

} // namespace MyEngine
