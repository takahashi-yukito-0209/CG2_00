#include "Object3dModelLoader.h"

#include "Logger.h"
#include "mathUtility.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kDefaultObjectTexturePath = "resources/uvChecker.png";
const std::vector<std::string> kModelTextureExtensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
constexpr unsigned int kAssimpLoadFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder;
constexpr size_t kObjectLogBufferSize = 256; // Object3dModelLoader用ログバッファサイズ
std::unordered_map<std::string, Object3d::ModelData> g_modelDataCache; // Assimp読み込み済みモデルのキャッシュ
/// <summary>
/// モデルデータキャッシュで使用するファイルパスキーを作成する。
/// </summary>
std::string MakeModelDataCacheKey(const std::string& filePath)
{
    std::filesystem::path modelPath(filePath); // キャッシュ対象のモデルパス
    std::error_code error; // ファイルシステム操作のエラー情報

    if (std::filesystem::exists(modelPath, error)) {
        std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(modelPath, error); // 表記ゆれを吸収したパス
        if (!error && !canonicalPath.empty()) {
            std::string cacheKey = canonicalPath.generic_string(); // 正規化済みパス
            error.clear();
            const auto writeTime = std::filesystem::last_write_time(canonicalPath, error); // モデルファイルの更新時刻
            if (!error) {
                cacheKey += "#" + std::to_string(writeTime.time_since_epoch().count());
            }
            return cacheKey;
        }
    }

    error.clear();
    std::filesystem::path absolutePath = std::filesystem::absolute(modelPath, error); // 存在しない場合の代替パス
    if (!error && !absolutePath.empty()) {
        return absolutePath.lexically_normal().generic_string();
    }

    return modelPath.lexically_normal().generic_string();
}

/// <summary>
/// 読み込み済みのモデルデータをキャッシュから取得する。
/// </summary>
const Object3d::ModelData* FindCachedModelData(const std::string& cacheKey)
{
    auto cacheIterator = g_modelDataCache.find(cacheKey); // キャッシュ上の検索位置
    if (cacheIterator == g_modelDataCache.end()) {
        return nullptr;
    }

    return &cacheIterator->second;
}

/// <summary>
/// 読み込みに成功したモデルデータをキャッシュへ保存する。
/// </summary>
Object3d::ModelData StoreCachedModelData(const std::string& cacheKey, const Object3d::ModelData& modelData)
{
    g_modelDataCache[cacheKey] = modelData;
    return g_modelDataCache[cacheKey];
}

/// <summary>
/// モデル読み込みで最終的に選択されたテクスチャパスをログへ出力する
/// </summary>
void LogResolvedModelTexturePath(const Object3d::ModelData& modelData)
{
    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "LoadModelFile: 最終的な textureFilePath = %s\n", modelData.material.textureFilePath.c_str());
    Logger::Debug(buffer);
}

/// <summary>
/// 読み込み済みモデルデータを必要に応じてキャッシュへ保存して返す
/// </summary>
Object3d::ModelData FinalizeLoadedModelData(const std::string& cacheKey, const Object3d::ModelData& modelData)
{
    if (!modelData.vertices.empty()) {
        return StoreCachedModelData(cacheKey, modelData);
    }

    return modelData;
}

/// <summary>
/// モデルファイルの読み込みに使用するフルパス文字列を作成する。
/// </summary>
std::string BuildModelFilePath(const std::string& directoryPath, const std::string& filename)
{
    return directoryPath + "/" + filename;
}

/// <summary>
/// Assimp を使用してモデルファイルを読み込む。
/// </summary>
const aiScene* ReadAssimpScene(Assimp::Importer& importer, const std::string& fullPath)
{
    return importer.ReadFile(fullPath.c_str(), kAssimpLoadFlags);
}

/// <summary>
/// Assimp のマテリアルから diffuse テクスチャパスを取得する
/// </summary>
std::string FindDiffuseTexturePathFromMaterials(const aiScene* scene, const std::string& modelDirectory)
{
    std::string textureFilePath; // マテリアルから最後に見つかったテクスチャパス
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial* material = scene->mMaterials[materialIndex]; // Assimp のマテリアル
        if (material->GetTextureCount(aiTextureType_DIFFUSE) == 0) {
            continue;
        }

        aiString assimpTexturePath; // Assimp が返す diffuse テクスチャパス
        material->GetTexture(aiTextureType_DIFFUSE, 0, &assimpTexturePath);
        textureFilePath = modelDirectory + "/" + assimpTexturePath.C_Str();
    }

    return textureFilePath;
}

/// <summary>
/// モデルファイルと同じベース名の画像ファイルを探す
/// </summary>
std::string FindTexturePathByModelBaseName(const std::string& modelDirectory, const std::string& modelBaseName)
{
    for (const auto& extension : kModelTextureExtensions) {
        std::string texturePath = modelDirectory + "/" + modelBaseName + extension; // ベース名から作る候補パス
        if (std::filesystem::exists(texturePath)) {
            char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
            sprintf_s(buffer, "Object3d::LoadModelFile: ベース名からテクスチャを検出 %s\n", texturePath.c_str());
            Logger::Debug(buffer);
            return texturePath;
        }
    }

    return {};
}

/// <summary>
/// モデルディレクトリ内で最初に見つかった画像ファイルを探す
/// </summary>
std::string FindFirstTexturePathInDirectory(const std::string& modelDirectory)
{
    for (const auto& entry : std::filesystem::directory_iterator(modelDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string extension = entry.path().extension().string(); // 判定する拡張子
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (std::find(kModelTextureExtensions.begin(), kModelTextureExtensions.end(), extension) == kModelTextureExtensions.end()) {
            continue;
        }

        std::string texturePath = entry.path().string(); // ディレクトリ内で見つかった画像パス
        char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
        sprintf_s(buffer, "Object3d::LoadModelFile: ディレクトリ内でテクスチャを検出 %s\n", texturePath.c_str());
        Logger::Debug(buffer);
        return texturePath;
    }

    return {};
}

/// <summary>
/// モデルに割り当てるテクスチャパスをマテリアル、ベース名、ディレクトリ内画像、既定画像の順に解決する
/// </summary>
std::string ResolveModelTextureFilePath(const aiScene* scene, const std::filesystem::path& modelPath)
{
    const std::string modelDirectory = modelPath.parent_path().string(); // モデルファイルの配置ディレクトリ
    std::string textureFilePath = FindDiffuseTexturePathFromMaterials(scene, modelDirectory); // マテリアル由来のテクスチャパス
    if (!textureFilePath.empty()) {
        return textureFilePath;
    }

    const std::string modelBaseName = modelPath.stem().string(); // 拡張子を除いたモデル名
    textureFilePath = FindTexturePathByModelBaseName(modelDirectory, modelBaseName);
    if (!textureFilePath.empty()) {
        return textureFilePath;
    }

    textureFilePath = FindFirstTexturePathInDirectory(modelDirectory);
    if (!textureFilePath.empty()) {
        return textureFilePath;
    }

    Logger::Debug(std::string("Object3d::LoadModelFile: テクスチャが見つからなかったため、resources/uvChecker.png を既定として使用\n"));
    return kDefaultObjectTexturePath;
}

/// <summary>
/// Assimp のモデルファイル読み込み失敗を警告ログへ出力する。
/// </summary>
void LogAssimpLoadFailure(const std::string& fullPath)
{
    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: Assimp failed to load %s\n", fullPath.c_str());
    Logger::Warn(buffer);
}

/// <summary>
/// Assimp シーンにメッシュが存在しないことを警告ログへ出力する。
/// </summary>
void LogAssimpSceneHasNoMeshes(const std::string& fullPath)
{
    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: Assimp scene has no meshes %s\n", fullPath.c_str());
    Logger::Warn(buffer);
}

/// <summary>
/// MTL ファイルを開けなかったことを警告ログへ出力する。
/// </summary>
void LogMaterialTemplateOpenFailure(const std::string& directoryPath, const std::string& filename)
{
    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: LoadMaterialTemplateFile failed to open %s/%s\n", directoryPath.c_str(), filename.c_str());
    Logger::Warn(buffer);
}

/// <summary>
/// MTL ファイルから取得した diffuse テクスチャパスをログへ出力する。
/// </summary>
void LogMaterialTemplateTexturePath(const std::string& textureFilePath)
{
    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "LoadMaterialTemplateFile: map_Kd -> %s\n", textureFilePath.c_str());
    Logger::Debug(buffer);
}

/// <summary>
/// MTL ファイルの1行を読み取り、対応するマテリアル情報へ反映する。
/// </summary>
void ApplyMaterialTemplateLine(const std::string& line, const std::string& directoryPath, Object3d::MaterialData& materialData)
{
    std::istringstream lineStream(line); // MTL ファイルから読んだ1行の解析用ストリーム
    std::string identifier; // 行頭の識別子
    lineStream >> identifier;

    if (identifier != "map_Kd") {
        return;
    }

    std::string textureFilename; // MTL に記述されたテクスチャファイル名
    lineStream >> textureFilename;
    materialData.textureFilePath = directoryPath + "/" + textureFilename;
    LogMaterialTemplateTexturePath(materialData.textureFilePath);
}

/// <summary>
/// Assimp で読み込んだシーンがモデルデータとして使用可能か確認する
/// </summary>
bool ValidateAssimpScene(const aiScene* scene, const std::string& fullPath)
{
    if (!scene) {
        LogAssimpLoadFailure(fullPath);
        return false;
    }

    if (!scene->HasMeshes()) {
        LogAssimpSceneHasNoMeshes(fullPath);
        return false;
    }

    return true;
}


/// <summary>
/// Skinning用の初期VertexInfluenceを作成する
/// </summary>
Object3d::VertexInfluence CreateDefaultVertexInfluence()
{
    Object3d::VertexInfluence influence {}; // 初期化済みの影響情報
    influence.weights[0] = 1.0f;
    influence.jointIndices[0] = 0;
    return influence;
}

/// <summary>
/// Node階層をCreateJointと同じ順番で走査し、Node名からJointIndexを引ける辞書を作る
/// </summary>
void BuildNodeIndexMap(const Object3d::ModelData::Node& node, std::unordered_map<std::string, int32_t>& nodeIndexMap, int32_t& nextIndex)
{
    const int32_t currentIndex = nextIndex; // 現在のNodeに割り当てるIndex
    nodeIndexMap[node.name] = currentIndex;
    ++nextIndex;

    for (const Object3d::ModelData::Node& child : node.children) {
        BuildNodeIndexMap(child, nodeIndexMap, nextIndex);
    }
}

/// <summary>
/// Assimpの逆BindPose行列をエンジン側の座標系に変換する
/// </summary>
Math::Matrix4x4 ConvertAssimpInverseBindPoseMatrix(const aiMatrix4x4& inverseBindPoseMatrixAssimp)
{
    aiMatrix4x4 bindPoseMatrixAssimp = inverseBindPoseMatrixAssimp; // Assimp側の逆BindPose行列
    bindPoseMatrixAssimp.Inverse();

    aiVector3D scale; // BindPoseのスケール
    aiQuaternion rotate; // BindPoseの回転
    aiVector3D translate; // BindPoseの平行移動
    bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

    const Math::Vector3 convertedScale = { scale.x, scale.y, scale.z }; // 変換後のスケール
    const Math::Quaternion convertedRotate = { rotate.x, -rotate.y, -rotate.z, rotate.w }; // 変換後の回転
    const Math::Vector3 convertedTranslate = { -translate.x, translate.y, translate.z }; // 変換後の平行移動
    const Math::Matrix4x4 bindPoseMatrix = MathUtil::MakeAffineMatrix(convertedScale, convertedRotate, convertedTranslate); // エンジン側のBindPose行列
    return MathUtil::Inverse(bindPoseMatrix);
}

/// <summary>
/// 頂点にJointの影響を追加する
/// </summary>
void AddVertexInfluence(Object3d::VertexInfluence& influence, int32_t jointIndex, float weight)
{
    uint32_t weakestIndex = 0; // 最も小さい重みのスロット
    float weakestWeight = (std::numeric_limits<float>::max)(); // 最小重みの比較値

    for (uint32_t influenceIndex = 0; influenceIndex < Object3d::kNumMaxInfluence; ++influenceIndex) {
        if (influence.weights[influenceIndex] == 0.0f) {
            influence.weights[influenceIndex] = weight;
            influence.jointIndices[influenceIndex] = jointIndex;
            return;
        }
        if (influence.weights[influenceIndex] < weakestWeight) {
            weakestWeight = influence.weights[influenceIndex];
            weakestIndex = influenceIndex;
        }
    }

    if (weight > weakestWeight) {
        influence.weights[weakestIndex] = weight;
        influence.jointIndices[weakestIndex] = jointIndex;
    }
}

/// <summary>
/// 1頂点分のSkinning重みを正規化する
/// </summary>
void NormalizeVertexInfluence(Object3d::VertexInfluence& influence)
{
    float weightSum = 0.0f; // 合計重み
    for (float weight : influence.weights) {
        weightSum += weight;
    }

    if (weightSum <= 0.0f) {
        influence = CreateDefaultVertexInfluence();
        return;
    }

    for (float& weight : influence.weights) {
        weight /= weightSum;
    }
}

/// <summary>
/// AssimpのBone情報からメッシュ内頂点のSkinning影響を構築する
/// </summary>
std::vector<Object3d::VertexInfluence> BuildMeshVertexInfluences(const aiMesh* mesh, const std::unordered_map<std::string, int32_t>& nodeIndexMap, Object3d::ModelData& modelData)
{
    std::vector<Object3d::VertexInfluence> meshVertexInfluences(mesh->mNumVertices); // メッシュ元頂点ごとの影響情報

    for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        const aiBone* bone = mesh->mBones[boneIndex]; // Assimp側のBone
        const std::string jointName = bone->mName.C_Str(); // 対応するJoint名
        auto jointIterator = nodeIndexMap.find(jointName); // JointIndexの検索位置
        if (jointIterator == nodeIndexMap.end()) {
            continue;
        }

        Object3d::JointWeightData& jointWeightData = modelData.skinClusterData[jointName]; // JointごとのSkinning補助情報
        jointWeightData.inverseBindPoseMatrix = ConvertAssimpInverseBindPoseMatrix(bone->mOffsetMatrix);

        for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
            const aiVertexWeight& vertexWeight = bone->mWeights[weightIndex]; // Boneが持つ頂点重み
            if (vertexWeight.mVertexId >= meshVertexInfluences.size()) {
                continue;
            }
            AddVertexInfluence(meshVertexInfluences[vertexWeight.mVertexId], jointIterator->second, vertexWeight.mWeight);
        }
    }

    for (Object3d::VertexInfluence& influence : meshVertexInfluences) {
        NormalizeVertexInfluence(influence);
    }

    return meshVertexInfluences;
}
/// <summary>
/// Assimp メッシュが頂点展開に必要な法線を持つか確認する。
/// </summary>
bool HasRequiredMeshNormals(const aiMesh* mesh, uint32_t meshIndex)
{
    if (mesh->HasNormals()) {
        return true;
    }

    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: mesh %u has no normals - skipping\n", meshIndex);
    Logger::Warn(buffer);
    return false;
}

/// <summary>
/// Assimp の頂点情報を Object3d 用の頂点データへ変換する。
/// </summary>
Object3d::VertexData ConvertAssimpVertexToObjectVertex(const aiMesh* mesh, uint32_t vertexIndex, bool hasTextureCoords)
{
    const aiVector3D& position = mesh->mVertices[vertexIndex]; // Assimp 側の座標
    const aiVector3D& normal = mesh->mNormals[vertexIndex]; // Assimp 側の法線
    aiVector3D texcoord(0, 0, 0); // Assimp 側のテクスチャ座標
    if (hasTextureCoords) {
        texcoord = mesh->mTextureCoords[0][vertexIndex];
    }

    Object3d::VertexData vertexData; // ModelData に追加する頂点データ
    vertexData.position = { -position.x, position.y, position.z, 1.0f };
    vertexData.texcoord = { texcoord.x, texcoord.y };
    vertexData.normal = { -normal.x, normal.y, normal.z };
    return vertexData;
}

/// <summary>
/// Assimp のメッシュ頂点を Object3d 用の頂点配列へ追加する。
/// </summary>
void AppendMeshSourceVerticesToModelData(const aiMesh* mesh, bool hasTextureCoords, const std::vector<Object3d::VertexInfluence>& meshVertexInfluences, Object3d::ModelData& modelData)
{
    for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
        const Object3d::VertexData vertexData = ConvertAssimpVertexToObjectVertex(mesh, vertexIndex, hasTextureCoords); // 追加する頂点データ
        modelData.vertices.push_back(vertexData);
        modelData.vertexInfluences.push_back(meshVertexInfluences[vertexIndex]);
    }
}

/// <summary>
/// Assimp の三角形面から Object3d 用のIndex配列へ追加する。
/// </summary>
void AppendFaceIndicesToModelData(const aiFace& face, uint32_t vertexBaseIndex, Object3d::ModelData& modelData)
{
    for (uint32_t element = 0; element < 3; ++element) {
        const uint32_t vertexIndex = face.mIndices[element]; // 面が参照するメッシュ内頂点番号
        modelData.indices.push_back(vertexBaseIndex + vertexIndex);
    }
}

/// <summary>
/// Assimp の全メッシュを Object3d 用の頂点データとIndexデータへ変換する。
/// </summary>
void AppendMeshVerticesToModelData(const aiScene* scene, const std::unordered_map<std::string, int32_t>& nodeIndexMap, Object3d::ModelData& modelData)
{
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex]; // 変換対象のメッシュ

        if (!HasRequiredMeshNormals(mesh, meshIndex)) {
            continue;
        }

        const bool hasTextureCoords = mesh->HasTextureCoords(0); // テクスチャ座標を持っているか
        const uint32_t vertexBaseIndex = static_cast<uint32_t>(modelData.vertices.size()); // このメッシュの先頭頂点番号
        const std::vector<Object3d::VertexInfluence> meshVertexInfluences = BuildMeshVertexInfluences(mesh, nodeIndexMap, modelData); // メッシュ元頂点ごとのSkinning影響
        AppendMeshSourceVerticesToModelData(mesh, hasTextureCoords, meshVertexInfluences, modelData);

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex]; // 変換対象の面
            if (face.mNumIndices != 3) {
                continue;
            }

            AppendFaceIndicesToModelData(face, vertexBaseIndex, modelData);
        }
    }
}
/// <summary>
/// AssimpのVector3キーをObject3d用Keyframeに変換して追加する
/// </summary>
void AppendVector3Keyframes(const aiVectorKey* sourceKeys, uint32_t keyCount, double ticksPerSecond, std::vector<Object3d::KeyframeVector3>& destination, bool invertX)
{
    for (uint32_t keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
        const aiVectorKey& sourceKey = sourceKeys[keyIndex]; // Assimp側のキーフレーム
        Object3d::KeyframeVector3 keyframe {}; // 追加するキーフレーム
        keyframe.time = static_cast<float>(sourceKey.mTime / ticksPerSecond);
        keyframe.value = {
            invertX ? -sourceKey.mValue.x : sourceKey.mValue.x,
            sourceKey.mValue.y,
            sourceKey.mValue.z
        };
        destination.push_back(keyframe);
    }
}

/// <summary>
/// AssimpのQuaternionキーをObject3d用Keyframeに変換して追加する
/// </summary>
void AppendQuaternionKeyframes(const aiQuatKey* sourceKeys, uint32_t keyCount, double ticksPerSecond, std::vector<Object3d::KeyframeQuaternion>& destination)
{
    for (uint32_t keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
        const aiQuatKey& sourceKey = sourceKeys[keyIndex]; // Assimp側のキーフレーム
        Object3d::KeyframeQuaternion keyframe {}; // 追加するキーフレーム
        keyframe.time = static_cast<float>(sourceKey.mTime / ticksPerSecond);
        keyframe.value = {
            sourceKey.mValue.x,
            -sourceKey.mValue.y,
            -sourceKey.mValue.z,
            sourceKey.mValue.w
        };
        destination.push_back(keyframe);
    }
}

/// <summary>
/// AssimpのNodeAnimationをObject3d用NodeAnimationに変換する
/// </summary>
Object3d::NodeAnimation ReadNodeAnimation(const aiNodeAnim* channel, double ticksPerSecond)
{
    Object3d::NodeAnimation nodeAnimation {}; // 変換後のノードアニメーション
    AppendVector3Keyframes(channel->mPositionKeys, channel->mNumPositionKeys, ticksPerSecond, nodeAnimation.translate.keyframes, true);
    AppendQuaternionKeyframes(channel->mRotationKeys, channel->mNumRotationKeys, ticksPerSecond, nodeAnimation.rotate.keyframes);
    AppendVector3Keyframes(channel->mScalingKeys, channel->mNumScalingKeys, ticksPerSecond, nodeAnimation.scale.keyframes, false);
    return nodeAnimation;
}

/// <summary>
/// AssimpのAnimationをObject3d用Animationに変換する
/// </summary>
Object3d::Animation BuildAnimationFromAssimpScene(const aiScene* scene)
{
    Object3d::Animation animation {}; // 読み込んだアニメーション
    if (!scene || scene->mNumAnimations == 0) {
        return animation;
    }

    aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーション
    const double ticksPerSecond = animationAssimp->mTicksPerSecond != 0.0 ? animationAssimp->mTicksPerSecond : 1.0; // 秒変換用の周波数
    animation.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        const aiNodeAnim* channel = animationAssimp->mChannels[channelIndex]; // Assimp側のノードアニメーション
        const std::string nodeName = channel->mNodeName.C_Str(); // 対象ノード名
        animation.nodeAnimations[nodeName] = ReadNodeAnimation(channel, ticksPerSecond);
    }

    return animation;
}

/// <summary>
/// Assimp のノードを再帰的に読み込んで Object3d::ModelData::Node に変換する関数
/// </summary>
static Object3d::ModelData::Node ReadNode(const aiNode* node)
{
    Object3d::ModelData::Node result; // 変換後のノード情報
    aiVector3D scale; // Assimp側のスケール
    aiQuaternion rotate; // Assimp側の回転
    aiVector3D translate; // Assimp側の平行移動
    node->mTransformation.Decompose(scale, rotate, translate);

    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };
    result.transform.translate = { -translate.x, translate.y, translate.z };
    result.localMatrix = MathUtil::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);

    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    return result;
}
/// <summary>
/// Assimp のルートノードをモデルデータへ読み込む
/// </summary>
static void ReadRootNodeToModelData(const aiScene* scene, Object3d::ModelData& modelData)
{
    if (!scene->mRootNode) {
        return;
    }

    modelData.rootNode = ReadNode(scene->mRootNode);
}

/// <summary>
/// Assimp シーンから解決したテクスチャパスをモデルデータのマテリアルへ設定する。
/// </summary>
void ReadMaterialTexturePathToModelData(const aiScene* scene, const std::filesystem::path& modelPath, Object3d::ModelData& modelData)
{
    modelData.material.textureFilePath = ResolveModelTextureFilePath(scene, modelPath);
}

/// <summary>
/// Assimp のシーンから Object3d 用のモデルデータを構築する
/// </summary>
static Object3d::ModelData BuildModelDataFromAssimpScene(const aiScene* scene, const std::filesystem::path& modelPath)
{
    Object3d::ModelData modelData; // 構築するモデルデータ

    ReadRootNodeToModelData(scene, modelData);
    std::unordered_map<std::string, int32_t> nodeIndexMap; // Node名からJointIndexを引く辞書
    int32_t nextNodeIndex = 0; // 次に割り当てるJointIndex
    BuildNodeIndexMap(modelData.rootNode, nodeIndexMap, nextNodeIndex);
    AppendMeshVerticesToModelData(scene, nodeIndexMap, modelData);
    ReadMaterialTexturePathToModelData(scene, modelPath, modelData);

    return modelData;
}

/// <summary>
/// Assimp でモデルファイルを読み込み、Object3d 用のモデルデータを構築する。
/// </summary>
static bool TryLoadModelDataFromAssimpFile(const std::string& fullPath, const std::filesystem::path& modelPath, Object3d::ModelData& modelData)
{
    Assimp::Importer importer; // Assimp の読み込み管理
    const aiScene* scene = ReadAssimpScene(importer, fullPath); // Assimp が読み込んだシーン

    if (!ValidateAssimpScene(scene, fullPath)) {
        return false;
    }

    modelData = BuildModelDataFromAssimpScene(scene, modelPath);
    return true;
}

}

namespace MyEngine::Object3dModelLoader {

/// <summary>
/// .mtlファイルを読み取り、マテリアル情報を取得する。
/// </summary>
Object3d::MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    Object3d::MaterialData materialData; // 読み込むMaterialData
    std::string line; // 解析対象の1行

    std::ifstream file(directoryPath + "/" + filename); // 読み込むMTLファイル
    if (!file.is_open()) {
        LogMaterialTemplateOpenFailure(directoryPath, filename);
        return materialData;
    }

    while (std::getline(file, line)) {
        ApplyMaterialTemplateLine(line, directoryPath, materialData);
    }

    return materialData;
}

/// <summary>
/// モデルファイルを読み込み、Object3d用のモデルデータを作成する。
/// </summary>
Object3d::ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
    Object3d::ModelData modelData; // 読み込むModelData

    const std::string fullPath = BuildModelFilePath(directoryPath, filename); // 読み込み対象のフルパス
    const std::filesystem::path modelPath(fullPath); // filesystemで扱うモデルパス
    const std::string cacheKey = MakeModelDataCacheKey(fullPath); // キャッシュ検索用のキー
    if (const Object3d::ModelData* cachedModelData = FindCachedModelData(cacheKey)) {
        return *cachedModelData;
    }

    if (!TryLoadModelDataFromAssimpFile(fullPath, modelPath, modelData)) {
        return modelData;
    }

    LogResolvedModelTexturePath(modelData);
    return FinalizeLoadedModelData(cacheKey, modelData);
}

/// <summary>
/// アニメーションファイルを読み込み、Object3d用のアニメーションデータを作成する。
/// </summary>
Object3d::Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
    Object3d::Animation animation {}; // 読み込むアニメーション
    const std::string fullPath = BuildModelFilePath(directoryPath, filename); // 読み込み対象のフルパス

    Assimp::Importer importer; // Assimpの読み込み管理
    const aiScene* scene = ReadAssimpScene(importer, fullPath); // Assimpが読み込んだシーン
    if (!scene || scene->mNumAnimations == 0) {
        Logger::Warn(std::string("Warning: animation not found ") + fullPath + "\n");
        return animation;
    }

    return BuildAnimationFromAssimpScene(scene);
}

} // namespace MyEngine::Object3dModelLoader
