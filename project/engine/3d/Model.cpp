#include "Model.h"
#include "ModelCommon.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "Logger.h"
#include "StringUtility.h"
#include "mathUtility.h"
#include <cassert>
#include <cstring>

using namespace MyEngine;

void Model::Draw(Object3d* owner)
{
    if (!modelCommon_ || !owner) return;

    if (!modelCommon_->GetDxCommon()) {
        Logger::Log("Model::Draw skipped: modelCommon_->GetDxCommon() is null\n");
        return;
    }

    auto cmdList = modelCommon_->GetDxCommon()->GetCommandList();
    if (!cmdList) {
        Logger::Log("Model::Draw skipped: command list is null from modelCommon_->GetDxCommon()\n");
        return;
    }

    // 頂点バッファの設定 (モデルまたはオーナーから)
    D3D12_VERTEX_BUFFER_VIEW vbv = vertexBufferView_.SizeInBytes != 0 ? vertexBufferView_ : owner->GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    // マテリアル定数バッファのCBV設定 (モデルまたはオーナーから)
    if (materialResource_) {
        auto addr = materialResource_->GetGPUVirtualAddress();
        if (addr == 0) { Logger::Log("Model::Draw: materialResource_ GPU address is 0 - skipping\n"); return; }
        cmdList->SetGraphicsRootConstantBufferView(0, addr);
    } else if (owner->GetMaterialResource()) {
        auto res = owner->GetMaterialResource();
        if (!res) { Logger::Log("Model::Draw: owner material resource null - skipping\n"); return; }
        auto addr = res->GetGPUVirtualAddress();
        if (addr == 0) { Logger::Log("Model::Draw: owner material GPU address is 0 - skipping\n"); return; }
        cmdList->SetGraphicsRootConstantBufferView(0, addr);
    } else {
        Logger::Log("Model::Draw skipped: material CBV missing\n");
        return;
    }

    // 座標変換行列CBV設定 (オーナーから)
    if (owner->GetTransformationMatrixResource()) {
        cmdList->SetGraphicsRootConstantBufferView(1, owner->GetTransformationMatrixResource()->GetGPUVirtualAddress());
    } else {
        Logger::Log("Model::Draw skipped: transformation matrix CBV missing\n");
        return;
    }

    // 平行光源CBV設定 (shared in Object3dCommon)
    Object3dCommon* common = owner->GetObject3dCommon();
    if (!common) {
        Logger::Log("Model::Draw skipped: missing Object3dCommon\n");
        return;
    }
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = common->GetDirectionalLightGPUAddress();
    if (lightAddr == 0) {
        Logger::Log("Model::Draw skipped: directional light CBV missing\n");
        return;
    }
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

    // camera CBV
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = common->GetCameraGPUAddress();
    if (camAddr != 0) {
        cmdList->SetGraphicsRootConstantBufferView(6, camAddr);
    }

    // Non-instanced path: do not bind instancing SRV (root param 4) to avoid unintended descriptor table conflicts.

    // テクスチャSRV設定 (オーナー設定を最優先、無ければモデルの設定)
    uint32_t texIndex = UINT32_MAX;
    const auto& ownerMat = owner->GetModelData().material;
    if (!ownerMat.textureFilePath.empty()) {
        // オーナーが明示的にテクスチャを指定している場合のみオーナーの index を優先
        if (ownerMat.textureIndex != UINT32_MAX) {
            texIndex = ownerMat.textureIndex;
        }
    } else if (textureIndex_ != UINT32_MAX) {
        // 通常はモデル側のテクスチャを使う
        texIndex = textureIndex_;
    }
    auto texMgr = TextureManager::GetInstance();
    uint32_t loadedCount = texMgr->GetLoadedTextureCount();
    if (texIndex != UINT32_MAX && texIndex < loadedCount) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        if (srvHandle.ptr == 0) {
            char buf[256]; sprintf_s(buf, "Model::Draw: srv handle for index %u is null - skipping SRV\n", texIndex);
            Logger::Log(buf);
        } else {
        {
            char buf[256];
            sprintf_s(buf, "Model::Draw: textureIndex=%u srv.ptr=0x%016llX\n", texIndex, static_cast<unsigned long long>(srvHandle.ptr));
            Logger::Log(buf);
        }
        cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        }
    } else if (loadedCount > 0) {
        // シェーダーが有効な SRV を持つようにデフォルトのテクスチャ（インデックス0）にフォールバックする
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(0);
        if (srvHandle.ptr == 0) {
            Logger::Log("Model::Draw: fallback srv handle is null - skipping SRV bind\n");
        } else {
            char buf[256]; sprintf_s(buf, "Model::Draw: using fallback texture index=0 srv.ptr=0x%016llX\n", static_cast<unsigned long long>(srvHandle.ptr));
            Logger::Log(buf);
            cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        }
    } else {
        {
            char buf[256];
            sprintf_s(buf, "Model::Draw: no textures loaded at all (index=%u) - drawing without SRV\n", texIndex);
            Logger::Log(buf);
        }
    }

    // 描画コマンド
    const auto& verts = modelData_.vertices.empty() ? owner->GetModelData().vertices : modelData_.vertices;
    if (verts.empty()) {
        Logger::Log("Model::Draw skipped: no vertices available\n");
        return;
    }
    cmdList->DrawInstanced(static_cast<UINT>(verts.size()), 1, 0, 0);
}

void Model::DrawInstanced(Object3d* owner, uint32_t instanceCount)
{
    if (!modelCommon_ || !owner) return;
    auto cmdList = modelCommon_->GetDxCommon()->GetCommandList();
    if (!cmdList) return;

    // set VB
    D3D12_VERTEX_BUFFER_VIEW vbv = vertexBufferView_.SizeInBytes != 0 ? vertexBufferView_ : owner->GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    // material CBV
    if (materialResource_) {
        cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    } else if (owner->GetMaterialResource()) {
        cmdList->SetGraphicsRootConstantBufferView(0, owner->GetMaterialResource()->GetGPUVirtualAddress());
    } else return;

    // transformation (owner's CBV is unused when using instancing)
    if (owner->GetTransformationMatrixResource()) {
        cmdList->SetGraphicsRootConstantBufferView(1, owner->GetTransformationMatrixResource()->GetGPUVirtualAddress());
    }

    // light
    Object3dCommon* common = owner->GetObject3dCommon();
    if (!common) return;
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = common->GetDirectionalLightGPUAddress();
    if (lightAddr == 0) return;
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = common->GetCameraGPUAddress();
    if (camAddr != 0) { cmdList->SetGraphicsRootConstantBufferView(6, camAddr); }

    // texture: owner override > model texture
    uint32_t texIndex = UINT32_MAX;
    const auto& ownerMat2 = owner->GetModelData().material;
    if (!ownerMat2.textureFilePath.empty()) {
        if (ownerMat2.textureIndex != UINT32_MAX) {
            texIndex = ownerMat2.textureIndex;
        }
    } else if (textureIndex_ != UINT32_MAX) {
        texIndex = textureIndex_;
    }
    auto texMgr = TextureManager::GetInstance();
    if (texIndex != UINT32_MAX && texIndex < texMgr->GetLoadedTextureCount()) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        if (srvHandle.ptr != 0) cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
    }

    // instancing SRV
    D3D12_GPU_DESCRIPTOR_HANDLE instSrv = common->GetInstancingSrvGPUHandle();
    if (instSrv.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(4, instSrv);
    }

    const auto& verts = modelData_.vertices.empty() ? owner->GetModelData().vertices : modelData_.vertices;
    if (verts.empty()) return;
    cmdList->DrawInstanced(static_cast<UINT>(verts.size()), instanceCount, 0, 0);
}

bool Model::LoadFromFile(const std::string& directoryPath, const std::string& filename)
{
    // Objファイルを読みこむ
    modelData_ = Object3d::LoadObjFile(directoryPath, filename);
    return !modelData_.vertices.empty();
}

// CreateFence は削除済み — 手続き的フォールバックはもはや使用されない。

void Model::Initialize(ModelCommon* modelCommon)
{
    modelCommon_ = modelCommon;
    if (modelData_.vertices.empty()) return;

    // 頂点バッファの生成
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
        auto texMgr = TextureManager::GetInstance();
        uint32_t idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        if (idx == UINT32_MAX) {
            // テクスチャがまだロードされていなければ読み込んでアップロードする
            texMgr->LoadTexture(modelData_.material.textureFilePath);
            texMgr->ExecuteResourceUpload();
            idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        }
        if (idx != UINT32_MAX) textureIndex_ = idx;
    }
}
