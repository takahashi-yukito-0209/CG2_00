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

    // ファイルが存在しない場合、いくつかの代替ディレクトリを試し、限定的な探索を行う
    if (!std::filesystem::exists(filePath)) {
        // よく使われる代替ディレクトリを試す
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

        // それでも見つからない場合、プロジェクトルート（現在のパス）から短い再帰検索を行う
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
                    break; // 最初に見つかったものを採用
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

    // 解決された filePath をキャッシュキーとして使用
    std::string cacheKey = filePath;

    // すでに読み込まれているかチェック（早期リターン）
    if (models_.find(cacheKey) != models_.end()) {
        // 既にある場合はそのポインタを返す
        return models_[cacheKey].get();
    }

    // 新規モデルを生成して読み込む
    auto model = std::make_unique<Model>();
    // 実際の filePath が確定している場合、ローダー用にディレクトリとファイル名に分割
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

    // 手続き的フォールバックは無し。失敗時は nullptr を返す

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
