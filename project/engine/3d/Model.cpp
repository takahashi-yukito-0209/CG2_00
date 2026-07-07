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
/// 描画時に使用するテクスチャ番号を決定する
/// </summary>
uint32_t Model::ResolveTextureIndex(const Object3d* owner) const
{
    if (owner) {
        const auto& ownerMaterial = owner->GetModelData().material; // Object3dで明示指定されたマテリアル
        const bool hasOwnerTextureOverride = !ownerMaterial.textureFilePath.empty() && ownerMaterial.textureIndex != UINT32_MAX; // Object3d側の明示テクスチャが有効か
        if (hasOwnerTextureOverride) {
            return ownerMaterial.textureIndex;
        }
    }

    if (textureIndex_ != UINT32_MAX) {
        return textureIndex_;
    }

    auto textureManager = TextureManager::GetInstance(); // fallbackテクスチャの取得元
    if (!textureManager) {
        return UINT32_MAX;
    }
    return textureManager->GetSrvIndex(kFallbackModelTexturePath);
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
    } else if (modelCommon_ && modelCommon_->GetDxCommon()) {
        // フォールバック: モデル初期化時に渡された ModelCommon を使う
        dxCommon = modelCommon_->GetDxCommon();
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

    // 頂点バッファの設定 (モデルまたはオーナーから)
    D3D12_VERTEX_BUFFER_VIEW vbv = ResolveVertexBufferView(owner); // 描画に使う頂点バッファビュー
    cmdList->IASetVertexBuffers(0, 1, &vbv);

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

    // 平行光源CBV設定 (shared in Object3dCommon)
    Object3dCommon* common = owner->GetObject3dCommon();
    // Object3dCommonが有効でない場合は描画できない
    if (!common) {
        Logger::Debug("Model::Draw skipped: missing Object3dCommon\n");
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

    // テクスチャSRV設定 (オーナー設定を最優先、無ければモデルの設定)
    const uint32_t textureIndex = ResolveTextureIndex(owner); // 描画に使うSRV番号
    BindTexture(cmdList, textureIndex, "Model::Draw");

    const auto& verts = ResolveDrawVertices(owner); // 描画に使う頂点データ
    if (verts.empty()) {
        Logger::Debug("Model::Draw skipped: no vertices available\n");
        return;
    }
    cmdList->DrawInstanced(static_cast<UINT>(verts.size()), 1, 0, 0);
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
    } else if (modelCommon_ && modelCommon_->GetDxCommon()) {
        // フォールバック: モデル初期化時に渡された ModelCommon から DirectXCommon を取得
        dxCommon = modelCommon_->GetDxCommon();
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

    const uint32_t textureIndex = ResolveTextureIndex(owner); // 描画に使うSRV番号
    BindTexture(cmdList, textureIndex, "Model::DrawInstanced");

    D3D12_GPU_DESCRIPTOR_HANDLE instSrv = common->GetInstancingSrvGPUHandle();
    // インスタンシング描画パスでは、インスタンシング用SRVはバインドしない（ルートパラメータ4は使用しない）。
    if (instSrv.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(4, instSrv);
    }

    const auto& verts = ResolveDrawVertices(owner); // 描画に使う頂点データ
    if (verts.empty()) {
        return;
    }
    // 描画コマンド
    cmdList->DrawInstanced(static_cast<UINT>(verts.size()), instanceCount, 0, 0);
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
    DirectXCommon* dxCommon = modelCommon_->GetDxCommon(); // GPUリソース生成元
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
/// モデルの初期化
/// </summary>
void Model::Initialize(ModelCommon* modelCommon)
{
    modelCommon_ = modelCommon;
    // 前提条件のチェック
    if (modelData_.vertices.empty()) {
        return;
    }

    // テクスチャ管理の取得とfallbackテクスチャの読み込み
    auto texMgr = TextureManager::GetInstance(); // テクスチャ管理
    EnsureFallbackModelTextureLoaded(texMgr);

    // 頂点バッファの生成
    CreateVertexBuffer();

    // モデルマテリアルテクスチャの解決
    const uint32_t materialTextureIndex = ResolveModelMaterialTextureIndex(texMgr, modelData_.material); // モデルマテリアルのSRV番号
    if (materialTextureIndex != UINT32_MAX) {
        textureIndex_ = materialTextureIndex;
    }
}
