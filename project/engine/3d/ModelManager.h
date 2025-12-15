#pragma once

#include <memory>
#include <string>
#include <vector>

namespace MyEngine {

// 前方宣言
class Model;
class ModelCommon;

class ModelManager {
public: // メンバ関数
    static ModelManager* GetInstance();
    void Finalize();

    // モデルの読み込み
    Model* LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon);

private: // メンバ変数
    ModelManager() = default;
    ~ModelManager() = default;

    static ModelManager* instance_;
    std::vector<std::unique_ptr<Model>> models_;
};

} // namespace MyEngine
