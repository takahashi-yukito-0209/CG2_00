#pragma once
#include "mathUtility.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

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

namespace MyEngine {

// 前方宣言
class SpriteCommon;

class Sprite {

public: // メンバ関数
    // 初期化
    void Initialize(SpriteCommon* spriteCommon);
    // 更新
    void Update();
    // 描画
    void Draw(const D3D12_GPU_DESCRIPTOR_HANDLE& textureSrvHandleGPU);

private: // メンバ変数
    SpriteCommon* spriteCommon_ = nullptr;

    // バッファリソース
    ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    ComPtr<ID3D12Resource> indexResource_ = nullptr;
    ComPtr<ID3D12Resource> materialResource_ = nullptr;
    ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;

    // バッファリソース内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    Material* materialData_ = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    // バッファリソースの使い道を補足するバッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

    // スプライトの変換情報
    Transform transform_ = {};
    // UVテクスチャ変換情報
    Transform uvTransform_ = {};
};

} // namespace MyEngine