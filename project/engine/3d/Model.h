#pragma once

#include "Object3d.h"
#include <d3d12.h>
#include <vector>
#include <wrl.h>

namespace MyEngine {

// 前方宣言
class ModelCommon;
class Object3d;

class Model {
public: // メンバ関数
    Model() = default;
    ~Model() = default;

    // モデルデータをファイルから読み込む
    bool LoadFromFile(const std::string& directoryPath, const std::string& filename);


    // 初期化
    void Initialize(ModelCommon* modelCommon);

    // 描画
    void Draw(Object3d* owner);
    // 描画（インスタンス数指定）
    void DrawInstanced(Object3d* owner, uint32_t instanceCount);

private: // メンバ変数
    // モデル共通情報へのポインタ
    ModelCommon* modelCommon_ = nullptr;

    // 読み込んだモデルの構造データ
    ModelData modelData_;

    // GPU 上に配置される頂点バッファ用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // 頂点データ転送時に使用する中間バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_;
    // DirectX12 用の頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    // マテリアル用定数バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    // マテリアルデータへの CPU 側ポインタ
    Object3d::Material* materialData_ = nullptr;
    // 使用するテクスチャのインデックス番号
    uint32_t textureIndex_ = 0;
};

} // namespace MyEngine
