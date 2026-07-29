#include "LevelLoader.h"

#include "../utility/JsonFileLoader.h"
#include "../utility/Logger.h"
#include "../utility/mathUtility.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace MyEngine {
namespace {

/// <summary>
/// 呼び出し元へレベル読み込み失敗理由を設定する。
/// </summary>
void SetLoadError(std::string* errorMessage, const std::string& message)
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
/// JSONメンバーから文字列値を取得する。
/// </summary>
bool ReadStringMember(const JsonDocument& object, const char* name, std::string& value)
{
    if (!object.is_object()) {
        return false;
    }

    const auto member = object.find(name); // 読み取るJSONメンバー
    if (member == object.end() || !member->is_string()) {
        return false;
    }

    value = member->get<std::string>();
    return true;
}

/// <summary>
/// JSONメンバーから整数値を取得する。
/// </summary>
bool ReadIntMember(const JsonDocument& object, const char* name, int& value)
{
    if (!object.is_object()) {
        return false;
    }

    const auto member = object.find(name); // 読み取るJSONメンバー
    if (member == object.end() || !member->is_number_integer()) {
        return false;
    }

    value = member->get<int>();
    return true;
}

/// <summary>
/// JSON上のコライダー種別をエンジンで扱える種別へ正規化する。
/// </summary>
std::string NormalizeLevelColliderType(const std::string& colliderType)
{
    if (colliderType == "SPHERE" || colliderType == "CIRCLE") {
        return "SPHERE";
    }
    if (colliderType == "CAPSULE") {
        return "CAPSULE";
    }

    return "BOX";
}

/// <summary>
/// コライダーサイズの1成分を保存・判定に使える正の値へ補正する。
/// </summary>
float SanitizeLevelColliderSizeValue(float value)
{
    constexpr float kMinimumColliderSize = 0.001f; // コライダーサイズの最小値
    return (std::max)(std::fabs(value), kMinimumColliderSize);
}

/// <summary>
/// コライダー種別に合わせてサイズを補正する。
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
        const float diameter = (std::max)(sanitizedSize.x, sanitizedSize.z); // カプセルの直径
        sanitizedSize.x = diameter;
        sanitizedSize.y = (std::max)(sanitizedSize.y, diameter);
        sanitizedSize.z = diameter;
    }

    return sanitizedSize;
}

/// <summary>
/// 必須JSON文字列メンバーを読み込み、失敗時は詳細理由を設定する。
/// </summary>
bool ReadRequiredStringMember(const JsonDocument& object, const char* name, const std::string& objectPath, std::string& value, std::string* errorMessage)
{
    if (!object.is_object()) {
        SetLoadError(errorMessage, objectPath + " must be object.");
        return false;
    }

    const auto member = object.find(name); // 読み取るJSONメンバー
    const std::string memberPath = BuildMemberPath(objectPath, name); // エラー表示用パス
    if (member == object.end()) {
        SetLoadError(errorMessage, memberPath + " is required.");
        return false;
    }
    if (!member->is_string()) {
        SetLoadError(errorMessage, memberPath + " must be string.");
        return false;
    }

    value = member->get<std::string>();
    return true;
}

/// <summary>
/// JSON配列からVector3値を取得する。
/// </summary>
bool ReadVector3Value(const JsonDocument& value, Math::Vector3& vector)
{
    if (!value.is_array() || value.size() != 3) {
        return false;
    }

    for (const JsonDocument& element : value) {
        if (!element.is_number()) {
            return false;
        }
    }

    vector.x = value[0].get<float>();
    vector.y = value[1].get<float>();
    vector.z = value[2].get<float>();
    return true;
}

/// <summary>
/// JSONオブジェクトのメンバーからVector3値を取得する。
/// </summary>
bool ReadVector3Member(const JsonDocument& object, const char* name, Math::Vector3& vector)
{
    if (!object.is_object()) {
        return false;
    }

    const auto member = object.find(name); // 読み取るJSONメンバー
    if (member == object.end()) {
        return false;
    }

    return ReadVector3Value(*member, vector);
}

/// <summary>
/// 必須JSON Vector3メンバーを読み込み、失敗時は詳細理由を設定する。
/// </summary>
bool ReadRequiredVector3Member(const JsonDocument& object, const char* name, const std::string& objectPath, Math::Vector3& vector, std::string* errorMessage)
{
    if (!object.is_object()) {
        SetLoadError(errorMessage, objectPath + " must be object.");
        return false;
    }

    const auto member = object.find(name); // 読み取るJSONメンバー
    const std::string memberPath = BuildMemberPath(objectPath, name); // エラー表示用パス
    if (member == object.end()) {
        SetLoadError(errorMessage, memberPath + " is required.");
        return false;
    }
    if (!ReadVector3Value(*member, vector)) {
        SetLoadError(errorMessage, memberPath + " must be array of 3 numbers.");
        return false;
    }

    return true;
}

/// <summary>
/// Blender座標の平行移動をエンジン座標へ変換する。
/// </summary>
Math::Vector3 ConvertBlenderTranslation(const Math::Vector3& translation)
{
    return { -translation.x, translation.y, translation.z };
}

/// <summary>
/// Blenderの度数法Euler回転をエンジン用ラジアン回転へ変換する。
/// </summary>
Math::Vector3 ConvertBlenderRotationDegrees(const Math::Vector3& rotationDegrees)
{
    return {
        MathUtil::DegToRad(rotationDegrees.x),
        MathUtil::DegToRad(-rotationDegrees.y),
        MathUtil::DegToRad(-rotationDegrees.z)
    };
}

/// <summary>
/// Blender座標のスケールをエンジン座標へ変換する。
/// </summary>
Math::Vector3 ConvertBlenderScale(const Math::Vector3& scale)
{
    return scale;
}

/// <summary>
/// 親を持たない場合に使う単位Transformを作成する。
/// </summary>
Math::Transform CreateIdentityTransform()
{
    Math::Transform transform { // 親Transformがない場合の基準
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };
    return transform;
}

/// <summary>
/// 子のローカルTransformへ親のワールドTransformを反映する。
/// </summary>
Math::Transform CombineParentTransform(const Math::Transform& parentTransform, const Math::Transform& localTransform)
{
    const Math::Matrix4x4 parentMatrix = MathUtil::MakeAffineMatrix(parentTransform.scale, parentTransform.rotate, parentTransform.translate); // 子の原点を変換する親行列
    Math::Transform worldTransform { // 親を反映したワールドTransform
        {
            parentTransform.scale.x * localTransform.scale.x,
            parentTransform.scale.y * localTransform.scale.y,
            parentTransform.scale.z * localTransform.scale.z,
        },
        {
            parentTransform.rotate.x + localTransform.rotate.x,
            parentTransform.rotate.y + localTransform.rotate.y,
            parentTransform.rotate.z + localTransform.rotate.z,
        },
        MathUtil::Transform(localTransform.translate, parentMatrix)
    };
    return worldTransform;
}

/// <summary>
/// レベルオブジェクト階層のローカルTransformをワールドTransformへ変換する。
/// </summary>
void ResolveWorldTransformsRecursive(std::vector<LevelObjectData>& objectDataList, const Math::Transform& parentTransform)
{
    for (LevelObjectData& objectData : objectDataList) {
        objectData.transform = CombineParentTransform(parentTransform, objectData.localTransform);
        ResolveWorldTransformsRecursive(objectData.children, objectData.transform);
    }
}

/// <summary>
/// JSONのtransformオブジェクトからTransform値を読み込む。
/// </summary>
bool ReadTransform(const JsonDocument& object, const std::string& objectPath, Math::Transform& transform, std::string* errorMessage)
{
    if (!object.is_object()) {
        SetLoadError(errorMessage, objectPath + " must be object.");
        return false;
    }

    const auto transformObject = object.find("transform"); // transformメンバー
    const std::string transformPath = BuildMemberPath(objectPath, "transform"); // エラー表示用パス
    if (transformObject == object.end()) {
        SetLoadError(errorMessage, transformPath + " is required.");
        return false;
    }
    if (!transformObject->is_object()) {
        SetLoadError(errorMessage, transformPath + " must be object.");
        return false;
    }

    Math::Vector3 translation { 0.0f, 0.0f, 0.0f }; // Blender座標の平行移動
    Math::Vector3 rotationDegrees { 0.0f, 0.0f, 0.0f }; // Blender座標の度数法回転
    Math::Vector3 scaling { 1.0f, 1.0f, 1.0f }; // Blender座標のスケール
    if (!ReadRequiredVector3Member(*transformObject, "translation", transformPath, translation, errorMessage)
        || !ReadRequiredVector3Member(*transformObject, "rotation", transformPath, rotationDegrees, errorMessage)
        || !ReadRequiredVector3Member(*transformObject, "scaling", transformPath, scaling, errorMessage)) {
        return false;
    }

    transform.translate = ConvertBlenderTranslation(translation);
    transform.rotate = ConvertBlenderRotationDegrees(rotationDegrees);
    transform.scale = ConvertBlenderScale(scaling);
    return true;
}

/// <summary>
/// JSONのcolliderオブジェクトからコライダー情報を読み込む。
/// </summary>
bool ReadCollider(const JsonDocument& object, const std::string& objectPath, LevelColliderData& collider, std::string* errorMessage)
{
    if (!object.is_object()) {
        collider.enabled = false;
        return true;
    }

    const auto colliderObject = object.find("collider"); // colliderメンバー
    const std::string colliderPath = BuildMemberPath(objectPath, "collider"); // エラー表示用パス
    if (colliderObject == object.end()) {
        collider.enabled = false;
        return true;
    }
    if (!colliderObject->is_object()) {
        SetLoadError(errorMessage, colliderPath + " must be object.");
        return false;
    }

    Math::Vector3 center { 0.0f, 0.0f, 0.0f }; // Blender座標のコライダー中心
    Math::Vector3 size { 1.0f, 1.0f, 1.0f }; // Blender座標のコライダーサイズ
    collider.enabled = true;
    std::string colliderType; // JSONから読み取ったコライダー種別
    std::string legacyColliderShape; // 旧形式shapeから読み取った表示形状
    ReadStringMember(*colliderObject, "type", colliderType);
    ReadStringMember(*colliderObject, "shape", legacyColliderShape);
    if (!legacyColliderShape.empty() && legacyColliderShape != "BOX") {
        colliderType = legacyColliderShape;
    }
    collider.type = NormalizeLevelColliderType(colliderType);
    const auto centerMember = colliderObject->find("center"); // centerメンバー
    if (centerMember != colliderObject->end()) {
        if (!ReadVector3Value(*centerMember, center)) {
            SetLoadError(errorMessage, BuildMemberPath(colliderPath, "center") + " must be array of 3 numbers.");
            return false;
        }
        collider.center = ConvertBlenderTranslation(center);
    }
    const auto sizeMember = colliderObject->find("size"); // sizeメンバー
    if (sizeMember != colliderObject->end()) {
        if (!ReadVector3Value(*sizeMember, size)) {
            SetLoadError(errorMessage, BuildMemberPath(colliderPath, "size") + " must be array of 3 numbers.");
            return false;
        }
        collider.size = size;
    }
    collider.size = SanitizeLevelColliderSize(collider.type, collider.size);
    return true;
}

/// <summary>
/// JSONオブジェクトからレベルオブジェクト1件を読み込む。
/// </summary>
bool ReadObjectData(const JsonDocument& object, const std::string& objectPath, LevelObjectData& objectData, std::string* errorMessage)
{
    if (!object.is_object()) {
        SetLoadError(errorMessage, objectPath + " must be object.");
        return false;
    }

    if (!ReadRequiredStringMember(object, "type", objectPath, objectData.type, errorMessage)) {
        return false;
    }

    ReadStringMember(object, "name", objectData.name);
    ReadStringMember(object, "file_name", objectData.fileName);
    ReadStringMember(object, "prefab_source", objectData.prefabSource);
    if (!ReadTransform(object, objectPath, objectData.localTransform, errorMessage)) {
        return false;
    }

    objectData.transform = objectData.localTransform;
    if (!ReadCollider(object, objectPath, objectData.collider, errorMessage)) {
        return false;
    }

    const auto children = object.find("children"); // 子オブジェクト配列
    if (children != object.end()) {
        const std::string childrenPath = BuildMemberPath(objectPath, "children"); // エラー表示用パス
        if (!children->is_array()) {
            SetLoadError(errorMessage, childrenPath + " must be array.");
            return false;
        }
        objectData.children.reserve(children->size());
        for (size_t childIndex = 0; childIndex < children->size(); ++childIndex) {
            LevelObjectData childData; // 追加する子オブジェクト情報
            if (!ReadObjectData((*children)[childIndex], BuildArrayItemPath(childrenPath, childIndex), childData, errorMessage)) {
                return false;
            }
            objectData.children.push_back(std::move(childData));
        }
    }

    return true;
}

/// <summary>
/// JSONルートからレベルデータを構築する。
/// </summary>
bool BuildLevelData(const JsonDocument& root, LevelData& levelData, std::string* errorMessage)
{
    if (!root.is_object()) {
        SetLoadError(errorMessage, "root must be object.");
        return false;
    }

    ReadIntMember(root, "schema_version", levelData.schemaVersion);
    ReadStringMember(root, "name", levelData.name);
    if (levelData.name != "scene") {
        SetLoadError(errorMessage, "name must be \"scene\".");
        Logger::Warn("Warning: LevelLoader expected root name \"scene\".\n");
        return false;
    }

    const auto objects = root.find("objects"); // ルート直下のオブジェクト配列
    if (objects == root.end()) {
        SetLoadError(errorMessage, "objects is required.");
        return false;
    }
    if (!objects->is_array()) {
        SetLoadError(errorMessage, "objects must be array.");
        return false;
    }

    levelData.objects.clear();
    levelData.objects.reserve(objects->size());
    for (size_t objectIndex = 0; objectIndex < objects->size(); ++objectIndex) {
        LevelObjectData objectData; // 追加するオブジェクト情報
        if (!ReadObjectData((*objects)[objectIndex], BuildArrayItemPath("objects", objectIndex), objectData, errorMessage)) {
            return false;
        }
        levelData.objects.push_back(std::move(objectData));
    }

    LevelLoader::ResolveWorldTransforms(levelData);
    return true;
}

} // namespace

/// <summary>
/// レベルデータ内のローカルTransformからワールドTransformを再計算する。
/// </summary>
void LevelLoader::ResolveWorldTransforms(LevelData& levelData)
{
    ResolveWorldTransformsRecursive(levelData.objects, CreateIdentityTransform());
}

/// <summary>
/// 指定したJSONファイルからレベルデータを読み込む。
/// </summary>
bool LevelLoader::Load(const std::string& filePath, LevelData& levelData)
{
    return Load(filePath, levelData, nullptr);
}

/// <summary>
/// 指定したJSONファイルからレベルデータを読み込み、失敗理由を取得する。
/// </summary>
bool LevelLoader::Load(const std::string& filePath, LevelData& levelData, std::string* errorMessage)
{
    levelData = {};
    SetLoadError(errorMessage, std::string());

    JsonDocument root; // パースしたJSONルート
    std::string resolvedPath; // 解決済みJSONファイルパス
    std::string jsonLoadError; // JSONファイル読み込み失敗理由
    if (!JsonFileLoader::Load(filePath, root, &resolvedPath, ResourceResolver::Type::Json, &jsonLoadError)) {
        SetLoadError(errorMessage, jsonLoadError.empty() ? std::string("Load failed: ") + filePath : jsonLoadError);
        return false;
    }

    if (!BuildLevelData(root, levelData, errorMessage)) {
        Logger::Warn(std::string("Warning: LevelLoader invalid level data: ") + resolvedPath + "\n");
        if (errorMessage && !errorMessage->empty()) {
            *errorMessage = resolvedPath + ": " + *errorMessage;
        }
        levelData = {};
        return false;
    }

    if (errorMessage) {
        *errorMessage = std::string("Loaded: ") + resolvedPath;
    }
    return true;
}

} // namespace MyEngine