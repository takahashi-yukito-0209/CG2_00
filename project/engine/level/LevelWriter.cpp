#include "LevelWriter.h"

#include "../utility/FileUtility.h"
#include "../utility/JsonFileLoader.h"
#include "../utility/Logger.h"
#include "../utility/mathUtility.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace MyEngine {
namespace fs = std::filesystem;
namespace {
constexpr const char* kProjectResourcesDirectory = "project/resources"; // プロジェクト側のリソースディレクトリ
constexpr const char* kGeneratedToProjectResourcesDirectory = "../../../project/resources"; // 実行フォルダから見たプロジェクト側リソースディレクトリ
constexpr const char* kRuntimeResourcesDirectory = "resources"; // 実行フォルダ側のリソースディレクトリ
constexpr const char* kResourcesPrefix = "resources/"; // リソース基準指定の接頭辞
constexpr const char* kProjectResourcesPrefix = "project/resources/"; // プロジェクトリソース基準指定の接頭辞

/// <summary>
/// 呼び出し元へレベル保存結果メッセージを設定する。
/// </summary>
void SetSaveError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

/// <summary>
/// JSON階層のメンバー位置を表す文字列を作成する。
/// </summary>
std::string BuildMemberPath(const std::string& parentPath, const std::string& memberName)
{
    return parentPath.empty() ? memberName : parentPath + "." + memberName;
}

/// <summary>
/// JSON配列内の要素位置を表す文字列を作成する。
/// </summary>
std::string BuildArrayItemPath(const std::string& arrayPath, size_t itemIndex)
{
    return arrayPath + "[" + std::to_string(itemIndex) + "]";
}

/// <summary>
/// 指定文字列が接頭辞から始まるか判定する。
/// </summary>
bool StartsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

/// <summary>
/// レベル保存名をリソースディレクトリ基準の相対パスへ変換する。
/// </summary>
fs::path BuildResourceRelativePath(const std::string& filePath)
{
    fs::path relativePath(filePath); // リソース基準に直す保存名
    const std::string genericPath = relativePath.generic_string(); // 区切り文字を揃えた保存名

    if (StartsWith(genericPath, kProjectResourcesPrefix)) {
        return fs::path(genericPath.substr(std::string(kProjectResourcesPrefix).size()));
    }
    if (StartsWith(genericPath, kResourcesPrefix)) {
        return fs::path(genericPath.substr(std::string(kResourcesPrefix).size()));
    }

    return relativePath;
}

/// <summary>
/// 利用可能なプロジェクト側リソースディレクトリを探す。
/// </summary>
fs::path FindWritableResourceDirectory()
{
    const std::array<fs::path, 3> candidateDirectories {
        fs::current_path() / kProjectResourcesDirectory,
        fs::current_path() / kGeneratedToProjectResourcesDirectory,
        fs::current_path() / kRuntimeResourcesDirectory,
    }; // 保存先候補のリソースディレクトリ

    for (const fs::path& candidateDirectory : candidateDirectories) {
        const fs::path normalizedDirectory = candidateDirectory.lexically_normal(); // 正規化した候補ディレクトリ
        if (FileUtility::IsDirectory(normalizedDirectory.generic_string())) {
            return normalizedDirectory;
        }
    }

    return fs::path();
}

/// <summary>
/// 保存可能なコライダー種別か判定する。
/// </summary>
bool IsSupportedLevelColliderType(const std::string& colliderType)
{
    return colliderType == "BOX" || colliderType == "SPHERE" || colliderType == "CAPSULE";
}

/// <summary>
/// コライダーサイズの1成分を保存用の正の値へ補正する。
/// </summary>
float SanitizeLevelColliderSizeValue(float value)
{
    constexpr float kMinimumColliderSize = 0.001f; // コライダーサイズの最小値
    return (std::max)(std::fabs(value), kMinimumColliderSize);
}

/// <summary>
/// コライダー種別に合わせて保存用サイズを補正する。
/// </summary>
Math::Vector3 SanitizeLevelColliderSize(const std::string& colliderType, const Math::Vector3& size)
{
    Math::Vector3 sanitizedSize {
        SanitizeLevelColliderSizeValue(size.x),
        SanitizeLevelColliderSizeValue(size.y),
        SanitizeLevelColliderSizeValue(size.z)
    }; // 正の値へ補正したサイズ

    if (colliderType == "SPHERE") {
        const float diameter = (std::max)((std::max)(sanitizedSize.x, sanitizedSize.y), sanitizedSize.z); // 球の直径
        sanitizedSize = { diameter, diameter, diameter };
    } else if (colliderType == "CAPSULE") {
        const float diameter = (std::max)(sanitizedSize.x, sanitizedSize.z); // 保存用カプセルの直径
        sanitizedSize.x = diameter;
        sanitizedSize.y = (std::max)(sanitizedSize.y, diameter);
        sanitizedSize.z = diameter;
    }

    return sanitizedSize;
}

/// <summary>
/// Vector3をJSON配列へ変換する。
/// </summary>
JsonDocument WriteVector3(const Math::Vector3& value)
{
    JsonDocument arrayValue = JsonDocument::array(); // Vector3を書き込むJSON配列
    arrayValue.push_back(value.x);
    arrayValue.push_back(value.y);
    arrayValue.push_back(value.z);
    return arrayValue;
}

/// <summary>
/// エンジン座標の平行移動をBlender座標へ戻す。
/// </summary>
Math::Vector3 ConvertEngineTranslationToBlender(const Math::Vector3& translation)
{
    Math::Vector3 blenderTranslation { // Blenderへ書き戻す平行移動
        -translation.x,
        translation.y,
        translation.z,
    };
    return blenderTranslation;
}

/// <summary>
/// エンジン座標の回転をBlender座標の度数法へ戻す。
/// </summary>
Math::Vector3 ConvertEngineRotationToBlenderDegrees(const Math::Vector3& rotation)
{
    Math::Vector3 blenderRotation { // Blenderへ書き戻す度数法回転
        MathUtil::RadToDeg(rotation.x),
        MathUtil::RadToDeg(-rotation.y),
        MathUtil::RadToDeg(-rotation.z),
    };
    return blenderRotation;
}

/// <summary>
/// TransformをBlenderレベルJSON形式へ変換する。
/// </summary>
JsonDocument WriteTransform(const Math::Transform& transform)
{
    JsonDocument transformObject = JsonDocument::object(); // transformメンバーへ書き込むJSONオブジェクト
    transformObject["translation"] = WriteVector3(ConvertEngineTranslationToBlender(transform.translate));
    transformObject["rotation"] = WriteVector3(ConvertEngineRotationToBlenderDegrees(transform.rotate));
    transformObject["scaling"] = WriteVector3(transform.scale);
    return transformObject;
}

/// <summary>
/// コライダー情報をBlenderレベルJSON形式へ変換する。
/// </summary>
JsonDocument WriteCollider(const LevelColliderData& collider)
{
    const std::string colliderType = IsSupportedLevelColliderType(collider.type) ? collider.type : "BOX"; // 保存するコライダー種別
    const Math::Vector3 colliderSize = SanitizeLevelColliderSize(colliderType, collider.size); // 保存するコライダーサイズ
    JsonDocument colliderObject = JsonDocument::object(); // colliderメンバーへ書き込むJSONオブジェクト
    colliderObject["enabled"] = collider.enabled;
    colliderObject["type"] = colliderType;
    colliderObject["center"] = WriteVector3(ConvertEngineTranslationToBlender(collider.center));
    colliderObject["size"] = WriteVector3(colliderSize);
    return colliderObject;
}

/// <summary>
/// レベルオブジェクト1件を親子階層つきのBlenderレベルJSON形式へ変換する。
/// </summary>
JsonDocument WriteObject(const LevelObjectData& objectData)
{
    JsonDocument object = JsonDocument::object(); // オブジェクト1件分のJSON
    object["type"] = objectData.type;
    object["name"] = objectData.name;
    object["file_name"] = objectData.fileName;
    if (!objectData.prefabSource.empty()) {
        object["prefab_source"] = objectData.prefabSource;
    }
    object["transform"] = WriteTransform(objectData.localTransform);
    if (objectData.collider.enabled) {
        object["collider"] = WriteCollider(objectData.collider);
    }
    if (!objectData.children.empty()) {
        JsonDocument children = JsonDocument::array(); // 子オブジェクトを書き込むJSON配列
        for (const LevelObjectData& childData : objectData.children) {
            children.push_back(WriteObject(childData));
        }
        object["children"] = std::move(children);
    }
    return object;
}

/// <summary>
/// 再ロードに必要なtypeが空のオブジェクト位置を探す。
/// </summary>
bool FindEmptyTypeObjectPath(const std::vector<LevelObjectData>& objectDataList, const std::string& objectListPath, std::string& outObjectPath)
{
    for (size_t objectIndex = 0; objectIndex < objectDataList.size(); ++objectIndex) {
        const LevelObjectData& objectData = objectDataList[objectIndex]; // 検証対象のレベルオブジェクト
        const std::string objectPath = BuildArrayItemPath(objectListPath, objectIndex); // エラー表示用パス
        if (objectData.type.empty()) {
            outObjectPath = BuildMemberPath(objectPath, "type");
            return true;
        }
        if (FindEmptyTypeObjectPath(objectData.children, BuildMemberPath(objectPath, "children"), outObjectPath)) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// ルート直下のレベルオブジェクトを親子階層つきでJSON配列へ追加する。
/// </summary>
void AppendHierarchyObjects(const std::vector<LevelObjectData>& objectDataList, JsonDocument& outputObjects)
{
    for (const LevelObjectData& objectData : objectDataList) {
        outputObjects.push_back(WriteObject(objectData));
    }
}

} // namespace

/// <summary>
/// レベルJSONの保存先として使用する実ファイルパスを解決する。
/// </summary>
std::string LevelWriter::ResolveWritableLevelPath(const std::string& filePath)
{
    if (filePath.empty()) {
        return std::string();
    }

    try {
        const fs::path inputPath(filePath); // 入力された保存先パス
        if (inputPath.is_absolute()) {
            return inputPath.lexically_normal().generic_string();
        }

        const fs::path resourceDirectory = FindWritableResourceDirectory(); // 書き込み先リソースディレクトリ
        if (resourceDirectory.empty()) {
            return inputPath.lexically_normal().generic_string();
        }

        const fs::path resourceRelativePath = BuildResourceRelativePath(filePath); // リソース基準の相対保存先
        return (resourceDirectory / resourceRelativePath).lexically_normal().generic_string();
    } catch (...) {
    }

    return filePath;
}

/// <summary>
/// 現在のローカルTransformと親子階層を再ロード可能なJSONとして保存する。
/// </summary>
bool LevelWriter::SaveHierarchySnapshot(const std::string& filePath, const LevelData& levelData)
{
    return SaveHierarchySnapshot(filePath, levelData, nullptr);
}

/// <summary>
/// 現在のローカルTransformと親子階層を再ロード可能なJSONとして保存し、失敗理由を取得する。
/// </summary>
bool LevelWriter::SaveHierarchySnapshot(const std::string& filePath, const LevelData& levelData, std::string* errorMessage)
{
    SetSaveError(errorMessage, std::string());
    if (filePath.empty()) {
        const std::string message = "Save path is empty."; // 保存失敗理由
        Logger::Warn(std::string("Warning: LevelWriter ") + message + "\n");
        SetSaveError(errorMessage, message);
        return false;
    }

    std::string emptyTypePath; // typeが空のオブジェクト位置
    if (FindEmptyTypeObjectPath(levelData.objects, "objects", emptyTypePath)) {
        const std::string message = "Cannot save reloadable level because " + emptyTypePath + " is empty."; // 保存失敗理由
        Logger::Warn(std::string("Warning: LevelWriter ") + message + "\n");
        SetSaveError(errorMessage, message);
        return false;
    }

    JsonDocument root = JsonDocument::object(); // 書き出すJSONルート
    JsonDocument objects = JsonDocument::array(); // 書き出すオブジェクト配列
    AppendHierarchyObjects(levelData.objects, objects);

    root["schema_version"] = kCurrentLevelSchemaVersion;
    root["name"] = levelData.name.empty() ? "scene" : levelData.name;
    root["objects"] = std::move(objects);

    const std::string jsonText = root.dump(4); // 整形済みJSON文字列
    const std::string resolvedFilePath = ResolveWritableLevelPath(filePath); // 実際に書き込む保存先
    if (!FileUtility::WriteText(resolvedFilePath, jsonText)) {
        const std::string message = "Failed to save: " + resolvedFilePath; // 保存失敗理由
        Logger::Warn(std::string("Warning: LevelWriter ") + message + "\n");
        SetSaveError(errorMessage, message);
        return false;
    }

    SetSaveError(errorMessage, "Saved hierarchy snapshot: " + resolvedFilePath);
    return true;
}

/// <summary>
/// 互換用に階層を維持したJSON保存を呼び出す。
/// </summary>
bool LevelWriter::SaveFlatSnapshot(const std::string& filePath, const LevelData& levelData)
{
    return SaveHierarchySnapshot(filePath, levelData);
}

} // namespace MyEngine
