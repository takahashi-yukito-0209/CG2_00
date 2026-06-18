#include "ModelManager.h"
#include "../utility/ResourceResolver.h"
#include "Logger.h"
#include "Model.h"
#include <filesystem>
#include <vector>

using namespace MyEngine;

// シングルトンインスタンスの初期化
std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
ModelManager* ModelManager::GetInstance()
{
    // インスタンスがまだ存在しない場合は生成する
    if (!instance_) {
        instance_.reset(new ModelManager());
    }

    return instance_.get();
}

/// <summary>
/// 終了処理
/// </summary>
void ModelManager::Finalize()
{
    // すべてのモデルを解放
    models_.clear();
    // シングルトン自身を破棄
    instance_.reset();
}

/// <summary>
/// モデルの読み込みと取得
/// </summary>
Model* ModelManager::LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon)
{
    // ファイルパスをキーとして結合
    std::string requestedDir = directory;
    std::string requestedFilename = filename;
    std::string filePath = requestedDir + "/" + requestedFilename;

    // まずは ResourceResolver を使って直接解決を試みる
    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Model);
    if (resolved.empty()) {
        // 直接の結合パスで見つからない場合、ファイル名だけで再度解決を試みる（リソース登録がディレクトリ無しの場合に対応）
        resolved = ResourceResolver::Resolve(requestedFilename, ResourceResolver::Type::Model);
    }

    // 解決されたパスが存在する場合はそれを使用する
    if (!resolved.empty()) {
        filePath = resolved;
    }

    // ファイルが存在しない場合、いくつかの代替ディレクトリを試し、限定的な探索を行う
    if (!std::filesystem::exists(filePath)) {
        // よく使われる代替ディレクトリを試す
        std::vector<std::string> tryDirs = { requestedDir, requestedDir + "s", "resources", "resource", requestedDir + "/models", "models" };
        // 代替ディレクトリで見つかったかどうかのフラグ
        bool found = false;
        // 代替ディレクトリを順番に試す
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
            // 最初に見つかったものを採用するため、見つかるたびに更新していく
            for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())) {

                // ファイルのみ対象とする
                if (!entry.is_regular_file()) {
                    continue;
                }

                // ファイル名が一致するかをチェック
                if (entry.path().filename() == requestedFilename) {
                    // 見つかった場合は requestedDir と filePath を更新してループを抜ける
                    requestedDir = entry.path().parent_path().string();
                    requestedFilename = entry.path().filename().string();
                    filePath = entry.path().string();
                    found = true;
                    ++foundCount;
                    break; // 最初に見つかったものを採用
                }

                // 見つかった数が上限に達したら探索を打ち切る
                if (foundCount >= maxSearchResults) {
                    break;
                }
            }
        }

        // それでも見つからない場合は警告ログを出す
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

/// <summary>
/// ファイルパスに対応するモデルを検索して返す（見つからない場合は nullptr を返す）
/// </summary>
Model* ModelManager::FindModel(const std::string& filePath)
{
    auto it = models_.find(filePath); // ファイルパスをキーにしてモデルを検索
    // 見つかった場合は生ポインタを返す
    if (it != models_.end()) {
        return it->second.get();
    }

    // 見つからない場合は nullptr を返す
    return nullptr;
}
