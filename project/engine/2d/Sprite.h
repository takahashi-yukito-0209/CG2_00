#pragma once
#include <cstdint>
#include <MathTypes.h>
#include <d3d12.h>
#include <string>
#include <wrl.h>

using namespace Math;

namespace MyEngine {

// 前方宣言
class SpriteCommon;

class Sprite {

public: // メンバ構造体
    // 頂点データ構造体
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // マテリアル構造体
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding1[3];
        Matrix4x4 uvTransform;
        int lightingMode;
        float padding2[3];
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

public: // メンバ関数
    // 初期化
    void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath);
    // 更新
    void Update();
    // 描画
    void Draw();

    // ゲッター (取得関数)
    const Vector2& GetPosition() const { return position_; }
    float GetRotation() const { return rotation_; }
    const Vector4& GetColor() const { return materialData_->color; }
    const Vector2& GetSize() const { return size_; }
    const Vector2& GetAnchorPoint() const { return anchorPoint_; }
    const bool& GetIsFlipX() const { return isFlipX_; }
    const bool& GetIsFlipY() const { return isFlipY_; }
    const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
    const Vector2& GetTextureSize() const { return textureSize_; }

    // セッター (設定関数)
    void SetPosition(const Vector2& position) { position_ = position; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetColor(const Vector4& color) { materialData_->color = color; }
    void SetSize(const Vector2& size) { size_ = size; }
    void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
    void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
    void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
    void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }
    void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

private: // メンバ関数
    // テクスチャサイズをイメージに合わせる
    void AdjustTextureSize();

private: // メンバ変数
    SpriteCommon* spriteCommon_ = nullptr;

    // バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;

    // バッファリソース内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    Material* materialData_ = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    // バッファリソースの使い道を補足するバッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

    // テクスチャ番号
    uint32_t textureIndex_ = 0;

    // スプライトの変換情報
    Transform transform_ = {};
    // UVテクスチャ変換情報
    Transform uvTransform_ = {};

    // 座標
    Vector2 position_ = { 0.0f, 0.0f };
    // 回転
    float rotation_ = 0.0f;
    // サイズ
    Vector2 size_ = { 1.0f, 1.0f };
    // アンカーポイント
    Vector2 anchorPoint_ = { 0.0f, 0.0f };
    // 左右フリップ
    bool isFlipX_ = false;
    // 上下フリップ
    bool isFlipY_ = false;
    // テクスチャ左上座標
    Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    // テクスチャ切り出しサイズ
    Vector2 textureSize_ = { 100.0f, 100.0f };
};

} // namespace MyEngine