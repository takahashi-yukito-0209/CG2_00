#include "ModelManager.h"
#include "../utility/ResourceResolver.h"
#include "Logger.h"
#include "Model.h"
#include "TextureManager.h"
#include <cstddef>
#include <filesystem>
#include <vector>

using namespace MyEngine;

std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

namespace {
constexpr size_t kModelManagerLogBufferSize = 256; // ModelManagerのログ用バッファサイズ

/// <summary>
/// モデルキャッシュで使うファイルパスキーを作成する。
/// </summary>
std::string MakeModelCacheKey(const std::string& filePath)
{
    std::filesystem::path modelPath(filePath); // キャッシュ対象のモデルパス
    std::error_code error; // パス正規化時のエラー情報

    if (std::filesystem::exists(modelPath, error)) {
        std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(modelPath, error); // 表記ゆれを吸収したパス
        if (!error && !canonicalPath.empty()) {
            return canonicalPath.generic_string();
        }
    }

    error.clear();
    std::filesystem::path absolutePath = std::filesystem::absolute(modelPath, error); // 存在しない場合の代替キー
    if (!error && !absolutePath.empty()) {
        return absolutePath.lexically_normal().generic_string();
    }

    return modelPath.lexically_normal().generic_string();
}

} // namespace

/// <summary>
/// シングルトンインスタンスを取得する。
/// </summary>
ModelManager* ModelManager::GetInstance()
{
    if (!instance_) {
        instance_.reset(new ModelManager());
    }

    return instance_.get();
}

/// <summary>
/// モデルマネージャーを終了し、保持しているモデルを解放する。
/// </summary>
void ModelManager::Finalize()
{
    models_.clear();
    instance_.reset();
}

/// <summary>
/// モデルを読み込み、読み込み済みの場合はキャッシュ済みモデルを返す。
/// </summary>
Model* ModelManager::LoadModel(const std::string& directory, const std::string& filename, ModelCommon* modelCommon)
{
    std::string requestedDir = directory; // 呼び出し側が指定したモデルディレクトリ
    std::string requestedFilename = filename; // 呼び出し側が指定したモデルファイル名
    std::string filePath = requestedDir + "/" + requestedFilename; // 解決前のモデルパス

    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Model); // Resolverで解決したモデルパス
    if (resolved.empty()) {
        resolved = ResourceResolver::Resolve(requestedFilename, ResourceResolver::Type::Model);
    }

    if (!resolved.empty()) {
        filePath = resolved;
    }

    if (!std::filesystem::exists(filePath)) {
        std::vector<std::string> tryDirs = { requestedDir, requestedDir + "s", "resources", "resource", requestedDir + "/models", "models" }; // 互換用の探索先
        bool found = false; // 代替パスでモデルが見つかったか
        for (const auto& d : tryDirs) {
            std::string tryPath = d + "/" + requestedFilename; // 代替探索用のモデルパス
            if (std::filesystem::exists(tryPath)) {
                requestedDir = d;
                filePath = tryPath;
                found = true;
                break;
            }
        }

        if (!found) {
            char buf[kModelManagerLogBufferSize];
            sprintf_s(buf, "Warning: ModelManager::LoadModel could not find model file: %s/%s\n", directory.c_str(), filename.c_str());
            Logger::Warn(buf);
        }
    }

    const std::string cacheKey = MakeModelCacheKey(filePath); // 同一モデル判定用の正規化済みキャッシュキー
    auto modelIt = models_.find(cacheKey); // キャッシュ済みモデルの検索位置
    if (modelIt != models_.end()) {
        return modelIt->second.get();
    }

    std::error_code relativePathError; // 相対パス変換時のエラー情報
    const std::filesystem::path relativeModelPath = std::filesystem::relative(
        std::filesystem::path(filePath),
        std::filesystem::current_path(),
        relativePathError); // 実行フォルダを基準にしたモデルパス
    if (!relativePathError && !relativeModelPath.empty()) {
        const std::string relativePathText = relativeModelPath.generic_string(); // Assimpへ渡す相対パス
        if (relativePathText != ".." && !relativePathText.starts_with("../")) {
            filePath = relativePathText;
        }
    }

    auto model = std::make_unique<Model>(); // 新規に読み込むモデルインスタンス
    std::filesystem::path modelFilePath(filePath); // 読み込みに使うモデルパス
    std::string useDir = modelFilePath.parent_path().string(); // Modelへ渡すディレクトリ
    std::string useFile = modelFilePath.filename().string(); // Modelへ渡すファイル名
    if (model->LoadFromFile(useDir, useFile)) {
        model->Initialize(modelCommon);
        Model* loadedModel = model.get(); // 呼び出し側へ返すモデルポインタ
        models_.insert(std::make_pair(cacheKey, std::move(model)));
        return loadedModel;
    }

    return nullptr;
}

/// <summary>
/// ファイルパスに対応する読み込み済みモデルを検索して返す。
/// </summary>
Model* ModelManager::FindModel(const std::string& filePath)
{
    const std::string cacheKey = MakeModelCacheKey(filePath); // 検索用の正規化済みキャッシュキー
    auto it = models_.find(cacheKey); // キャッシュ済みモデルの検索位置
    if (it != models_.end()) {
        return it->second.get();
    }

    const std::string resolvedPath = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Model); // Resolverで解決したモデルパス
    if (!resolvedPath.empty()) {
        const std::string resolvedCacheKey = MakeModelCacheKey(resolvedPath); // 解決後パスのキャッシュキー
        it = models_.find(resolvedCacheKey);
        if (it != models_.end()) {
            return it->second.get();
        }
    }

    return nullptr;
}