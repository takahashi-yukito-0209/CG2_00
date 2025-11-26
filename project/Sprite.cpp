#include "Sprite.h"
#include "SpriteCommon.h"
#include "mathUtility.h"

using namespace MyEngine;

void Sprite::Initialize(SpriteCommon* spriteCommon)
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
    assert(SUCCEEDED(hr));

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

    // IndexResourceSpriteの生成
    // ローカル変数 -> メンバ変数への代入
    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

    // IndexBufferViewの生成
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // インデックスリソースにデータを書き込むポインタを取得し、メンバ変数に代入
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    assert(SUCCEEDED(hr));

    // インデックスデータの内容を書き込む
    indexData_[0] = 0;
    indexData_[1] = 1;
    indexData_[2] = 2;
    indexData_[3] = 1;
    indexData_[4] = 3;
    indexData_[5] = 2;

    // Sprite用のマテリアルリソースを作る
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));

    // マテリアルにデータを書き込むポインタを取得し、メンバ変数に代入
    hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    assert(SUCCEEDED(hr));

    // マテリアルデータの内容を書き込む
    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData_->enableLighting = false; // SpriteはLightingしない
    materialData_->uvTransform = math.MakeIdentity4x4();

    // Sprite用のTransformationMatrix用のリソースを作る
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    // データを書き込むポインタを取得し、メンバ変数に代入
    hr = transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    assert(SUCCEEDED(hr));

    // 変換行列データの内容を書き込む（初期値）
    transformationMatrixData_->WVP = math.MakeIdentity4x4();
    transformationMatrixData_->World = math.MakeIdentity4x4();
}

void Sprite::Update()
{
    MathUtility math;

    // World行列の作成
    // ここで this->transform_ の値が外部から更新されている必要がある
    Matrix4x4 worldMatrix = math.MakeAffineMatrix(this->transform_.scale, this->transform_.rotate, this->transform_.translate);

    // View/Projection行列の作成 (2Dスプライト用)
    Matrix4x4 viewMatrix = math.MakeIdentity4x4();
    // 画面サイズはSpriteCommonやWindowAppから取得するのが理想
    const float kWindowWidth = 1280.0f; // 仮の値
    const float kWindowHeight = 720.0f; // 仮の値
    Matrix4x4 projectionMatrix = math.MakeOrthograhicMatrix(0.0f, 0.0f, kWindowWidth, kWindowHeight, 0.0f, 100.0f);

    // WVP行列の計算と定数バッファへの書き込み
    Matrix4x4 wvpMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));

    // メンバ変数ポインタに書き込み
    transformationMatrixData_->WVP = wvpMatrix;
    transformationMatrixData_->World = worldMatrix; // World行列も忘れずに更新

    //  UV変換行列の計算と定数バッファへの書き込み
    Matrix4x4 uvTransformMatrix = math.MakeScaleMatrix(this->uvTransform_.scale);
    uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeRotateZMatrix(this->uvTransform_.rotate.z));
    uvTransformMatrix = math.Multiply(uvTransformMatrix, math.MakeTranslateMatrix(this->uvTransform_.translate));

    // メンバ変数ポインタに書き込み
    materialData_->uvTransform = uvTransformMatrix;
}

void Sprite::Draw(const D3D12_GPU_DESCRIPTOR_HANDLE& textureSrvHandleGPU)
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    // VBVを設定
    dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // IBVを設定
    dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

    // マテリアルCBufferの場所を指定
    dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // TransformationMatrixBufferの箇所を設定
    dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

    // SRVのDescriptorTableの先頭を指定
    dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

    // 描画！(インデックス数6)
    dxCommon->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
