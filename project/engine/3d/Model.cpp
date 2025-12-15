#include "Model.h"
#include "ModelCommon.h"
#include "Object3d.h"
#include "DirectXCommon.h"
#include "engine/2d/TextureManager.h"
#include "Logger.h"
#include "StringUtility.h"
#include "engine/utility/mathUtility.h"
#include <cassert>
#include <cstring>

using namespace MyEngine;

void Model::Draw(Object3d* owner)
{
    if (!modelCommon_ || !owner) return;

    auto cmdList = modelCommon_->GetDxCommon()->GetCommandList();

    // 頂点バッファの設定 (モデルまたはオーナーから)
    D3D12_VERTEX_BUFFER_VIEW vbv = vertexBufferView_.SizeInBytes != 0 ? vertexBufferView_ : owner->GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    // マテリアル定数バッファのCBV設定 (モデルまたはオーナーから)
    if (materialResource_) {
        cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    } else if (owner->GetMaterialResource()) {
        cmdList->SetGraphicsRootConstantBufferView(0, owner->GetMaterialResource()->GetGPUVirtualAddress());
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

    // 平行光源CBV設定 (オーナーから)
    if (owner->GetDirectionalLightResource()) {
        cmdList->SetGraphicsRootConstantBufferView(3, owner->GetDirectionalLightResource()->GetGPUVirtualAddress());
    } else {
        Logger::Log("Model::Draw skipped: directional light CBV missing\n");
        return;
    }

    // テクスチャSRV設定 (モデルまたはオーナーから)
    uint32_t texIndex = (textureIndex_ != UINT32_MAX) ? textureIndex_ : owner->GetModelData().material.textureIndex; // Updated texture index logic
    auto texMgr = TextureManager::GetInstance();
    if (texIndex != UINT32_MAX && texIndex < texMgr->GetLoadedTextureCount()) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        Logger::Log(std::format("Model::Draw: textureIndex={} srv.ptr=0x{:016X}\n", texIndex, srvHandle.ptr));
        cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
    } else {
        Logger::Log(std::format("Model::Draw: no valid texture assigned (index={}) - drawing without SRV\n", texIndex));
    }

    // 描画コマンド
    const auto& verts = modelData_.vertices.empty() ? owner->GetModelData().vertices : modelData_.vertices;
    if (verts.empty()) {
        Logger::Log("Model::Draw skipped: no vertices available\n");
        return;
    }
    cmdList->DrawInstanced(static_cast<UINT>(verts.size()), 1, 0, 0);
}

bool Model::LoadFromFile(const std::string& directoryPath, const std::string& filename)
{
    // Objファイルを読みこむ
    modelData_ = Object3d::LoadObjFile(directoryPath, filename);
    return !modelData_.vertices.empty();
}

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
        uint32_t idx = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        if (idx != UINT32_MAX) textureIndex_ = idx;
    }
}
