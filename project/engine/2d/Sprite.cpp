#include "Sprite.h"
#include "SpriteCommon.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "../utility/ResourceResolver.h"
#include "Logger.h"
#include "TextureManager.h"
#include "mathUtility.h"
#include <cstdio>

using namespace MyEngine;

/// <summary>
/// SpriteCommonの初期化と、Sprite描画に必要なリソースの生成を行う
/// </summary>
void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, ImGuiManager* imguiManager)
{
    // 引数で受け取ってメンバ変数に記録する
    this->spriteCommon_ = spriteCommon;

    DirectXCommon* dxCommon = spriteCommon->GetDxCommon();

    HRESULT hr;

    // トランスフォームの初期化
    this->transform_.scale = { 1.0f, 1.0f, 1.0f };
    this->transform_.rotate = { 0.0f, 0.0f, 0.0f };
    this->transform_.translate = { 0.0f, 0.0f, 0.0f };

    // UVトランスフォームの初期化
    this->uvTransform_.scale = { 1.0f, 1.0f, 1.0f };
    this->uvTransform_.rotate = { 0.0f, 0.0f, 0.0f };
    this->uvTransform_.translate = { 0.0f, 0.0f, 0.0f };

    // フレームごとの頂点リソースを作成する
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        vertexResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(VertexData) * vertexState_.size());
        hr = vertexResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexData_[frameIndex]));
        if (FAILED(hr)) {
            Logger::Log("Warning: Sprite::Initialize vertex resource map failed\n");
            mappedVertexData_[frameIndex] = nullptr;
        }
    }

    vertexData_[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
    vertexData_[0].texcoord = { 0.0f, 1.0f };
    vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };
    vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[1].texcoord = { 0.0f, 0.0f };
    vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };
    vertexData_[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
    vertexData_[2].texcoord = { 1.0f, 1.0f };
    vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };
    vertexData_[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
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

    // フレームごとのマテリアルと変換行列リソースを作成する
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        materialResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(Material));
        materialResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterialData_[frameIndex]));
        transformationMatrixResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
        transformationMatrixResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransformationMatrixData_[frameIndex]));
    }

    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData_->enableLighting = false;
    materialData_->uvTransform = MathUtil::MakeIdentity4x4();
    materialData_->lightingMode = 0;
    materialData_->useAlphaCutoutSampler = 0;
    materialData_->shininess = 1.0f;
    transformationMatrixData_->WVP = MathUtil::MakeIdentity4x4();
    transformationMatrixData_->World = MathUtil::MakeIdentity4x4();
    // パス指定されたテクスチャをリソースリゾルバで解決してみる
    std::string resolvedTex = ResourceResolver::Resolve(textureFilePath, ResourceResolver::Type::Texture);
    // 無かったらそのままのパスにする
    if (!resolvedTex.empty()) {
        textureFilePath = resolvedTex;
    }

    uint32_t idx = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
    if (idx == UINT32_MAX) {
        // 指定が未ロード/不明なら既定のチェッカーテクスチャへフォールバックし、SRV絶対インデックスを使用
        uint32_t srvIdx = TextureManager::GetInstance()->GetSrvIndex("resources/uvChecker.png");

        // まだチェッカーテクスチャがロードされていない場合はロードしてSRVを確保
        if (srvIdx == UINT32_MAX) {
            TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
            TextureManager::GetInstance()->ReleaseIntermediateResources();
            srvIdx = TextureManager::GetInstance()->GetSrvIndex("resources/uvChecker.png");
        }

        // フォールバックのSRVインデックスを使用
        textureIndex_ = srvIdx;
        char buf[256];
        sprintf_s(buf, "Warning: Sprite texture not found (%s). Falling back to uvChecker srvIndex=%u\n", textureFilePath.c_str(), srvIdx);
        Logger::Log(buf);

    } else {
        // 指定されたテクスチャがロードされている場合はそのSRVインデックスを使用
        textureIndex_ = idx;
    }

    // テクスチャサイズに合わせてスプライトのサイズを調整
    AdjustTextureSize();
    // ImGui parameter is accepted for compatibility but UI is handled centrally
    (void)imguiManager;
}

Sprite::~Sprite()
{
    // no ImGui unregister needed; registration is centralized
}

/// <summary>
/// スプライトのプロパティを編集する関数
/// </summary>
void Sprite::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Sprite");
    Vector2 pos = GetPosition();
    if (ImGui::DragFloat2("Position", &pos.x, 0.1f)) {
        SetPosition(pos);
    }

    float rot = GetRotation();
    if (ImGui::DragFloat("Rotation", &rot, 0.01f)) {
        SetRotation(rot);
    }

    Vector4 col = GetColor();
    if (ImGui::ColorEdit4("Color", &col.x)) {
        SetColor(col);
    }

    Vector2 size = GetSize();
    if (ImGui::DragFloat2("Size", &size.x, 0.1f)) {
        SetSize(size);
    }

    Vector2 anchor = GetAnchorPoint();
    if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
        SetAnchorPoint(anchor);
    }

    bool fx = GetIsFlipX();
    bool fy = GetIsFlipY();

    if (ImGui::Checkbox("FlipX", &fx)) {
        SetIsFlipX(fx);
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("FlipY", &fy)) {
        SetIsFlipY(fy);
    }

    Vector2 texLT = GetTextureLeftTop();
    Vector2 texSize = GetTextureSize();

    if (ImGui::DragFloat2("Tex LeftTop", &texLT.x, 1.0f)) {
        SetTextureLeftTop(texLT);
    }

    if (ImGui::DragFloat2("Tex Size", &texSize.x, 1.0f, 1.0f, 8192.0f)) {
        SetTextureSize(texSize);
    }

    static char texBuf[256] = "";
    ImGui::InputText("Texture Path", texBuf, sizeof(texBuf));
    if (ImGui::Button("Apply Texture")) {
        SetTexture(std::string(texBuf));
    }

#else
    (void)0;
#endif
}

/// <summary>
/// スプライトの状態を反映して、頂点データや変換行列を更新
/// </summary>
void Sprite::Update()
{
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
    if (isFlipX_) {
        std::swap(tleft, tright);
    }

    // フリップ時はUVを入れ替える
    if (isFlipY_) {
        std::swap(ttop, tbottom);
    }

    // UV座標を反映 (vertexData_ が有効なら)
    if (vertexData_) {
        vertexData_[0].texcoord = { tleft, tbottom }; // 左下
        vertexData_[1].texcoord = { tleft, ttop }; // 左上
        vertexData_[2].texcoord = { tright, tbottom }; // 右下
        vertexData_[3].texcoord = { tright, ttop }; // 右上
    }

    // World行列の作成
    // ここで this->transform_ の値が外部から更新されている必要がある
    Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(this->transform_.scale, this->transform_.rotate, this->transform_.translate);

    // View/Projection行列の作成（2Dスプライト用）
    Matrix4x4 viewMatrix = MathUtil::MakeIdentity4x4();
    float renderWidth = 1.0f; // 現在の描画幅
    float renderHeight = 1.0f; // 現在の描画高さ
    if (spriteCommon_ && spriteCommon_->GetDxCommon()) {
        renderWidth = spriteCommon_->GetDxCommon()->GetRenderWidth();
        renderHeight = spriteCommon_->GetDxCommon()->GetRenderHeight();
    }
    if (renderWidth <= 0.0f) {
        renderWidth = 1.0f;
    }
    if (renderHeight <= 0.0f) {
        renderHeight = 1.0f;
    }
    Matrix4x4 projectionMatrix = MathUtil::MakeOrthographicMatrix(0.0f, 0.0f, renderWidth, renderHeight, 0.0f, 100.0f);

    // WVP行列の計算と定数バッファへの書き込み
    Matrix4x4 wvpMatrix = MathUtil::Multiply(worldMatrix, MathUtil::Multiply(viewMatrix, projectionMatrix));

    // メンバ変数ポインタに書き込み
    if (transformationMatrixData_) {
        transformationMatrixData_->WVP = wvpMatrix;
        transformationMatrixData_->World = worldMatrix; // World行列も忘れずに更新
    } else {
        // 変換行列データが利用できない場合は警告を出す（描画は続行するが、スプライトは正しく表示されない可能性がある）
        Logger::Log("Warning: Sprite::Update transformationMatrixData_ is null, skipping matrix update\n");
    }

    //  UV変換行列の計算と定数バッファへの書き込み
    Matrix4x4 uvTransformMatrix = MathUtil::MakeScaleMatrix(this->uvTransform_.scale);
    uvTransformMatrix = MathUtil::Multiply(uvTransformMatrix, MathUtil::MakeRotateZMatrix(this->uvTransform_.rotate.z));
    uvTransformMatrix = MathUtil::Multiply(uvTransformMatrix, MathUtil::MakeTranslateMatrix(this->uvTransform_.translate));

    // メンバ変数ポインタに書き込み
    if (materialData_) {
        materialData_->uvTransform = uvTransformMatrix;
    } else {
        // UV変換行列データが利用できない場合は警告を出す（描画は続行するが、UV変換が反映されない可能性がある）
        Logger::Log("Warning: Sprite::Update materialData_ is null, skipping uvTransform update\n");
    }
}

/// <summary>
/// 頂点バッファビュー、インデックスバッファビュー、マテリアル定数バッファ、変換行列定数バッファ、SRVをコマンドリストにセットして描画
/// </summary>
void Sprite::Draw()
{
    UpdateFrameResources();
    // DirectXCommonとコマンドリストの存在確認
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
        // 頂点データが利用できない場合は警告を出して描画をスキップする（頂点データがないと描画できないため）
        Logger::Log("Warning: Sprite::Draw skipped: vertexData_ is null\n");
        return;
    }

    if (!indexData_) {
        // インデックスデータが利用できない場合は警告を出して描画をスキップする（インデックスデータがないと描画できないため）
        Logger::Log("Warning: Sprite::Draw skipped: indexData_ is null\n");
        return;
    }

    if (!materialResources_[dxCommon->GetCurrentFrameIndex()]) {
        // マテリアルリソースが利用できない場合は警告を出して描画をスキップする（マテリアル定数バッファがないと描画できないため）
        Logger::Log("Warning: Sprite::Draw skipped: materialResource_ is null\n");
        return;
    }

    if (!transformationMatrixResources_[dxCommon->GetCurrentFrameIndex()]) {
        // 変換行列リソースが利用できない場合は警告を出して描画をスキップする（変換行列定数バッファがないと描画できないため）
        Logger::Log("Warning: Sprite::Draw skipped: transformationMatrixResource_ is null\n");
        return;
    }

    // VBVを設定
    const uint32_t frameIndex = dxCommon->GetCurrentFrameIndex(); // 描画対象フレーム番号
    vertexBufferView_.BufferLocation = vertexResources_[frameIndex]->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertexState_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // IBVを設定
    dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

    // マテリアルCBufferの場所を指定
    dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResources_[frameIndex]->GetGPUVirtualAddress());

    // TransformationMatrixBufferの箇所を設定
    dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResources_[frameIndex]->GetGPUVirtualAddress());

    // SRVのDescriptorTableの先頭を指定（有効なtextureIndex_のときのみ）
    if (textureIndex_ != UINT32_MAX) {
        D3D12_GPU_DESCRIPTOR_HANDLE srv = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
        // SRVハンドルが有効なときのみバインドする。無効なときは警告を出すが描画は続行する（SRVがないと描画できないわけではないため）
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

/// <summary>
/// テクスチャサイズをイメージに合わせる
/// </summary>
void Sprite::AdjustTextureSize()
{
    // テクスチャメタデータを取得
    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex_);

    // テクスチャサイズを反映
    textureSize_.x = static_cast<float>(metadata.width);
    textureSize_.y = static_cast<float>(metadata.height);
    // 画像サイズをテクスチャサイズに合わせつ
    size_ = textureSize_;
}

/// <summary>
/// ロード済みテクスチャをスプライトへ設定する
/// </summary>
void Sprite::SetTexture(const std::string& filePath)
{

    auto texMgr = TextureManager::GetInstance(); // テクスチャマネージャーのインスタンスを取得
    // 登録済みテクスチャのSRVインデックスを取得する
    uint32_t idx = texMgr->GetTextureIndexByFilePath(filePath);
    // 未ロードの場合は現在のテクスチャを維持する
    if (idx == UINT32_MAX) {
        Logger::Log(std::string("Sprite::SetTexture: texture is not loaded: ") + filePath + "\n");
        return;
    }

    // テクスチャと表示サイズを更新する
    textureIndex_ = idx;
    AdjustTextureSize();
}

/// <summary>
/// 現在のフレーム用GPUバッファへCPU側の状態を転送する
/// </summary>
void Sprite::UpdateFrameResources()
{
    if (!spriteCommon_ || !spriteCommon_->GetDxCommon()) {
        return;
    }

    const uint32_t frameIndex = spriteCommon_->GetDxCommon()->GetCurrentFrameIndex(); // 転送先フレーム番号
    if (mappedVertexData_[frameIndex]) {
        memcpy(mappedVertexData_[frameIndex], vertexState_.data(), sizeof(VertexData) * vertexState_.size());
    }
    if (mappedMaterialData_[frameIndex]) {
        *mappedMaterialData_[frameIndex] = materialState_;
    }
    if (mappedTransformationMatrixData_[frameIndex]) {
        *mappedTransformationMatrixData_[frameIndex] = transformationMatrixState_;
    }
}