#include "Sprite.h"
#include "SpriteCommon.h"
#include "WinApp.h"
#include "mathUtility.h"
#include "TextureManager.h"
#include "Logger.h"
#include <cstdio>

using namespace MyEngine;

void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath)
{
    // 引数で受け取ってメンバ変数に記録する
    this->spriteCommon_ = spriteCommon;

    DirectXCommon* dxCommon = spriteCommon->GetDxCommon();
    HRESULT hr;
    MathUtility math;

    // トランスフォームの初期化
    this->transform_.scale = { 1.0f, 1.0f, 1.0f };
    this->transform_.rotate = { 0.0f, 0.0f, 0.0f };
    this->transform_.translate = { 0.0f, 0.0f, 0.0f };

    // UVトランスフォームの初期化
    this->uvTransform_.scale = { 1.0f, 1.0f, 1.0f };
    this->uvTransform_.rotate = { 0.0f, 0.0f, 0.0f };
    this->uvTransform_.translate = { 0.0f, 0.0f, 0.0f };

    // Sprite用の頂点リソースを作る
    // ローカル変数 -> メンバ変数への代入
    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);

    // 頂点バッファビューを作成し、メンバ変数に代入
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 頂点データ書き込み用ポインタを取得し、メンバ変数に代入
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    if (FAILED(hr)) {
        Logger::Log("Warning: Sprite::Initialize vertexResource_->Map failed. Vertex data will be unavailable\n");
        vertexData_ = nullptr;
    } else {
        // 頂点データの内容を書き込む
        vertexData_[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
        vertexData_[0].texcoord = { 0.0f, 1.0f };
        vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

        vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
        vertexData_[1].texcoord = { 0.0f, 0.0f };
        vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

        vertexData_[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
        vertexData_[2].texcoord = { 1.0f, 1.0f };
        vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

        vertexData_[3].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 右上
        vertexData_[3].texcoord = { 1.0f, 0.0f };
        vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };
    }

    // IndexResourceSpriteの生成
    // ローカル変数 -> メンバ変数への代入
    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

    // IndexBufferViewの生成
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // インデックスリソースにデータを書き込むポインタを取得し、メンバ変数に代入
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    if (FAILED(hr)) {
        Logger::Log("Warning: Sprite::Initialize indexResource_->Map failed. Index data will be unavailable\n");
        indexData_ = nullptr;
    } else {
        // インデックスデータの内容を書き込む
        indexData_[0] = 0;
        indexData_[1] = 1;
        indexData_[2] = 2;
        indexData_[3] = 1;
        indexData_[4] = 3;
        indexData_[5] = 2;
    }

    // Sprite用のマテリアルリソースを作る
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));

    // マテリアルにデータを書き込むポインタを取得し、メンバ変数に代入
    hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    if (FAILED(hr)) {
        Logger::Log("Warning: Sprite::Initialize materialResource_->Map failed. Material data will be unavailable\n");
        materialData_ = nullptr;
    } else {
        // マテリアルデータの内容を書き込む
        materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        materialData_->enableLighting = false; // SpriteはLightingしない
        materialData_->uvTransform = math.MakeIdentity4x4();
    }

    // Sprite用のTransformationMatrix用のリソースを作る
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    // データを書き込むポインタを取得し、メンバ変数に代入
    hr = transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    if (FAILED(hr)) {
        Logger::Log("Warning: Sprite::Initialize transformationMatrixResource_->Map failed. Transformation data will be unavailable\n");
        transformationMatrixData_ = nullptr;
    } else {
        // 変換行列データの内容を書き込む（初期値）
        transformationMatrixData_->WVP = math.MakeIdentity4x4();
        transformationMatrixData_->World = math.MakeIdentity4x4();
    }

    // 単位行列を書き込んでおく
    uint32_t idx = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
    if (idx == UINT32_MAX) {
        uint32_t loaded = TextureManager::GetInstance()->GetLoadedTextureCount();
        if (loaded > 0) {
            textureIndex_ = 0; // fallback to first texture
            char buf[256];
            sprintf_s(buf, "Warning: Sprite texture not found (%s). Falling back to index 0\n", textureFilePath.c_str());
            Logger::Log(buf);
        } else {
            textureIndex_ = UINT32_MAX; // no texture available
            char buf[256];
            sprintf_s(buf, "Warning: Sprite texture not found (%s) and no textures loaded\n", textureFilePath.c_str());
            Logger::Log(buf);
        }
    } else {
        textureIndex_ = idx;
    }
    // テクスチャサイズに合わせてスプライトのサイズを調整
    AdjustTextureSize();
}

void Sprite::Update()
{
    MathUtility math;

    // スケール（サイズ）を反映
    this->transform_.scale = { size_.x, size_.y, 1.0f };
    // 回転を反映
    this->transform_.rotate.z = rotation_;
    // 座標（平行移動）を反映
    this->transform_.translate = { position_.x, position_.y, 0.0f };

    // アンカーポイントの反映
    float left = 0.0f - anchorPoint_.x;
    float right = 1.0f - anchorPoint_.x;
    float top = 0.0f - anchorPoint_.y;
    float bottom = 1.0f - anchorPoint_.y;

    // 頂点位置を反映 (vertexData_ が有効なら)
    if (vertexData_) {
        vertexData_[0].position = { left, bottom, 0.0f, 1.0f }; // 左下
        vertexData_[1].position = { left, top, 0.0f, 1.0f }; // 左上
        vertexData_[2].position = { right, bottom, 0.0f, 1.0f }; // 右下
        vertexData_[3].position = { right, top, 0.0f, 1.0f }; // 右上
    }

    // テクスチャUV座標の計算
    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex_);
    float tex_left = textureLeftTop_.x / static_cast<float>(metadata.width);
    float tex_right = (textureLeftTop_.x + textureSize_.x) / static_cast<float>(metadata.width);
    float tex_top = textureLeftTop_.y / static_cast<float>(metadata.height);
    float tex_bottom = (textureLeftTop_.y + textureSize_.y) / static_cast<float>(metadata.height);

    // 頂点リソースにデータを書き込む
    float tleft = tex_left;
    float tright = tex_right;
    float ttop = tex_top;
    float tbottom = tex_bottom;

    // フリップ時はUVを入れ替える
    if (isFlipX_) std::swap(tleft, tright);
    if (isFlipY_) std::swap(ttop, tbottom);

    if (vertexData_) {
        vertexData_[0].texcoord = { tleft, tbottom }; // 左下
        vertexData_[1].texcoord = { tleft, ttop }; // 左上
        vertexData_[2].texcoord = { tright, tbottom }; // 右下
        vertexData_[3].texcoord = { tright, ttop }; // 右上
    }

    // World行列の作成
    // ここで this->transform_ の値が外部から更新されている必要がある
    Matrix4x4 worldMatrix = math.MakeAffineMatrix(this->transform_.scale, this->transform_.rotate, this->transform_.translate);

    // View/Projection行列の作成 (2Dスプライト用)
    Matrix4x4 viewMatrix = math.MakeIdentity4x4();
    Matrix4x4 projectionMatrix = math.MakeOrthograhicMatrix(0.0f, 0.0f, WinApp::kWindowWidth, WinApp::kWindowHeight, 0.0f, 100.0f);

    // WVP行列の計算と定数バッファへの書き込み
    Matrix4x4 wvpMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));

    // メンバ変数ポインタに書き込み
    if (transformationMatrixData_) {
        transformationMatrixData_->WVP = wvpMatrix;
        transformationMatrixData_->World = worldMatrix; // World行列も忘れずに更新
    } else {
        Logger::Log("Warning: Sprite::Update transformationMatrixData_ is null, skipping matrix update\n");
    }

    //  UV変換行列の計算と定数バッファへの書き込み
    Matrix4x4 uvTransformMatrix = math.MakeScaleMatrix(this->uvTransform_.scale);
    uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeRotateZMatrix(this->uvTransform_.rotate.z));
    uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeTranslateMatrix(this->uvTransform_.translate));

    // メンバ変数ポインタに書き込み
    if (materialData_) {
        materialData_->uvTransform = uvTransformMatrix;
    } else {
        Logger::Log("Warning: Sprite::Update materialData_ is null, skipping uvTransform update\n");
    }
}

void Sprite::Draw()
{
    if (!spriteCommon_) {
        Logger::Log("Warning: Sprite::Draw skipped: spriteCommon_ is null\n");
        return;
    }
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();
    if (!dxCommon || !dxCommon->GetCommandList()) {
        Logger::Log("Warning: Sprite::Draw skipped: DirectXCommon or command list is null\n");
        return;
    }

    // 必要なバッファ/リソースの存在確認
    if (!vertexData_) {
        Logger::Log("Warning: Sprite::Draw skipped: vertexData_ is null\n");
        return;
    }
    if (!indexData_) {
        Logger::Log("Warning: Sprite::Draw skipped: indexData_ is null\n");
        return;
    }
    if (!materialResource_) {
        Logger::Log("Warning: Sprite::Draw skipped: materialResource_ is null\n");
        return;
    }
    if (!transformationMatrixResource_) {
        Logger::Log("Warning: Sprite::Draw skipped: transformationMatrixResource_ is null\n");
        return;
    }

    // VBVを設定
    dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // IBVを設定
    dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

    // マテリアルCBufferの場所を指定
    dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // TransformationMatrixBufferの箇所を設定
    dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

    // SRVのDescriptorTableの先頭を指定（有効なtextureIndex_のときのみ）
    if (textureIndex_ != UINT32_MAX) {
        D3D12_GPU_DESCRIPTOR_HANDLE srv = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
        if (srv.ptr != 0) {
            dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, srv);
        } else {
            Logger::Log("Warning: Sprite::Draw texture SRV handle is null, skipping SRV bind\n");
        }
    } else {
        Logger::Log("Warning: Sprite::Draw has invalid textureIndex_, skipping SRV bind\n");
    }

    // 描画！(インデックス数6)
    dxCommon->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::AdjustTextureSize()
{
    // テクスチャメタデータを取得
    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex_);

    // テクスチャサイズを反映
    textureSize_.x = static_cast<float>(metadata.width);
    textureSize_.y = static_cast<float>(metadata.height);
    //画像サイズをテクスチャサイズに合わせつ
    size_ = textureSize_;
}
