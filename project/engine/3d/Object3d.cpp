#include "Object3d.h"
#include "../utility/ResourceResolver.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "Object3dCommon.h"
#include "Object3dModelLoader.h"
#include "engine/base/SrvManager.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include <memory>
#include <optional>
#include "mathUtility.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>

using namespace MyEngine;
using Microsoft::WRL::ComPtr;
using namespace Math;

namespace {
constexpr const char* kDefaultObjectTexturePath = "resources/uvChecker.png";
constexpr Vector3 kDefaultTransformScale = { 1.0f, 1.0f, 1.0f }; // 初期スケール
constexpr Vector3 kDefaultTransformRotation = { 0.0f, 0.0f, 0.0f }; // 初期回転
constexpr Vector3 kDefaultTransformTranslation = { 0.0f, 0.0f, 0.0f }; // 初期位置
constexpr Vector3 kDefaultCameraRotation = { 0.3f, 0.0f, 0.0f }; // 内部カメラの初期回転
constexpr Vector3 kDefaultCameraTranslation = { 0.0f, 4.0f, -10.0f }; // 内部カメラの初期位置
constexpr Vector4 kDefaultMaterialColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 初期マテリアル色
constexpr int kDefaultLightingMode = 2; // 初期ライティングモード
constexpr float kDefaultShininess = 32.0f; // 初期スペキュラ指数
constexpr float kDefaultEnvironmentCoefficient = 0.0f; // 初期環境反射係数
constexpr float kEnvironmentCoefficientMin = 0.0f; // 環境反射係数の最小値
constexpr float kEnvironmentCoefficientMax = 1.0f; // 環境反射係数の最大値
constexpr size_t kObjectLogBufferSize = 256; // Object3dのログ用バッファサイズ
constexpr size_t kObjectSrvLogBufferSize = 128; // SRVバインド警告用バッファサイズ
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
    RebuildSkeletonFromModel();
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
    transform_ = {
        kDefaultTransformScale,
        kDefaultTransformRotation,
        kDefaultTransformTranslation
    };
    cameraTransform_ = {
        kDefaultTransformScale,
        kDefaultCameraRotation,
        kDefaultCameraTranslation
    };
}

/// <summary>
/// BOXコライダーを設定する。
/// </summary>
void Object3d::SetBoxCollider(const Math::Vector3& center, const Math::Vector3& size)
{
    hasCollider_ = true;
    collider_.type = CollisionUtility::ColliderType::OBB;
    colliderLocalCenter_ = center;
    colliderSize_ = size;
    RefreshColliderShape();
}

/// <summary>
/// SPHEREコライダーを設定する。
/// </summary>
void Object3d::SetSphereCollider(const Math::Vector3& center, const Math::Vector3& size)
{
    hasCollider_ = true;
    collider_.type = CollisionUtility::ColliderType::Sphere;
    colliderLocalCenter_ = center;
    colliderSize_ = size;
    RefreshColliderShape();
}

/// <summary>
/// CAPSULEコライダーを設定する。
/// </summary>
void Object3d::SetCapsuleCollider(const Math::Vector3& center, const Math::Vector3& size)
{
    hasCollider_ = true;
    collider_.type = CollisionUtility::ColliderType::Capsule;
    colliderLocalCenter_ = center;
    colliderSize_ = size;
    RefreshColliderShape();
}
/// <summary>
/// コライダーを削除する。
/// </summary>
void Object3d::ClearCollider()
{
    hasCollider_ = false;
    collider_ = {};
    colliderLocalCenter_ = { 0.0f, 0.0f, 0.0f };
    colliderSize_ = { 1.0f, 1.0f, 1.0f };
}

/// <summary>
/// コライダーの所属レイヤーと衝突対象マスクを設定する。
/// </summary>
void Object3d::SetColliderLayerMask(CollisionUtility::LayerMask layer, CollisionUtility::LayerMask collideMask)
{
    collider_.layer = layer;
    collider_.collideMask = collideMask;
}

/// <summary>
/// 現在のTransformからコライダー形状を更新する。
/// </summary>
void Object3d::RefreshColliderShape()
{
    if (!hasCollider_) {
        return;
    }

    const Math::Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate); // Object3dのワールド行列
    const Math::Vector3 worldCenter = MathUtil::Transform(colliderLocalCenter_, worldMatrix); // コライダー中心のワールド座標
    if (collider_.type == CollisionUtility::ColliderType::Sphere) {
        const float diameterX = std::fabs(colliderSize_.x * transform_.scale.x); // X方向のワールド直径
        const float diameterY = std::fabs(colliderSize_.y * transform_.scale.y); // Y方向のワールド直径
        const float diameterZ = std::fabs(colliderSize_.z * transform_.scale.z); // Z方向のワールド直径
        const float diameter = (std::max)((std::max)(diameterX, diameterY), diameterZ); // 球として扱う最大直径
        collider_.sphere.center = worldCenter;
        collider_.sphere.radius = (std::max)(diameter * 0.5f, 0.001f);
        collider_.aabb = CollisionUtility::GetSphereAABB(collider_.sphere);
        return;
    }

    if (collider_.type == CollisionUtility::ColliderType::Capsule) {
        const float diameterX = std::fabs(colliderSize_.x * transform_.scale.x); // X方向のワールド直径
        const float diameterZ = std::fabs(colliderSize_.z * transform_.scale.z); // Z方向のワールド直径
        const float radius = (std::max)((std::max)(diameterX, diameterZ) * 0.5f, 0.001f); // カプセル半径
        const float totalHeight = (std::max)(std::fabs(colliderSize_.y * transform_.scale.y), radius * 2.0f); // カプセル全高
        const float segmentHalfLength = (std::max)(totalHeight * 0.5f - radius, 0.0f); // 球端を除いた軸の半分長さ
        const Math::Matrix4x4 rotateMatrix = MathUtil::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, transform_.rotate, { 0.0f, 0.0f, 0.0f }); // 回転だけを反映する行列
        const Math::Vector3 capsuleAxis = MathUtil::SafeNormalize(MathUtil::Transform({ 0.0f, 1.0f, 0.0f }, rotateMatrix), { 0.0f, 1.0f, 0.0f }); // カプセル軸のワールド方向
        collider_.capsule.start = worldCenter - capsuleAxis * segmentHalfLength;
        collider_.capsule.end = worldCenter + capsuleAxis * segmentHalfLength;
        collider_.capsule.radius = radius;
        collider_.aabb = CollisionUtility::GetCapsuleAABB(collider_.capsule);
        return;
    }
    const Math::Vector3 halfLengths = {
        colliderSize_.x * 0.5f,
        colliderSize_.y * 0.5f,
        colliderSize_.z * 0.5f
    }; // コライダーのローカル半サイズ
    Math::Transform colliderTransform = transform_; // コライダー計算用Transform
    colliderTransform.translate = worldCenter;

    collider_.obb = CollisionUtility::MakeOBBFromTransform(colliderTransform, halfLengths);
    collider_.aabb = CollisionUtility::GetOBBAABB(collider_.obb);
}

/// <summary>
/// 使用するテクスチャを指定し、マテリアル情報へ反映する。
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
    RebuildSkeletonFromModel();
    RefreshSkinningResourcesFromModel();
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
    skeleton_ = Skeleton {}; // カスタムメッシュではSkeletonを使用しない
    hasSkeleton_ = false;
    ReleaseSkinningResources();

    if (vertices.empty() || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        DeferReleaseResource(vertexResource_);
        vertexData_ = nullptr;
        vertexBufferView_ = {};
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // DirectX共通処理
    const size_t vertexBufferSize = sizeof(VertexData) * vertices.size(); // 頂点バッファサイズ

    DeferReleaseResource(vertexResource_);
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
    ReleaseSkeletonDebugResources();
    ReleaseSkinningResources();
    ReleaseOwnedGpuResources();
    // modelCommon_ は std::unique_ptr なので自動解放される
}

/// <summary>
/// GPU参照が終わるまでD3D12リソースの解放を遅延する。
/// </summary>
void Object3d::DeferReleaseResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
{
    if (!resource) {
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_ ? object3dCommon_->GetDxCommon() : nullptr; // 遅延解放を管理するDirectX共通処理
    if (dxCommon) {
        dxCommon->DeferReleaseResource(resource);
        return;
    }

    resource.Reset();
}

/// <summary>
/// Object3dが直接保持するGPUリソースを解放予約する。
/// </summary>
void Object3d::ReleaseOwnedGpuResources()
{
    DeferReleaseResource(vertexResource_);
    vertexData_ = nullptr;
    vertexBufferView_ = {};

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (materialResources_[frameIndex] && mappedMaterialData_[frameIndex]) {
            materialResources_[frameIndex]->Unmap(0, nullptr);
        }
        mappedMaterialData_[frameIndex] = nullptr;
        DeferReleaseResource(materialResources_[frameIndex]);

        if (transformationMatrixResources_[frameIndex] && mappedTransformationMatrixData_[frameIndex]) {
            transformationMatrixResources_[frameIndex]->Unmap(0, nullptr);
        }
        mappedTransformationMatrixData_[frameIndex] = nullptr;
        DeferReleaseResource(transformationMatrixResources_[frameIndex]);
    }
}

/// <summary>
/// .mtlファイルを読み取る関数
/// </summary>
Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    return Object3dModelLoader::LoadMaterialTemplateFile(directoryPath, filename);
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
    materialData_->color = kDefaultMaterialColor;
    materialData_->enableLighting = 1;
    materialData_->uvTransform = MathUtil::MakeIdentity4x4();
    materialData_->lightingMode = kDefaultLightingMode;
    useAlphaCutoutSampler_ = false;
    materialData_->useAlphaCutoutSampler = 0;
    useAlphaDiscard_ = true;
    materialData_->useAlphaDiscard = 1;
    materialData_->shininess = kDefaultShininess;
    materialData_->environmentCoefficient = kDefaultEnvironmentCoefficient;
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

/// <summary>
/// 環境マップ反射強度を設定する。
/// </summary>
void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (materialData_) {
        materialData_->environmentCoefficient = std::clamp(
            coefficient,
            kEnvironmentCoefficientMin,
            kEnvironmentCoefficientMax);
    }
}

/// <summary>
/// 環境マップ反射強度を取得する。
/// </summary>
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
/// Object3d側で明示指定されたテクスチャを割り当てる。
/// </summary>
bool Object3d::AssignExplicitTextureOverride()
{
    if (!HasExplicitTextureOverride()) {
        return false;
    }

    std::string resolvedTexturePath; // 解決後のテクスチャパス
    const uint32_t textureIndex = ResolveTextureIndex(modelData_.material.textureFilePath, &resolvedTexturePath, false); // 割り当てるSRV番号

    modelData_.material.textureFilePath = resolvedTexturePath.empty() ? modelData_.material.textureFilePath : resolvedTexturePath;
    modelData_.material.textureIndex = textureIndex;

    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "Object3d::AssignTexture: file=%s -> srvIndex=%u\n", modelData_.material.textureFilePath.c_str(), textureIndex);
    Logger::Debug(buffer);
    return true;
}

/// <summary>
/// Model側のマテリアルテクスチャを使う状態に設定する。
/// </summary>
void Object3d::AssignLoadedModelMaterialTexture()
{
    modelData_.material.textureIndex = UINT32_MAX;
    Logger::Debug("Object3d::AssignTexture: model material texture will be used\n");
}

/// <summary>
/// 既定テクスチャをfallbackとして割り当てる。
/// </summary>
bool Object3d::AssignFallbackTexture()
{
    modelData_.material.textureIndex = ResolveFallbackTextureIndex();
    if (modelData_.material.textureIndex == UINT32_MAX) {
        return false;
    }

    modelData_.material.textureFilePath = kDefaultObjectTexturePath;

    char buffer[kObjectLogBufferSize]; // ログ出力用バッファ
    sprintf_s(buffer, "Object3d::AssignTexture: no material texture specified, defaulting to uvChecker srvIndex=%u\n", modelData_.material.textureIndex);
    Logger::Debug(buffer);
    return true;
}

/// <summary>
/// Object3d側の明示テクスチャ、Model側マテリアル、fallbackの順でテクスチャを割り当てる。
/// </summary>
void Object3d::AssignTexture()
{
    if (AssignExplicitTextureOverride()) {
        return;
    }

    if (UsesLoadedModelMaterialTexture()) {
        AssignLoadedModelMaterialTexture();
        return;
    }

    if (AssignFallbackTexture()) {
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
        char buffer[kObjectSrvLogBufferSize]; // ログ出力用バッファ
        sprintf_s(buffer, "%s: texture SRV is invalid - skipping SRV bind\n", logContext);
        Logger::Debug(buffer);
        return false;
    }

    auto textureManager = TextureManager::GetInstance(); // テクスチャSRVの取得元
    if (!textureManager) {
        char buffer[kObjectSrvLogBufferSize]; // ログ出力用バッファ
        sprintf_s(buffer, "%s: TextureManager is null - skipping SRV bind\n", logContext);
        Logger::Debug(buffer);
        return false;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = textureManager->GetSrvHandleGPU(textureIndex); // 描画に使うSRV
    if (srvHandle.ptr == 0) {
        char buffer[kObjectSrvLogBufferSize]; // ログ出力用バッファ
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
    Matrix4x4 baseWorld = MathUtil::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate); // Object3d自身のワールド行列
    RefreshColliderShape();
    Matrix4x4 world = animationEnabled_ ? MathUtil::Multiply(animationLocalMatrix_, baseWorld) : baseWorld; // アニメーションを合成したワールド行列
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
        DrawSkeletonDebug();
        return;
    }

    // モデルがセットされていない場合は頂点バッファから直接描画する
    if (vertexBufferView_.SizeInBytes == 0) {
        Logger::Debug("Object3d::Draw skipped: no vertex buffer for non-model draw\n");
        return;
    }

    object3dCommon_->SetCommonDrawSetting();

    // VBVを設定
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 非モデル通常描画で使用する共通リソースを設定する
    if (!BindNonModelDrawResources(cmdList)) {
        return;
    }
    // 描画コマンド
    cmdList->DrawInstanced(GetDrawVertexCount(), 1, 0, 0);
    DrawSkeletonDebug();
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
    return Object3dModelLoader::LoadModelFile(directoryPath, filename);
}

/// <summary>
/// アニメーションファイルを読み込む
/// </summary>
Object3d::Animation Object3d::LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
    return Object3dModelLoader::LoadAnimationFile(directoryPath, filename);
}

/// <summary>
/// 任意時刻のVector3値を取得する
/// </summary>
Math::Vector3 Object3d::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time)
{
    assert(!keyframes.empty());
    if (keyframes.size() == 1 || time <= keyframes.front().time) {
        return keyframes.front().value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        const KeyframeVector3& current = keyframes[index]; // 補間元のキーフレーム
        const KeyframeVector3& next = keyframes[index + 1]; // 補間先のキーフレーム
        if (current.time <= time && time <= next.time) {
            const float t = (time - current.time) / (next.time - current.time); // キーフレーム間の補間率
            return MathUtil::Lerp(current.value, next.value, t);
        }
    }

    return keyframes.back().value;
}

/// <summary>
/// 任意時刻のQuaternion値を取得する
/// </summary>
Math::Quaternion Object3d::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time)
{
    assert(!keyframes.empty());
    if (keyframes.size() == 1 || time <= keyframes.front().time) {
        return keyframes.front().value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        const KeyframeQuaternion& current = keyframes[index]; // 補間元のキーフレーム
        const KeyframeQuaternion& next = keyframes[index + 1]; // 補間先のキーフレーム
        if (current.time <= time && time <= next.time) {
            const float t = (time - current.time) / (next.time - current.time); // キーフレーム間の補間率
            return MathUtil::Slerp(current.value, next.value, t);
        }
    }

    return keyframes.back().value;
}

/// <summary>
/// 再生するアニメーションを設定する
/// </summary>
void Object3d::SetAnimation(const Animation& animation)
{
    animation_ = animation;
    animationTime_ = 0.0f;
    hasAnimation_ = animation.duration > 0.0f && !animation.nodeAnimations.empty();
    animationEnabled_ = hasAnimation_;
    animationLocalMatrix_ = MathUtil::MakeIdentity4x4();
}

/// <summary>
/// 指定ファイルからアニメーションを読み込んで設定する
/// </summary>
bool Object3d::SetAnimation(const std::string& filePath)
{
    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Model); // 解決済みのモデルパス
    Animation animation {}; // 読み込むアニメーション
    if (!resolved.empty()) {
        std::filesystem::path path(resolved); // ファイル分解用のパス
        animation = LoadAnimationFile(path.parent_path().string(), path.filename().string());
    } else {
        animation = LoadAnimationFile("resources", filePath);
    }

    SetAnimation(animation);
    return hasAnimation_;
}

/// <summary>
/// アニメーション再生状態を更新する
/// </summary>
void Object3d::UpdateAnimation(float deltaTime)
{
    if (!hasAnimation_ || !animationEnabled_ || animation_.duration <= 0.0f) {
        return;
    }

    animationTime_ += deltaTime * animationPlaybackSpeed_;
    animationTime_ = std::fmod(animationTime_, animation_.duration);
    ApplyAnimationAtCurrentTime();
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
