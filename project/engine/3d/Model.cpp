#include "Model.h"
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
        Logger::Log("Model::Draw skipped: DirectXCommon unavailable\n");
        return;
    }

    // コマンドリストの取得
    auto cmdList = dxCommon->GetCommandList();
    // コマンドリストが取得できない場合は描画できない
    if (!cmdList) {
        Logger::Log("Model::Draw skipped: command list is null\n");
        return;
    }

    // 頂点バッファの設定 (モデルまたはオーナーから)
    D3D12_VERTEX_BUFFER_VIEW vbv = vertexBufferView_.SizeInBytes != 0 ? vertexBufferView_ : owner->GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    // マテリアル定数バッファのCBV設定 (モデルまたはオーナーから)
    if (materialResource_) {
        // モデルがマテリアルリソースを持っている場合はそれを優先して使用
        auto addr = materialResource_->GetGPUVirtualAddress();
        // アドレスが0の場合は描画できない
        if (addr == 0) {
            Logger::Log("Model::Draw: materialResource_ GPU address is 0 - skipping\n");
            return;
        }
        // ルートパラメータ0にマテリアルCBVをバインド
        cmdList->SetGraphicsRootConstantBufferView(0, addr);

    } else if (owner->GetMaterialResource()) {
        // モデルがマテリアルリソースを持っていない場合はオーナーのマテリアルリソースを使用
        auto res = owner->GetMaterialResource();
        // オーナーのマテリアルリソースが有効でない場合は描画できない
        if (!res) {
            Logger::Log("Model::Draw: owner material resource null - skipping\n");
            return;
        }

        // オーナーのマテリアルリソースのGPUアドレスが0の場合は描画できない
        auto addr = res->GetGPUVirtualAddress();
        // アドレスが0の場合は描画できない
        if (addr == 0) {
            Logger::Log("Model::Draw: owner material GPU address is 0 - skipping\n");
            return;
        }

        // ルートパラメータ0にオーナーのマテリアルCBVをバインド
        cmdList->SetGraphicsRootConstantBufferView(0, addr);

    } else {
        // モデルもオーナーもマテリアルリソースを持っていない場合は描画できない
        Logger::Log("Model::Draw skipped: material CBV missing\n");
        return;
    }

    // 座標変換行列CBV設定 (オーナーから)
    if (owner->GetTransformationMatrixResource()) {
        // オーナーが座標変換行列リソースを持っている場合はそれを使用
        cmdList->SetGraphicsRootConstantBufferView(1, owner->GetTransformationMatrixResource()->GetGPUVirtualAddress());
    } else {
        // オーナーが座標変換行列リソースを持っていない場合は描画できない
        Logger::Log("Model::Draw skipped: transformation matrix CBV missing\n");
        return;
    }

    // 平行光源CBV設定 (shared in Object3dCommon)
    Object3dCommon* common = owner->GetObject3dCommon();
    // Object3dCommonが有効でない場合は描画できない
    if (!common) {
        Logger::Log("Model::Draw skipped: missing Object3dCommon\n");
        return;
    }

    // 平行光源のGPUアドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = common->GetDirectionalLightGPUAddress();
    // 平行光源のGPUアドレスが0の場合は描画できない
    if (lightAddr == 0) {
        Logger::Log("Model::Draw skipped: directional light CBV missing\n");
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
    uint32_t texIndex = UINT32_MAX;
    // オーナーのマテリアル情報を確認してテクスチャインデックスを取得
    const auto& ownerMat = owner->GetModelData().material;
    // オーナーが明示的にテクスチャファイルパスを指定している場合のみ、オーナーのテクスチャインデックスを優先して使用
    if (!ownerMat.textureFilePath.empty()) {
        // オーナーが明示的にテクスチャを指定している場合のみオーナーの index を優先
        if (ownerMat.textureIndex != UINT32_MAX) {
            texIndex = ownerMat.textureIndex;
        }
    } else if (textureIndex_ != UINT32_MAX) {
        // 通常はモデル側のテクスチャを使う
        texIndex = textureIndex_;
    }

    // テクスチャインデックスが有効な場合はSRVをバインド、無効な場合はフォールバックのチェッカーテクスチャを使用
    auto texMgr = TextureManager::GetInstance();
    // テクスチャインデックスが有効な場合はSRVをバインド
    if (texIndex != UINT32_MAX) {
        // テクスチャインデックスが有効な場合はSRVをバインド
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        // SRVハンドルが無効な場合はSRVをバインドせずに描画する（テクスチャが存在しない可能性があるため）
        if (srvHandle.ptr == 0) {
            char buf[256];
            sprintf_s(buf, "Model::Draw: srv handle for index %u is null - skipping SRV\n", texIndex);
            Logger::Log(buf);
        } else {
            cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        }

    } else {
        // フォールバック: 既定のチェッカーテクスチャを使用（SRV絶対インデックス）
        uint32_t fallbackIdx = texMgr->GetSrvIndex(kFallbackModelTexturePath);
        // フォールバックのチェッカーテクスチャのSRVインデックスを取得
        if (fallbackIdx == UINT32_MAX) {
            Logger::Log("Model::Draw: fallback texture is not loaded - skipping SRV bind\n");
            return;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(fallbackIdx);
        // フォールバックのSRVハンドルを取得
        if (srvHandle.ptr == 0) {
            Logger::Log("Model::Draw: fallback srv handle is null - skipping SRV bind\n");
        } else {
            cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        }
    }

    // スタンス描画パスでは、インスタンシング用SRVはバインドしない（ルートパラメータ4は使用しない）。
    const auto& verts = modelData_.vertices.empty() ? owner->GetModelData().vertices : modelData_.vertices;
    // 頂点データはモデル側が空の場合はオーナー側の頂点データを使用
    if (verts.empty()) {
        Logger::Log("Model::Draw skipped: no vertices available\n");
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
    D3D12_VERTEX_BUFFER_VIEW vbv = vertexBufferView_.SizeInBytes != 0 ? vertexBufferView_ : owner->GetVertexBufferView();
    // モデル側の頂点バッファが有効であればそれを使用、無ければオーナー側の頂点バッファを使用
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    if (materialResource_) {
        // モデル側のマテリアルリソースが有効であればそれを優先して使用
        cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    } else if (owner->GetMaterialResource()) {
        // モデル側のマテリアルリソースが無ければオーナー側のマテリアルリソースを使用
        cmdList->SetGraphicsRootConstantBufferView(0, owner->GetMaterialResource()->GetGPUVirtualAddress());

    } else {
        // オーナーのマテリアルリソースが有効でない場合は描画できない
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

    uint32_t texIndex = UINT32_MAX;
    const auto& ownerMat2 = owner->GetModelData().material;
    // オーナーのマテリアル情報を確認してテクスチャインデックスを取得
    if (!ownerMat2.textureFilePath.empty()) {
        if (ownerMat2.textureIndex != UINT32_MAX) {
            texIndex = ownerMat2.textureIndex;
        }
    } else if (textureIndex_ != UINT32_MAX) {
        texIndex = textureIndex_;
    }

    auto texMgr = TextureManager::GetInstance();
    // テクスチャインデックスが有効な場合はSRVをバインド、無効な場合はフォールバックのチェッカーテクスチャを使用
    if (texIndex != UINT32_MAX) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        if (srvHandle.ptr != 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        } else {
            // フォールバック
            uint32_t fb = texMgr->GetSrvIndex(kFallbackModelTexturePath);
            if (fb == UINT32_MAX) {
                Logger::Log("Model::DrawInstanced: fallback texture is not loaded - skipping SRV bind\n");
                return;
            }

            D3D12_GPU_DESCRIPTOR_HANDLE fbHandle = texMgr->GetSrvHandleGPU(fb);
            if (fbHandle.ptr != 0) {
                cmdList->SetGraphicsRootDescriptorTable(2, fbHandle);
            }
        }
    } else {
        uint32_t fb = texMgr->GetSrvIndex(kFallbackModelTexturePath);
        if (fb == UINT32_MAX) {
            Logger::Log("Model::DrawInstanced: fallback texture is not loaded - skipping SRV bind\n");
            return;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE fbHandle = texMgr->GetSrvHandleGPU(fb);
        if (fbHandle.ptr != 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, fbHandle);
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE instSrv = common->GetInstancingSrvGPUHandle();
    // インスタンシング描画パスでは、インスタンシング用SRVはバインドしない（ルートパラメータ4は使用しない）。
    if (instSrv.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(4, instSrv);
    }

    const auto& verts = modelData_.vertices.empty() ? owner->GetModelData().vertices : modelData_.vertices;
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
/// モデルの初期化
/// </summary>
void Model::Initialize(ModelCommon* modelCommon)
{
    modelCommon_ = modelCommon;
    // 前提条件のチェック
    if (modelData_.vertices.empty()) {
        return;
    }

    // 頂点バッファの生成
    auto texMgr = TextureManager::GetInstance();
    uint32_t fallbackIdx = texMgr->GetSrvIndex(kFallbackModelTexturePath);
    if (fallbackIdx == UINT32_MAX) {
        texMgr->LoadTexture(kFallbackModelTexturePath);
        texMgr->ExecuteResourceUpload();
    }

    DirectXCommon* dx = modelCommon_->GetDxCommon();
    size_t vbSize = sizeof(Object3d::VertexData) * modelData_.vertices.size();
    vertexResource_ = dx->CreateBufferResource(vbSize);
    // 頂点データ転送
    void* mapped = nullptr;
    vertexResource_->Map(0, nullptr, &mapped);
    assert(mapped != nullptr);
    std::memcpy(mapped, modelData_.vertices.data(), vbSize);
    vertexResource_->Unmap(0, nullptr);

    // 頂点バッファビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vbSize);
    vertexBufferView_.StrideInBytes = static_cast<UINT>(sizeof(Object3d::VertexData));

    // マテリアル用定数バッファの生成
    if (!modelData_.material.textureFilePath.empty()) {
        uint32_t idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        if (idx == UINT32_MAX) {
            // テクスチャがまだロードされていなければ読み込んでアップロードする
            texMgr->LoadTexture(modelData_.material.textureFilePath);
            texMgr->ExecuteResourceUpload();
            idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        }

        if (idx != UINT32_MAX) {
            textureIndex_ = idx;
        }
    }
}
