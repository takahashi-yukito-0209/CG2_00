#include "ModelManager.h"
#include "Model.h"
#include <filesystem>
#include <vector>
#include "Logger.h"

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
    std::string requestedDir = directory;
    std::string requestedFilename = filename;
    std::string filePath = requestedDir + "/" + requestedFilename;

    // If the file does not exist, try a few alternate locations and a limited search
    if (!std::filesystem::exists(filePath)) {
        // Try alternate common directories
        std::vector<std::string> tryDirs = { requestedDir, requestedDir + "s", "resources", "resource", requestedDir + "/models", "models" };
        bool found = false;
        for (const auto& d : tryDirs) {
            std::string tryPath = d + "/" + requestedFilename;
            if (std::filesystem::exists(tryPath)) {
                requestedDir = d;
                filePath = tryPath;
                found = true;
                break;
            }
        }

        // If still not found, perform a short recursive search from project root (current path)
        if (!found) {
            const int maxSearchResults = 4;
            int foundCount = 0;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().filename() == requestedFilename) {
                    requestedDir = entry.path().parent_path().string();
                    requestedFilename = entry.path().filename().string();
                    filePath = entry.path().string();
                    found = true;
                    ++foundCount;
                    break; // take first match
                }
                if (foundCount >= maxSearchResults) break;
            }
        }

        if (!found) {
            char buf[256];
            sprintf_s(buf, "Warning: ModelManager::LoadModel could not find model file: %s/%s\n", directory.c_str(), filename.c_str());
            Logger::Log(buf);
        }
    }

    // Use the resolved filePath as cache key
    std::string cacheKey = filePath;

    // すでに読み込まれているかチェック（早期リターン）
    if (models_.find(cacheKey) != models_.end()) {
        // 既にある場合はそのポインタを返す
        return models_[cacheKey].get();
    }

    // 新規モデルを生成して読み込む
    auto model = std::make_unique<Model>();
    // If we found a concrete filePath, split into dir+filename for loader
    std::filesystem::path p(filePath);
    std::string useDir = p.parent_path().string();
    std::string useFile = p.filename().string();
    if (model->LoadFromFile(useDir, useFile)) {
        // モデルの初期化
        model->Initialize(modelCommon);
        Model* ptr = model.get();
        // コンテナに格納（所有権を移す）
        models_.insert(std::make_pair(cacheKey, std::move(model)));
        return ptr;
    }

    // No procedural fallback; return nullptr on failure

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
