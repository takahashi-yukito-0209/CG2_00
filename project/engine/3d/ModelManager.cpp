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
    models_.clear();
    delete instance_;
    instance_ = nullptr;
}

Model* ModelManager::LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon)
{
    // 読み込み済みモデルを検索
    for (auto& m : models_) {
        // naive: compare by first vertex count or filepath isn't stored yet
        // TODO: store filepath in Model
    }

    // モデルの新規読み込み
    auto model = std::make_unique<Model>();
    if (model->LoadFromFile(directory, filename)) {
        model->Initialize(modelCommon);
        Model* ptr = model.get();
        models_.push_back(std::move(model));
        return ptr;
    }

    return nullptr;
}
