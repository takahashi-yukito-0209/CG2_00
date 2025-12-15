#include "ModelManager.h"
#include "Model.h"

using namespace MyEngine;

ModelManager* ModelManager::instance_ = nullptr;

ModelManager* ModelManager::GetInstance()
{
    if (!instance_) instance_ = new ModelManager();
    return instance_;
}

void ModelManager::Finalize()
{
    // すべてのモデルを解放
    models_.clear();
    // シングルトン自身を破棄
    delete instance_;
    instance_ = nullptr;
}

Model* ModelManager::LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon)
{
    // ファイルパスをキーとして結合
    std::string filePath = directory + "/" + filename;

    // すでに読み込まれているかチェック（早期リターン）
    if (models_.find(filePath) != models_.end()) {
        // 既にある場合はそのポインタを返す
        return models_[filePath].get();
    }

    // 新規モデルを生成して読み込む
    auto model = std::make_unique<Model>();
    if (model->LoadFromFile(directory, filename)) {
        // モデルの初期化
        model->Initialize(modelCommon);
        Model* ptr = model.get();
        // コンテナに格納（所有権を移す）
        models_.insert(std::make_pair(filePath, std::move(model)));
        return ptr;
    }

    // 読み込み失敗時はnullptrを返す
    return nullptr;
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    // コンテナ内を検索し、存在すれば生ポインタを返す
    auto it = models_.find(filePath);
    if (it != models_.end()) {
        return it->second.get();
    }
    return nullptr;
}
