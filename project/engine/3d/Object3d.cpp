#include "Object3d.h"
#include "../utility/ResourceResolver.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "Object3dCommon.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include <memory>
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "mathUtility.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>

using namespace MyEngine;
using Microsoft::WRL::ComPtr;
using namespace Math;

namespace {
constexpr const char* kDefaultObjectTexturePath = "resources/uvChecker.png";
const std::vector<std::string> kModelTextureExtensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
constexpr unsigned int kAssimpLoadFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder;
std::unordered_map<std::string, Object3d::ModelData> g_modelDataCache; // Assimp読み込み済みモデルデータ

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
    char buffer[256]; // ログ出力用バッファ
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
            char buffer[256]; // ログ出力用バッファ
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
        char buffer[256]; // ログ出力用バッファ
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
    char buffer[256]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: Assimp failed to load %s\n", fullPath.c_str());
    Logger::Warn(buffer);
}

/// <summary>
/// Assimp シーンにメッシュが存在しないことを警告ログへ出力する。
/// </summary>
void LogAssimpSceneHasNoMeshes(const std::string& fullPath)
{
    char buffer[256]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: Assimp scene has no meshes %s\n", fullPath.c_str());
    Logger::Warn(buffer);
}

/// <summary>
/// MTL ファイルを開けなかったことを警告ログへ出力する。
/// </summary>
void LogMaterialTemplateOpenFailure(const std::string& directoryPath, const std::string& filename)
{
    char buffer[256]; // ログ出力用バッファ
    sprintf_s(buffer, "Warning: LoadMaterialTemplateFile failed to open %s/%s\n", directoryPath.c_str(), filename.c_str());
    Logger::Warn(buffer);
}

/// <summary>
/// MTL ファイルから取得した diffuse テクスチャパスをログへ出力する。
/// </summary>
void LogMaterialTemplateTexturePath(const std::string& textureFilePath)
{
    char buffer[256]; // ログ出力用バッファ
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
/// Assimp メッシュが頂点展開に必要な法線を持つか確認する。
/// </summary>
bool HasRequiredMeshNormals(const aiMesh* mesh, uint32_t meshIndex)
{
    if (mesh->HasNormals()) {
        return true;
    }

    char buffer[256]; // ログ出力用バッファ
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
/// Assimp の三角形面を Object3d 用の頂点配列へ追加する。
/// </summary>
void AppendFaceVerticesToModelData(const aiMesh* mesh, const aiFace& face, bool hasTextureCoords, Object3d::ModelData& modelData)
{
    for (uint32_t element = 0; element < 3; ++element) {
        const uint32_t vertexIndex = face.mIndices[element]; // 面が参照する頂点番号
        const Object3d::VertexData vertexData = ConvertAssimpVertexToObjectVertex(mesh, vertexIndex, hasTextureCoords); // 追加する頂点データ
        modelData.vertices.push_back(vertexData);
    }
}

/// <summary>
/// Assimp の全メッシュを Object3d 用の頂点データへ展開する。
/// </summary>
void AppendMeshVerticesToModelData(const aiScene* scene, Object3d::ModelData& modelData)
{
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex]; // 展開対象のメッシュ

        if (!HasRequiredMeshNormals(mesh, meshIndex)) {
            continue;
        }

        const bool hasTextureCoords = mesh->HasTextureCoords(0); // テクスチャ座標を持っているか

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex]; // 展開対象の面
            if (face.mNumIndices != 3) {
                continue;
            }

            AppendFaceVerticesToModelData(mesh, face, hasTextureCoords, modelData);
        }
    }
}
}

/// <summary>
/// Assimp のノードを再帰的に読み込んで Object3d::ModelData::Node に変換する関数
/// </summary>
static Object3d::ModelData::Node ReadNode(const aiNode* node)
{
    Object3d::ModelData::Node result;
    // Assimp の行列は行優先で格納されているため、転置して列優先に変換する
    aiMatrix4x4 aiLocal = node->mTransformation;
    aiLocal.Transpose();
    // 行列のコピー
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            result.localMatrix.m[r][c] = aiLocal[r][c];
        }
    }

    // ノード名のコピー
    result.name = node->mName.C_Str();
    // 子ノードを再帰的に読み込む
    result.children.resize(node->mNumChildren);
    // 子ノードの数だけループして読み込む
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    // 読み込んだノードを返す
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
    AppendMeshVerticesToModelData(scene, modelData);
    ReadMaterialTexturePathToModelData(scene, modelPath, modelData);

    return modelData;
}

/// <summary>
/// Object3d の初期化
/// </summary>
void Object3d::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager)
{
    // 引数で受け取ってメンバ変数に記録する
    this->object3dCommon_ = object3dCommon;

    InitializeTransformState();

    // マテリアル用リソース作成
    CreateMaterialResource();
    // 座標変換行列用リソース作成
    CreateTransformationMatrixResource();

    // ModelCommon を生成して初期化
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(object3dCommon->GetDxCommon());

    // Model を読み込んでセット
    ModelManager* mgr = ModelManager::GetInstance();
    // デフォルトでは plane.obj を読み込むが、後で SetModel(file) で差し替え可能
    model_ = mgr->LoadModel("resources", "plane.obj", modelCommon_.get());
    debugName_ = "plane.obj";
    // モデル読み込み後にテクスチャ割り当てを行う（MTLが先に読み込まれるように）
    AssignTexture();

    // 既定のカメラを参照
    camera_ = object3dCommon->GetDefaultCamera();

    (void)imguiManager;
}

/// <summary>
/// Transform の初期値を設定する。
/// </summary>
void Object3d::InitializeTransformState()
{
    transform_ = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    cameraTransform_ = { { 1.0f, 1.0f, 1.0f }, { 0.3f, 0.0f, 0.0f }, { 0.0f, 4.0f, -10.0f } };
}

/// <summary>
/// ImGuiでオブジェクトの状態を表示・編集する
/// </summary>
void Object3d::DrawImGui(int index)
{
#ifdef USE_IMGUI
    // 選択中オブジェクトの識別名を表示
    ImGui::Text("Object %d : %s", index, debugName_.c_str());
    ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.001f, 1000.0f);
    ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f);
    ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);

    // マテリアル編集
    if (materialData_) {
        ImGui::Checkbox("Use Alpha Cutout Sampler", &useAlphaCutoutSampler_);
        materialData_->useAlphaCutoutSampler = useAlphaCutoutSampler_ ? 1 : 0;
        ImGui::Checkbox("Use Alpha Discard", &useAlphaDiscard_);
        materialData_->useAlphaDiscard = useAlphaDiscard_ ? 1 : 0;
        // マテリアル色を編集
        float col[4] = { materialData_->color.x, materialData_->color.y, materialData_->color.z, materialData_->color.w };
        if (ImGui::ColorEdit4("Color", col)) {
            materialData_->color.x = col[0];
            materialData_->color.y = col[1];
            materialData_->color.z = col[2];
            materialData_->color.w = col[3];
        }
        ImGui::SliderFloat("Environment Reflection", &materialData_->environmentCoefficient, 0.0f, 1.0f);
    }
#else
    (void)index;
    (void)materialData_;
    (void)useAlphaCutoutSampler_;
    (void)useAlphaDiscard_;
#endif
}

/// <summary>
/// 指定したテクスチャをこのオブジェクトへ割り当てる
/// </summary>
void Object3d::SetTexture(const std::string& filePath)
{
    std::string resolvedTexturePath; // 実際にTextureManagerへ渡すテクスチャパス
    const uint32_t textureIndex = ResolveTextureIndex(filePath, &resolvedTexturePath, true); // 明示指定されたテクスチャのSRV番号

    modelData_.material.textureFilePath = resolvedTexturePath.empty() ? filePath : resolvedTexturePath;
    modelData_.material.textureIndex = textureIndex;

    if (!model_) {
        debugName_ = std::string("Custom Mesh : ") + filePath; // カスタムメッシュを識別する表示名
    }
}


/// <summary>
/// ファイルパスを指定してモデルを取得・設定する
/// </summary>
void Object3d::SetModel(const std::string& filePath)
{
    ModelManager* mgr = ModelManager::GetInstance();
    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Model);
    Model* m = nullptr;
    if (!resolved.empty()) {
        // 解決されたパスで読み込む
        std::filesystem::path p(resolved);
        m = mgr->LoadModel(p.parent_path().string(), p.filename().string(), modelCommon_.get());
    } else {
        // 直接指定されたパスで読み込む
        m = mgr->LoadModel("resources", filePath, modelCommon_.get());
    }
    model_ = m; // 成功すればポインタが入る。失敗時は nullptr になる
    debugName_ = filePath; // ImGuiでモデルを識別するための表示名
    // モデル読み込み後にテクスチャの割り当てを行う
    AssignTexture();
}

/// <summary>
/// モデルを使わず、指定した頂点データを設定する
/// </summary>
void Object3d::SetMesh(const std::vector<VertexData>& vertices)
{
    model_ = nullptr;
    debugName_ = "Custom Mesh"; // 直接指定メッシュ用の表示名
    modelData_.vertices = vertices;

    if (vertices.empty() || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        vertexResource_.Reset();
        vertexData_ = nullptr;
        vertexBufferView_ = {};
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // DirectX共通処理
    const size_t vertexBufferSize = sizeof(VertexData) * vertices.size(); // 頂点バッファサイズ

    vertexResource_ = dxCommon->CreateBufferResource(vertexBufferSize);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    std::memcpy(vertexData_, vertices.data(), vertexBufferSize);
    vertexResource_->Unmap(0, nullptr);
    vertexData_ = nullptr;

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    vertexBufferView_.StrideInBytes = static_cast<UINT>(sizeof(VertexData));
}

/// <summary>
/// Object3d の終了処理
/// </summary>
Object3d::~Object3d()
{
    // modelCommon_ は std::unique_ptr なので自動解放される
}

/// <summary>
/// .mtlファイルを読み取る関数
/// </summary>
Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    // 1.中で必要となる変数の宣言
    MaterialData materialData; // 構築するMaterialData
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    if (!file.is_open()) {
        LogMaterialTemplateOpenFailure(directoryPath, filename);
        return materialData;
    }

    // 3.実際にファイルを読み、MaterialDataを構築していく
    while (std::getline(file, line)) {
        ApplyMaterialTemplateLine(line, directoryPath, materialData);
    }

    // 4.MaterialDataを返す
    return materialData;
}

/// <summary>
/// マテリアル用リソースを作成する関数
/// </summary>
void Object3d::CreateMaterialResource()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // GPUリソース生成元
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        materialResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(Material));
        materialResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterialData_[frameIndex]));
    }

    InitializeMaterialState();
}

/// <summary>
/// マテリアルの初期値をCPU側状態へ設定する。
/// </summary>
void Object3d::InitializeMaterialState()
{
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 1;
    materialData_->uvTransform = MathUtil::MakeIdentity4x4();
    materialData_->lightingMode = 2;
    useAlphaCutoutSampler_ = false;
    materialData_->useAlphaCutoutSampler = 0;
    useAlphaDiscard_ = true;
    materialData_->useAlphaDiscard = 1;
    materialData_->shininess = 32.0f;
    materialData_->environmentCoefficient = 0.0f;
}

/// <summary>
/// ライティングの有効状態を取得する
/// </summary>
bool Object3d::GetEnableLighting() const { return materialData_ ? materialData_->enableLighting != 0 : false; }

/// <summary>
/// ライティングの有効/無効を設定する関数
/// </summary>
void Object3d::SetEnableLighting(bool enable)
{
    // マテリアルデータが存在する場合にのみ設定を変更する
    if (materialData_) {
        materialData_->enableLighting = enable ? 1 : 0;
    }
}

/// <summary>
/// ライティングモードを取得する関数
/// </summary>
int Object3d::GetLightingMode() const { return materialData_ ? materialData_->lightingMode : 0; }

/// <summary>
/// ライティングモードを設定する関数
/// </summary>
void Object3d::SetLightingMode(int mode)
{
    // マテリアルデータが存在する場合にのみ設定を変更する
    if (materialData_) {
        materialData_->lightingMode = mode;
    }
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (materialData_) {
        materialData_->environmentCoefficient = std::clamp(coefficient, 0.0f, 1.0f);
    }
}

float Object3d::GetEnvironmentCoefficient() const
{
    return materialData_ ? materialData_->environmentCoefficient : 0.0f;
}

/// <summary>
/// UV変換行列を設定する
/// </summary>
void Object3d::SetUVTransform(const Math::Matrix4x4& uvTransform)
{
    if (materialData_) {
        materialData_->uvTransform = uvTransform;
    }
}

/// <summary>
/// 座標変換行列用リソースを作成する関数
/// </summary>
void Object3d::CreateTransformationMatrixResource()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // GPUリソース生成元
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        transformationMatrixResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
        transformationMatrixResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransformationMatrixData_[frameIndex]));
    }
}

/// <summary>
/// Object3d側で明示指定されたテクスチャがあるか確認する
/// </summary>
bool Object3d::HasExplicitTextureOverride() const
{
    return !modelData_.material.textureFilePath.empty();
}

/// <summary>
/// 読み込み済みモデル側のマテリアルテクスチャを使用するか確認する
/// </summary>
bool Object3d::UsesLoadedModelMaterialTexture() const
{
    return model_ != nullptr;
}

/// <summary>
/// Object3d側の明示テクスチャまたはメッシュ用fallbackを割り当てる
/// </summary>
void Object3d::AssignTexture()
{
    if (HasExplicitTextureOverride()) {
        std::string resolvedTexturePath; // モデルまたは明示指定から解決したテクスチャパス
        const uint32_t textureIndex = ResolveTextureIndex(modelData_.material.textureFilePath, &resolvedTexturePath, false); // 割り当てるSRV番号

        modelData_.material.textureFilePath = resolvedTexturePath.empty() ? modelData_.material.textureFilePath : resolvedTexturePath;
        modelData_.material.textureIndex = textureIndex;

        char buffer[256]; // ログ出力用バッファ
        sprintf_s(buffer, "Object3d::AssignTexture: file=%s -> srvIndex=%u\n", modelData_.material.textureFilePath.c_str(), textureIndex);
        Logger::Debug(buffer);
        return;
    }

    if (UsesLoadedModelMaterialTexture()) {
        modelData_.material.textureIndex = UINT32_MAX;
        Logger::Debug("Object3d::AssignTexture: model material texture will be used\n");
        return;
    }
    modelData_.material.textureIndex = ResolveFallbackTextureIndex();
    if (modelData_.material.textureIndex != UINT32_MAX) {
        modelData_.material.textureFilePath = kDefaultObjectTexturePath;

        char buffer[256]; // ログ出力用バッファ
        sprintf_s(buffer, "Object3d::AssignTexture: no material texture specified, defaulting to uvChecker srvIndex=%u\n", modelData_.material.textureIndex);
        Logger::Debug(buffer);
        return;
    }

    Logger::Debug("Object3d::AssignTexture: no fallback texture available, leaving textureIndex invalid\n");
}


/// <summary>
/// テクスチャパスを解決し、未ロードならロードしてSRV番号を取得する
/// </summary>
uint32_t Object3d::ResolveTextureIndex(const std::string& filePath, std::string* resolvedPath, bool releaseIntermediateAfterLoad) const
{
    auto textureManager = TextureManager::GetInstance(); // テクスチャ管理
    if (!textureManager || filePath.empty()) {
        return UINT32_MAX;
    }

    std::string texturePath = filePath; // 解決前後のテクスチャパス
    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture); // リソース検索後のパス
    if (!resolved.empty()) {
        texturePath = resolved;
    }

    uint32_t textureIndex = textureManager->GetTextureIndexByFilePath(texturePath); // 使用するSRV番号
    if (textureIndex == UINT32_MAX) {
        textureManager->LoadTexture(texturePath);
        if (releaseIntermediateAfterLoad) {
            textureManager->ReleaseIntermediateResources();
        }
        textureIndex = textureManager->GetTextureIndexByFilePath(texturePath);
    }

    if (resolvedPath) {
        *resolvedPath = texturePath;
    }

    return textureIndex;
}

/// <summary>
/// デフォルトテクスチャのSRV番号を取得する
/// </summary>
uint32_t Object3d::ResolveFallbackTextureIndex() const
{
    return ResolveTextureIndex(kDefaultObjectTexturePath, nullptr, false);
}

/// <summary>
/// 非モデル描画で使用する頂点数を取得する
/// </summary>
uint32_t Object3d::GetDrawVertexCount() const
{
    return static_cast<uint32_t>(modelData_.vertices.size());
}

/// <summary>
/// マテリアルCBVを描画用ルートパラメータへ設定する
/// </summary>
bool Object3d::BindMaterialResource(ID3D12GraphicsCommandList* commandList, const char* logContext) const
{
    auto materialResource = GetMaterialResource(); // 現在のフレームで使用するマテリアルCBV
    if (!commandList || !materialResource) {
        if (logContext) {
            Logger::Debug(std::string(logContext) + " skipped: material resource is null\n");
        }
        return false;
    }

    D3D12_GPU_VIRTUAL_ADDRESS materialAddress = materialResource->GetGPUVirtualAddress(); // マテリアルCBVのGPUアドレス
    if (materialAddress == 0) {
        if (logContext) {
            Logger::Debug(std::string(logContext) + " skipped: material GPU address is 0\n");
        }
        return false;
    }

    commandList->SetGraphicsRootConstantBufferView(0, materialAddress);
    return true;
}

/// <summary>
/// 座標変換行列CBVを描画用ルートパラメータへ設定する
/// </summary>
bool Object3d::BindTransformationMatrixResource(ID3D12GraphicsCommandList* commandList, const char* logContext) const
{
    auto transformationResource = GetTransformationMatrixResource(); // 現在のフレームで使用する座標変換行列CBV
    if (!commandList || !transformationResource) {
        if (logContext) {
            Logger::Debug(std::string(logContext) + " skipped: transformation matrix resource is null\n");
        }
        return false;
    }

    D3D12_GPU_VIRTUAL_ADDRESS transformationAddress = transformationResource->GetGPUVirtualAddress(); // 座標変換行列CBVのGPUアドレス
    commandList->SetGraphicsRootConstantBufferView(1, transformationAddress);
    return true;
}

/// <summary>
/// 平行光源CBVを描画用ルートパラメータへ設定する
/// </summary>
bool Object3d::BindDirectionalLightResource(ID3D12GraphicsCommandList* commandList, const char* logContext) const
{
    if (!commandList) {
        return false;
    }

    D3D12_GPU_VIRTUAL_ADDRESS lightAddress = object3dCommon_->GetDirectionalLightGPUAddress(); // 平行光源CBVのGPUアドレス
    if (lightAddress == 0) {
        if (logContext) {
            Logger::Debug(std::string(logContext) + " skipped: directional light GPU address is null\n");
        }
        return false;
    }

    commandList->SetGraphicsRootConstantBufferView(3, lightAddress);
    return true;
}

/// <summary>
/// カメラCBVを描画用ルートパラメータへ設定する
/// </summary>
void Object3d::BindCameraResource(ID3D12GraphicsCommandList* commandList) const
{
    if (!commandList) {
        return;
    }

    D3D12_GPU_VIRTUAL_ADDRESS cameraAddress = object3dCommon_->GetCameraGPUAddress(); // カメラCBVのGPUアドレス
    if (cameraAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView(6, cameraAddress);
    }
}

/// <summary>
/// 点光源CBVを描画用ルートパラメータへ設定する
/// </summary>
void Object3d::BindPointLightResource(ID3D12GraphicsCommandList* commandList) const
{
    if (!commandList) {
        return;
    }

    D3D12_GPU_VIRTUAL_ADDRESS pointLightAddress = object3dCommon_->GetPointLightsGPUAddress(); // 点光源CBVのGPUアドレス
    if (pointLightAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView(7, pointLightAddress);
    }
}

/// <summary>
/// 指定されたテクスチャ番号のSRVを描画用ルートパラメータへ設定する
/// </summary>
bool Object3d::BindTexture(ID3D12GraphicsCommandList* commandList, uint32_t textureIndex, const char* logContext) const
{
    if (!commandList || textureIndex == UINT32_MAX) {
        char buffer[128]; // ログ出力用バッファ
        sprintf_s(buffer, "%s: texture SRV is invalid - skipping SRV bind\n", logContext);
        Logger::Debug(buffer);
        return false;
    }

    auto textureManager = TextureManager::GetInstance(); // テクスチャSRVの取得元
    if (!textureManager) {
        char buffer[128]; // ログ出力用バッファ
        sprintf_s(buffer, "%s: TextureManager is null - skipping SRV bind\n", logContext);
        Logger::Debug(buffer);
        return false;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = textureManager->GetSrvHandleGPU(textureIndex); // 描画に使うSRV
    if (srvHandle.ptr == 0) {
        char buffer[128]; // ログ出力用バッファ
        sprintf_s(buffer, "%s: SRV handle for index %u is null - skipping SRV bind\n", logContext, textureIndex);
        Logger::Debug(buffer);
        return false;
    }

    commandList->SetGraphicsRootDescriptorTable(2, srvHandle);
    return true;
}

/// <summary>
/// インスタンシング用SRVを描画用ルートパラメータへ設定する
/// </summary>
void Object3d::BindInstancingResource(ID3D12GraphicsCommandList* commandList) const
{
    if (!commandList) {
        return;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandle = object3dCommon_->GetInstancingSrvGPUHandle(); // インスタンシング用SRV
    if (instancingSrvHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(4, instancingSrvHandle);
    }
}

/// <summary>
/// 非モデル通常描画で使用する共通リソースを設定する
/// </summary>
bool Object3d::BindNonModelDrawResources(ID3D12GraphicsCommandList* commandList) const
{
    if (!BindMaterialResource(commandList, "Object3d::Draw")) {
        return false;
    }

    if (!BindTransformationMatrixResource(commandList, "Object3d::Draw")) {
        return false;
    }

    if (!BindDirectionalLightResource(commandList, "Object3d::Draw")) {
        return false;
    }

    BindPointLightResource(commandList);
    BindCameraResource(commandList);

    const uint32_t textureIndex = modelData_.material.textureIndex; // カスタムメッシュで使用するテクスチャ番号
    BindTexture(commandList, textureIndex, "Object3d::Draw");
    return true;
}

/// <summary>
/// 非モデルインスタンシング描画で使用する共通リソースを設定する
/// </summary>
bool Object3d::BindNonModelInstancedDrawResources(ID3D12GraphicsCommandList* commandList) const
{
    if (!BindMaterialResource(commandList, nullptr)) {
        return false;
    }

    BindTransformationMatrixResource(commandList, nullptr);
    BindDirectionalLightResource(commandList, nullptr);
    BindCameraResource(commandList);
    BindPointLightResource(commandList);

    const uint32_t textureIndex = modelData_.material.textureIndex; // カスタムメッシュで使用するテクスチャ番号
    BindTexture(commandList, textureIndex, "Object3d::DrawInstanced");
    BindInstancingResource(commandList);
    return true;
}

/// <summary>
/// 座標変換行列を更新して定数バッファに転送する
/// </summary>
void Object3d::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix)
{
    // WVP行列計算
    // ワールド行列
    Matrix4x4 world = MathUtil::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // 逆転置行列（正規行列用にスケール影響除去）
    Matrix4x4 worldInv = MathUtil::Inverse(world);
    Matrix4x4 worldInvTranspose = MathUtil::Transpose(worldInv);

    // ワールドビュー射影行列
    // カメラが設定されていればそれを使用
    Matrix4x4 camView = viewMatrix;
    Matrix4x4 camProj = projectionMatrix;

    // WVP行列の計算
    Matrix4x4 wvp = MathUtil::Multiply(world, MathUtil::Multiply(camView, camProj));

    // 定数バッファに転送
    if (transformationMatrixData_) {
        transformationMatrixData_->World = world;
        transformationMatrixData_->WVP = wvp;
        transformationMatrixData_->WorldInverseTranspose = worldInvTranspose;
    }
}

/// <summary>
/// 描画関数。モデルがセットされていればモデルの Draw に任せる。セットされていなければ頂点バッファから直接描画する。
/// </summary>
void Object3d::Draw()
{
    UpdateFrameResources();
    // Object3dCommon がセットされていない場合は描画できないのでログを出して終了する
    if (!object3dCommon_) {
        Logger::Debug("Object3d::Draw skipped: object3dCommon_ is null\n");
        return;
    }

    // DirectXCommon が Object3dCommon から取得できない場合は描画できないのでログを出して終了する
    if (!object3dCommon_->GetDxCommon()) {
        Logger::Debug("Object3d::Draw skipped: DxCommon is null\n");
        return;
    }

    // 描画に必要なコマンドを積む
    auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList();

    // コマンドリストが取得できない場合は描画できないのでログを出して終了する
    if (!cmdList) {
        Logger::Debug("Object3d::Draw skipped: command list is null\n");
        return;
    }

    // モデルがセットされていればモデル描画に任せる
    if (model_) {
        model_->Draw(this);
        return;
    }

    // モデルがセットされていない場合は頂点バッファから直接描画する
    if (vertexBufferView_.SizeInBytes == 0) {
        Logger::Debug("Object3d::Draw skipped: no vertex buffer for non-model draw\n");
        return;
    }

    // VBVを設定
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 非モデル通常描画で使用する共通リソースを設定する
    if (!BindNonModelDrawResources(cmdList)) {
        return;
    }
    // 描画コマンド
    cmdList->DrawInstanced(GetDrawVertexCount(), 1, 0, 0);
}

/// <summary>
/// 同じメッシュを指定数だけインスタンシング描画する関数
/// </summary>
void Object3d::DrawInstanced(uint32_t instanceCount)
{
    UpdateFrameResources();
    if (instanceCount == 0 || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }

    if (model_) {
        model_->DrawInstanced(this, instanceCount);
        return;
    }

    if (vertexBufferView_.SizeInBytes == 0 || GetDrawVertexCount() == 0) {
        return;
    }

    auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList(); // 描画コマンドリスト
    if (!cmdList) {
        return;
    }

    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    if (!BindNonModelInstancedDrawResources(cmdList)) {
        return;
    }

    cmdList->DrawInstanced(GetDrawVertexCount(), instanceCount, 0, 0);
}

/// <summary>
/// モデルファイルを読みこむ関数。Assimp を使用して obj/glTF 等のモデルファイルを読み取る汎用関数。
/// </summary>
Object3d::ModelData Object3d::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData; // 構築するModelData

    const std::string fullPath = BuildModelFilePath(directoryPath, filename); // モデルファイルのフルパス
    const std::filesystem::path objPath(fullPath); // filesystem 操作用のモデルパス
    const std::string cacheKey = MakeModelDataCacheKey(fullPath); // モデルデータキャッシュの検索キー
    if (const ModelData* cachedModelData = FindCachedModelData(cacheKey)) {
        return *cachedModelData;
    }

    Assimp::Importer importer; // Assimp の読み込み管理
    const aiScene* scene = ReadAssimpScene(importer, fullPath); // Assimp が読み込んだシーン

    if (!ValidateAssimpScene(scene, fullPath)) {
        return modelData;
    }

    modelData = BuildModelDataFromAssimpScene(scene, objPath); // Assimp シーンからモデルデータを構築

    LogResolvedModelTexturePath(modelData);
    return FinalizeLoadedModelData(cacheKey, modelData);
}

/// <summary>
/// 現在のフレーム用GPUバッファへCPU側の状態を転送する
/// </summary>
void Object3d::UpdateFrameResources()
{
    if (!object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }

    const uint32_t frameIndex = object3dCommon_->GetDxCommon()->GetCurrentFrameIndex(); // 転送先フレーム番号
    UploadMaterialFrameResource(frameIndex);
    UploadTransformationMatrixFrameResource(frameIndex);
}

/// <summary>
/// 現在のフレームで使用するマテリアル状態をGPUバッファへ転送する。
/// </summary>
void Object3d::UploadMaterialFrameResource(uint32_t frameIndex)
{
    if (mappedMaterialData_[frameIndex]) {
        *mappedMaterialData_[frameIndex] = materialState_;
    }
}

/// <summary>
/// 現在のフレームで使用する座標変換行列をGPUバッファへ転送する。
/// </summary>
void Object3d::UploadTransformationMatrixFrameResource(uint32_t frameIndex)
{
    if (mappedTransformationMatrixData_[frameIndex]) {
        *mappedTransformationMatrixData_[frameIndex] = transformationMatrixState_;
    }
}

/// <summary>
/// 現在のフレームで使用するマテリアルリソースを取得する
/// </summary>
Microsoft::WRL::ComPtr<ID3D12Resource> const& Object3d::GetMaterialResource() const
{
    static const Microsoft::WRL::ComPtr<ID3D12Resource> emptyResource;
    if (!object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return emptyResource;
    }
    return materialResources_[object3dCommon_->GetDxCommon()->GetCurrentFrameIndex()];
}

/// <summary>
/// 現在のフレームで使用する変換行列リソースを取得する
/// </summary>
Microsoft::WRL::ComPtr<ID3D12Resource> const& Object3d::GetTransformationMatrixResource() const
{
    static const Microsoft::WRL::ComPtr<ID3D12Resource> emptyResource;
    if (!object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return emptyResource;
    }
    return transformationMatrixResources_[object3dCommon_->GetDxCommon()->GetCurrentFrameIndex()];
}
