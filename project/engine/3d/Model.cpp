#include "Model.h"
#include "../utility/ResourceResolver.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "ModelCommon.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include "mathUtility.h"
#include <cassert>
#include <cstring>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kFallbackModelTexturePath = "resources/uvChecker.png";

/// <summary>
/// モデル描画用のfallbackテクスチャを未ロードなら読み込む
/// </summary>
void EnsureFallbackModelTextureLoaded(TextureManager* textureManager)
{
    uint32_t fallbackTextureIndex = textureManager->GetSrvIndex(kFallbackModelTexturePath); // fallbackテクスチャのSRV番号
    if (fallbackTextureIndex == UINT32_MAX) {
        textureManager->LoadTexture(kFallbackModelTexturePath);
    }
}

/// <summary>
/// モデルマテリアルに設定されたテクスチャのSRV番号を取得する
/// </summary>
uint32_t ResolveModelMaterialTextureIndex(TextureManager* textureManager, const Object3d::MaterialData& materialData)
{
    if (materialData.textureFilePath.empty()) {
        return UINT32_MAX;
    }

    uint32_t textureIndex = textureManager->GetTextureIndexByFilePath(materialData.textureFilePath); // マテリアルテクスチャのSRV番号
    if (textureIndex == UINT32_MAX) {
        textureManager->LoadTexture(materialData.textureFilePath);
        textureIndex = textureManager->GetTextureIndexByFilePath(materialData.textureFilePath);
    }

    return textureIndex;
}
}

/// <summary>
/// モデルの終了処理
/// </summary>
Model::~Model()
{
    FinalizeGpuResources();
}

/// <summary>
/// GPU参照が終わるまでD3D12リソースの解放を遅延する。
/// </summary>
void Model::DeferReleaseResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
{
    if (!resource) {
        return;
    }

    DirectXCommon* dxCommon = dxCommon_; // 遅延解放を管理するDirectX共通処理
    if (dxCommon) {
        dxCommon->DeferReleaseResource(resource);
        return;
    }

    resource.Reset();
}

/// <summary>
/// モデルが保持するGPUリソースを解放予約する。
/// </summary>
void Model::FinalizeGpuResources()
{
    DeferReleaseResource(vertexResource_);
    DeferReleaseResource(intermediateResource_);
    DeferReleaseResource(indexResource_);
    DeferReleaseResource(vertexInfluenceResource_);

    vertexBufferView_ = {};
    indexBufferView_ = {};
    vertexInfluenceBufferView_ = {};
    materialTextureIndices_.clear();
}

/// <summary>
/// Object3d側で明示指定されたテクスチャ番号を取得する。
/// </summary>
uint32_t Model::ResolveOwnerTextureOverrideIndex(const Object3d* owner) const
{
    if (!owner) {
        return UINT32_MAX;
    }

    const auto& ownerMaterial = owner->GetModelData().material; // Object3dで明示指定されたマテリアル
    const bool hasOwnerTextureOverride = !ownerMaterial.textureFilePath.empty() && ownerMaterial.textureIndex != UINT32_MAX; // Object3d側の明示テクスチャが有効か
    if (!hasOwnerTextureOverride) {
        return UINT32_MAX;
    }

    return ownerMaterial.textureIndex;
}

/// <summary>
/// Model自身が保持しているテクスチャ番号を取得する。
/// </summary>
uint32_t Model::ResolveModelTextureIndex() const
{
    return textureIndex_;
}

/// <summary>
/// fallbackテクスチャのSRV番号を取得する。
/// </summary>
uint32_t Model::ResolveFallbackTextureIndex() const
{
    auto textureManager = TextureManager::GetInstance(); // fallbackテクスチャの取得元
    if (!textureManager) {
        return UINT32_MAX;
    }

    return textureManager->GetSrvIndex(kFallbackModelTexturePath);
}

/// <summary>
/// 指定マテリアル番号のテクスチャ番号を取得する。
/// </summary>
uint32_t Model::ResolveMaterialTextureIndex(uint32_t materialIndex) const
{
    if (materialIndex < materialTextureIndices_.size()) {
        const uint32_t materialTextureIndex = materialTextureIndices_[materialIndex]; // サブメッシュが参照するマテリアルのSRV番号
        if (materialTextureIndex != UINT32_MAX) {
            return materialTextureIndex;
        }
    }

    const uint32_t modelTextureIndex = ResolveModelTextureIndex(); // 代表マテリアルのSRV番号
    if (modelTextureIndex != UINT32_MAX) {
        return modelTextureIndex;
    }

    return ResolveFallbackTextureIndex();
}

/// <summary>
/// サブメッシュ描画時に使用するテクスチャ番号を決定する。
/// </summary>
uint32_t Model::ResolveMeshPartTextureIndex(const Object3d* owner, uint32_t materialIndex) const
{
    const uint32_t ownerTextureIndex = ResolveOwnerTextureOverrideIndex(owner); // Object3d側の明示テクスチャSRV番号
    if (ownerTextureIndex != UINT32_MAX) {
        return ownerTextureIndex;
    }

    return ResolveMaterialTextureIndex(materialIndex);
}
/// <summary>
/// 描画時に使用するテクスチャ番号を決定する。
/// </summary>
uint32_t Model::ResolveTextureIndex(const Object3d* owner) const
{
    const uint32_t ownerTextureIndex = ResolveOwnerTextureOverrideIndex(owner); // Object3d側の明示テクスチャSRV番号
    if (ownerTextureIndex != UINT32_MAX) {
        return ownerTextureIndex;
    }

    const uint32_t modelTextureIndex = ResolveModelTextureIndex(); // Model側のマテリアルテクスチャSRV番号
    if (modelTextureIndex != UINT32_MAX) {
        return modelTextureIndex;
    }

    return ResolveFallbackTextureIndex();
}

/// <summary>
/// 描画時に使用する頂点データを取得する
/// </summary>
const std::vector<Object3d::VertexData>& Model::ResolveDrawVertices(const Object3d* owner) const
{
    if (modelData_.vertices.empty()) {
        return owner->GetModelData().vertices;
    }

    return modelData_.vertices;
}

/// <summary>
/// 描画時に使用するIndexデータを取得する
/// </summary>
const std::vector<uint32_t>& Model::ResolveDrawIndices(const Object3d* owner) const
{
    if (modelData_.indices.empty()) {
        return owner->GetModelData().indices;
    }

    return modelData_.indices;
}
/// <summary>
/// 描画時に使用する頂点バッファビューを取得する
/// </summary>
D3D12_VERTEX_BUFFER_VIEW Model::ResolveVertexBufferView(const Object3d* owner) const
{
    if (vertexBufferView_.SizeInBytes != 0) {
        return vertexBufferView_;
    }

    return owner->GetVertexBufferView();
}

/// <summary>
/// 描画時に使用するIndexバッファビューを取得する
/// </summary>
D3D12_INDEX_BUFFER_VIEW Model::ResolveIndexBufferView(const Object3d* owner) const
{
    (void)owner;
    return indexBufferView_;
}

/// <summary>
/// サブメッシュ情報があればマテリアル単位でIndex描画を行う。
/// </summary>
bool Model::DrawMeshPartsIndexed(ID3D12GraphicsCommandList* commandList, const Object3d* owner, uint32_t instanceCount, const char* logContext) const
{
    const std::vector<uint32_t>& indices = ResolveDrawIndices(owner); // 描画に使うIndexデータ
    const D3D12_INDEX_BUFFER_VIEW indexBufferView = ResolveIndexBufferView(owner); // 描画に使うIndexBufferView
    if (indices.empty() || indexBufferView.SizeInBytes == 0 || modelData_.meshParts.empty()) {
        return false;
    }

    commandList->IASetIndexBuffer(&indexBufferView);
    for (const Object3d::ModelData::MeshPart& meshPart : modelData_.meshParts) {
        if (meshPart.indexCount == 0 || meshPart.indexOffset >= indices.size()) {
            continue;
        }

        const uint32_t drawIndexCount = (std::min)(meshPart.indexCount, static_cast<uint32_t>(indices.size()) - meshPart.indexOffset); // 範囲外参照を防いだ描画Index数
        const uint32_t textureIndex = ResolveMeshPartTextureIndex(owner, meshPart.materialIndex); // サブメッシュに使用するSRV番号
        BindTexture(commandList, textureIndex, logContext);
        commandList->DrawIndexedInstanced(drawIndexCount, instanceCount, meshPart.indexOffset, 0, 0);
    }

    return true;
}

/// <summary>
/// IndexがあればIndex描画、なければ従来の頂点描画を行う
/// </summary>
void Model::DrawIndexedOrVertices(ID3D12GraphicsCommandList* commandList, const Object3d* owner, uint32_t instanceCount, const char* logContext) const
{
    if (DrawMeshPartsIndexed(commandList, owner, instanceCount, logContext)) {
        return;
    }

    const uint32_t textureIndex = ResolveTextureIndex(owner); // 単一描画で使うSRV番号
    BindTexture(commandList, textureIndex, logContext);

    const std::vector<uint32_t>& indices = ResolveDrawIndices(owner); // 描画に使うIndexデータ
    const D3D12_INDEX_BUFFER_VIEW indexBufferView = ResolveIndexBufferView(owner); // 描画に使うIndexBufferView
    if (!indices.empty() && indexBufferView.SizeInBytes != 0) {
        commandList->IASetIndexBuffer(&indexBufferView);
        commandList->DrawIndexedInstanced(static_cast<UINT>(indices.size()), instanceCount, 0, 0, 0);
        return;
    }

    const std::vector<Object3d::VertexData>& vertices = ResolveDrawVertices(owner); // 描画に使う頂点データ
    if (vertices.empty()) {
        return;
    }

    commandList->DrawInstanced(static_cast<UINT>(vertices.size()), instanceCount, 0, 0);
}
/// <summary>
/// オーナーのマテリアルCBVを描画用ルートパラメータへ設定する
/// </summary>
bool Model::BindOwnerMaterialResource(ID3D12GraphicsCommandList* commandList, const Object3d* owner, const char* logContext) const
{
    auto materialResource = owner->GetMaterialResource(); // オーナーが保持するマテリアルCBV
    if (!materialResource) {
        if (logContext) {
            Logger::Debug(std::string(logContext) + " skipped: owner material resource missing\n");
        }
        return false;
    }

    D3D12_GPU_VIRTUAL_ADDRESS materialAddress = materialResource->GetGPUVirtualAddress(); // Material CBV のGPUアドレス
    if (materialAddress == 0) {
        if (logContext) {
            Logger::Debug(std::string(logContext) + " skipped: owner material GPU address is 0\n");
        }
        return false;
    }

    commandList->SetGraphicsRootConstantBufferView(0, materialAddress);
    return true;
}

/// <summary>
/// 指定されたテクスチャ番号のSRVを描画用ルートパラメータへ設定する
/// </summary>
bool Model::BindTexture(ID3D12GraphicsCommandList* commandList, uint32_t textureIndex, const char* logContext) const
{
    if (!commandList || textureIndex == UINT32_MAX) {
        Logger::Debug(std::string(logContext) + ": texture SRV is invalid - skipping SRV bind\n");
        return false;
    }

    auto textureManager = TextureManager::GetInstance(); // テクスチャSRVの取得元
    if (!textureManager) {
        Logger::Debug(std::string(logContext) + ": TextureManager is null - skipping SRV bind\n");
        return false;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = textureManager->GetSrvHandleGPU(textureIndex); // 描画に使うSRV
    if (srvHandle.ptr == 0) {
        char buffer[256];
        sprintf_s(buffer, "%s: srv handle for index %u is null - skipping SRV bind\n", logContext, textureIndex);
        Logger::Debug(buffer);
        return false;
    }

    commandList->SetGraphicsRootDescriptorTable(2, srvHandle);
    return true;
}

/// <summary>
/// 描画
/// </summary>
void Model::Draw(Object3d* owner)
{
    // 前提条件のチェック
    // オーナーオブジェクトが有効でなければ描画できない
    if (!owner)
        return;

    // DirectXCommon の参照はオーナーの Object3dCommon から取得するのが安全
    DirectXCommon* dxCommon = nullptr;
    if (owner->GetObject3dCommon() && owner->GetObject3dCommon()->GetDxCommon()) {
        dxCommon = owner->GetObject3dCommon()->GetDxCommon();
    } else if (dxCommon_) {
        // フォールバック: モデル初期化時に保持したDirectXCommonを使う
        dxCommon = dxCommon_;
    }

    // DirectXCommon が取得できない場合は描画できない
    if (!dxCommon) {
        Logger::Debug("Model::Draw skipped: DirectXCommon unavailable\n");
        return;
    }

    // コマンドリストの取得
    auto cmdList = dxCommon->GetCommandList();
    // コマンドリストが取得できない場合は描画できない
    if (!cmdList) {
        Logger::Debug("Model::Draw skipped: command list is null\n");
        return;
    }

    Object3dCommon* common = owner->GetObject3dCommon(); // Object3d共通描画管理
    // Object3dCommonが有効でない場合は描画できない
    if (!common) {
        Logger::Debug("Model::Draw skipped: missing Object3dCommon\n");
        return;
    }

    const bool useSkinning = owner->CanUseSkinning() && vertexInfluenceBufferView_.SizeInBytes != 0; // Skinning描画を使うか
    if (useSkinning) {
        common->SetSkinningDrawSetting();
        D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[2] = {
            ResolveVertexBufferView(owner),
            vertexInfluenceBufferView_
        }; // Skinning用の頂点バッファ一覧
        cmdList->IASetVertexBuffers(0, 2, vertexBufferViews);
        cmdList->SetGraphicsRootDescriptorTable(10, owner->GetSkinningPaletteSrvHandle());
    } else {
        common->SetCommonDrawSetting();
        D3D12_VERTEX_BUFFER_VIEW vbv = ResolveVertexBufferView(owner); // 描画に使う頂点バッファビュー
        cmdList->IASetVertexBuffers(0, 1, &vbv);
    }

    // Material CBV は Object3d が持つものを使用する
    if (!BindOwnerMaterialResource(cmdList, owner, "Model::Draw")) {
        return;
    }

    // 座標変換行列CBV設定 (オーナーから)
    if (owner->GetTransformationMatrixResource()) {
        // オーナーが座標変換行列リソースを持っている場合はそれを使用
        cmdList->SetGraphicsRootConstantBufferView(1, owner->GetTransformationMatrixResource()->GetGPUVirtualAddress());
    } else {
        // オーナーが座標変換行列リソースを持っていない場合は描画できない
        Logger::Debug("Model::Draw skipped: transformation matrix CBV missing\n");
        return;
    }

    // 平行光源のGPUアドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = common->GetDirectionalLightGPUAddress();
    // 平行光源のGPUアドレスが0の場合は描画できない
    if (lightAddr == 0) {
        Logger::Debug("Model::Draw skipped: directional light CBV missing\n");
        return;
    }
    // ルートパラメータ3に平行光源CBVをバインド
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

    // 点光源CBV設定 (shared in Object3dCommon)
    D3D12_GPU_VIRTUAL_ADDRESS plAddr = common->GetPointLightsGPUAddress();
    // 点光源のGPUアドレスが0の場合は点光源CBVをバインドしない（点光源が存在しない場合もあるため）
    if (plAddr != 0) {
        // 点光源のGPUアドレスが有効な場合はルートパラメータ7に点光源CBVをバインド
        cmdList->SetGraphicsRootConstantBufferView(7, plAddr);
    }

    // カメラCBV設定 (shared in Object3dCommon)
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = common->GetCameraGPUAddress();
    // カメラのGPUアドレスが0の場合はカメラCBVをバインドしない（カメラ情報が存在しない場合もあるため）
    if (camAddr != 0) {
        // カメラのGPUアドレスが有効な場合はルートパラメータ6にカメラCBVをバインド
        cmdList->SetGraphicsRootConstantBufferView(6, camAddr);
    }

    // 非インスタンス描画パス: 予期せぬディスクリプタテーブルの競合を避けるため、インスタンシングSRV（ルートパラメータ4）はバインドしない。

    DrawIndexedOrVertices(cmdList, owner, 1, "Model::Draw");
}
/// <summary>
/// インスタンシング描画
/// </summary>
void Model::DrawInstanced(Object3d* owner, uint32_t instanceCount)
{
    // 前提条件のチェック
    if (!owner) {
        return;
    }

    DirectXCommon* dxCommon = nullptr;
    // DirectXCommon の参照はオーナーの Object3dCommon から取得するのが安全
    if (owner->GetObject3dCommon() && owner->GetObject3dCommon()->GetDxCommon()) {
        // オーナーの Object3dCommon から DirectXCommon を取得
        dxCommon = owner->GetObject3dCommon()->GetDxCommon();
    } else if (dxCommon_) {
        // フォールバック: モデル初期化時に保持したDirectXCommonを使う
        dxCommon = dxCommon_;
    }

    // DirectXCommon が取得できない場合は描画できない
    if (!dxCommon) {
        return;
    }

    // コマンドリストの取得
    auto cmdList = dxCommon->GetCommandList();
    // コマンドリストが取得できない場合は描画できない
    if (!cmdList) {
        return;
    }

    // インスタンシング描画パスでは、インスタンシング用SRVはバインドしない（ルートパラメータ4は使用しない）。
    D3D12_VERTEX_BUFFER_VIEW vbv = ResolveVertexBufferView(owner); // 描画に使う頂点バッファビュー
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    // Material CBV は Object3d が持つものを使用する
    if (!BindOwnerMaterialResource(cmdList, owner, nullptr)) {
        return;
    }


    // 変換行列（インスタンシング使用時はオーナーのCBVは未使用）
    if (owner->GetTransformationMatrixResource()) {
        // インスタンシング描画パスでは、オーナーの座標変換行列CBVはバインドしない（ルートパラメータ1は使用しない）。
        cmdList->SetGraphicsRootConstantBufferView(1, owner->GetTransformationMatrixResource()->GetGPUVirtualAddress());
    }

    // オブジェクト3D共通情報の取得
    Object3dCommon* common = owner->GetObject3dCommon();
    // Object3dCommonが有効でない場合は描画できない
    if (!common) {
        return;
    }

    // 平行光源のGPUアドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = common->GetDirectionalLightGPUAddress();
    // 平行光源のGPUアドレスが0の場合は描画できない
    if (lightAddr == 0) {
        return;
    }

    // ルートパラメータ3に平行光源CBVをバインド
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

    // カメラのGPUアドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = common->GetCameraGPUAddress();
    // カメラが有効な場合はルートパラメータ6にカメラCBVをバインド
    if (camAddr != 0) {
        cmdList->SetGraphicsRootConstantBufferView(6, camAddr);
    }

    // 点光源のGPUアドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS plAddrInst = common->GetPointLightsGPUAddress();
    // インスタンシング描画パスでは、点光源CBVはバインドしない（ルートパラメータ7は使用しない）。
    if (plAddrInst != 0) {
        cmdList->SetGraphicsRootConstantBufferView(7, plAddrInst);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE instSrv = common->GetInstancingSrvGPUHandle();
    // インスタンシング描画パスでは、インスタンシング用SRVはバインドしない（ルートパラメータ4は使用しない）。
    if (instSrv.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(4, instSrv);
    }

    // 描画コマンド
    DrawIndexedOrVertices(cmdList, owner, instanceCount, "Model::DrawInstanced");
}

/// <summary>
/// モデルファイルを読みこむ（頂点データとマテリアル情報を格納した独自フォーマットのファイルを想定）
/// </summary>
bool Model::LoadFromFile(const std::string& directoryPath, const std::string& filename)
{
    // Objファイルを読み込む
    modelData_ = Object3d::LoadModelFile(directoryPath, filename);
    return !modelData_.vertices.empty();
}

/// <summary>
/// モデル頂点データから頂点バッファを作成する
/// </summary>
void Model::CreateVertexBuffer()
{
    DeferReleaseResource(vertexResource_);
    vertexBufferView_ = {};

    if (!dxCommon_ || modelData_.vertices.empty()) {
        return;
    }

    DirectXCommon* dxCommon = dxCommon_; // GPUリソース生成元
    const size_t vertexBufferSize = sizeof(Object3d::VertexData) * modelData_.vertices.size(); // 頂点バッファサイズ
    vertexResource_ = dxCommon->CreateBufferResource(vertexBufferSize);

    void* mappedVertexData = nullptr; // 頂点データ転送先
    vertexResource_->Map(0, nullptr, &mappedVertexData);
    assert(mappedVertexData != nullptr);
    std::memcpy(mappedVertexData, modelData_.vertices.data(), vertexBufferSize);
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    vertexBufferView_.StrideInBytes = static_cast<UINT>(sizeof(Object3d::VertexData));
}


/// <summary>
/// モデルのIndexデータからIndexバッファを作成する
/// </summary>
void Model::CreateIndexBuffer()
{
    DeferReleaseResource(indexResource_);
    indexBufferView_ = {};

    if (!dxCommon_ || modelData_.indices.empty()) {
        return;
    }

    DirectXCommon* dxCommon = dxCommon_; // GPUリソース生成元
    const size_t indexBufferSize = sizeof(uint32_t) * modelData_.indices.size(); // Indexバッファサイズ
    indexResource_ = dxCommon->CreateBufferResource(indexBufferSize);

    void* mappedIndexData = nullptr; // Indexデータ転送先
    indexResource_->Map(0, nullptr, &mappedIndexData);
    assert(mappedIndexData != nullptr);
    std::memcpy(mappedIndexData, modelData_.indices.data(), indexBufferSize);
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(indexBufferSize);
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}
/// <summary>
/// Skinning用の頂点影響バッファを作成する
/// </summary>
void Model::CreateVertexInfluenceBuffer()
{
    DeferReleaseResource(vertexInfluenceResource_);
    vertexInfluenceBufferView_ = {};

    if (!dxCommon_) {
        return;
    }
    if (modelData_.vertexInfluences.empty() || modelData_.vertexInfluences.size() != modelData_.vertices.size()) {
        return;
    }

    DirectXCommon* dxCommon = dxCommon_; // GPUリソース生成元
    const size_t bufferSize = sizeof(Object3d::VertexInfluence) * modelData_.vertexInfluences.size(); // 影響情報バッファサイズ
    vertexInfluenceResource_ = dxCommon->CreateBufferResource(bufferSize);

    void* mappedData = nullptr; // 頂点影響情報の転送先
    vertexInfluenceResource_->Map(0, nullptr, &mappedData);
    assert(mappedData != nullptr);
    std::memcpy(mappedData, modelData_.vertexInfluences.data(), bufferSize);
    vertexInfluenceResource_->Unmap(0, nullptr);

    vertexInfluenceBufferView_.BufferLocation = vertexInfluenceResource_->GetGPUVirtualAddress();
    vertexInfluenceBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
    vertexInfluenceBufferView_.StrideInBytes = static_cast<UINT>(sizeof(Object3d::VertexInfluence));
}
/// <summary>
/// モデル内マテリアルごとのテクスチャ番号を初期化する。
/// </summary>
void Model::InitializeMaterialTextureIndices(TextureManager* textureManager)
{
    materialTextureIndices_.clear();
    if (!textureManager) {
        return;
    }

    materialTextureIndices_.reserve(modelData_.materials.size());
    for (const Object3d::MaterialData& materialData : modelData_.materials) {
        const uint32_t materialTextureIndex = ResolveModelMaterialTextureIndex(textureManager, materialData); // マテリアルごとのSRV番号
        materialTextureIndices_.push_back(materialTextureIndex);
    }
}
/// <summary>
/// モデル描画で使うGPUリソースとテクスチャ状態を初期化する。
/// </summary>
void Model::InitializeModelResources()
{
    auto textureManager = TextureManager::GetInstance(); // テクスチャ管理
    EnsureFallbackModelTextureLoaded(textureManager);
    textureIndex_ = UINT32_MAX;

    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateVertexInfluenceBuffer();

    const uint32_t materialTextureIndex = ResolveModelMaterialTextureIndex(textureManager, modelData_.material); // モデル代表マテリアルのSRV番号
    if (materialTextureIndex != UINT32_MAX) {
        textureIndex_ = materialTextureIndex;
    }
    InitializeMaterialTextureIndices(textureManager);
}

/// <summary>
/// モデルの初期化
/// </summary>
void Model::Initialize(ModelCommon* modelCommon)
{
    dxCommon_ = modelCommon ? modelCommon->GetDxCommon() : nullptr;
    // 前提条件のチェック
    if (modelData_.vertices.empty()) {
        return;
    }

    InitializeModelResources();
}
