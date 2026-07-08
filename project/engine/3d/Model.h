#pragma once

#include "Object3d.h"
#include <d3d12.h>
#include <vector>
#include <wrl.h>

namespace MyEngine {

// 前方宣言
class ModelCommon;
class Object3d;

/// <summary>
/// モデルクラス
/// </summary>
class Model {
public: // メンバ関数
    Model() = default; // デフォルトコンストラクタ
    ~Model() = default; // デストラクタ

    /// <summary>
    /// モデルファイルを読みこむ（頂点データとマテリアル情報を格納した独自フォーマットのファイルを想定）
    /// </summary>
    bool LoadFromFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>
    /// モデルの初期化
    /// </summary>
    void Initialize(ModelCommon* modelCommon);

    /// <summary>
    /// 描画
    /// </summary>
    void Draw(Object3d* owner);

    /// <summary>
    /// インスタンシング描画
    /// </summary>
    void DrawInstanced(Object3d* owner, uint32_t instanceCount);

private: // メンバ変数

    /// <summary>
    /// Object3d側で明示指定されたテクスチャ番号を取得する。
    /// </summary>
    uint32_t ResolveOwnerTextureOverrideIndex(const Object3d* owner) const;

    /// <summary>
    /// Model自身が保持しているテクスチャ番号を取得する。
    /// </summary>
    uint32_t ResolveModelTextureIndex() const;

    /// <summary>
    /// fallbackテクスチャのSRV番号を取得する。
    /// </summary>
    uint32_t ResolveFallbackTextureIndex() const;

    /// <summary>
    /// 描画時に使用するテクスチャ番号を決定する。
    /// </summary>
    uint32_t ResolveTextureIndex(const Object3d* owner) const;

    /// <summary>
    /// 描画時に使用する頂点データを取得する
    /// </summary>
    const std::vector<Object3d::VertexData>& ResolveDrawVertices(const Object3d* owner) const;

    /// <summary>
    /// 描画時に使用する頂点バッファビューを取得する
    /// </summary>
    D3D12_VERTEX_BUFFER_VIEW ResolveVertexBufferView(const Object3d* owner) const;

    /// <summary>
    /// モデル描画で使うGPUリソースとテクスチャ状態を初期化する。
    /// </summary>
    void InitializeModelResources();

    /// <summary>
    /// モデル頂点データから頂点バッファを作成する
    /// </summary>
    void CreateVertexBuffer();

    /// <summary>
    /// オーナーのマテリアルCBVを描画用ルートパラメータへ設定する
    /// </summary>
    bool BindOwnerMaterialResource(ID3D12GraphicsCommandList* commandList, const Object3d* owner, const char* logContext) const;

    /// <summary>
    /// 指定されたテクスチャ番号のSRVを描画用ルートパラメータへ設定する
    /// </summary>
    bool BindTexture(ID3D12GraphicsCommandList* commandList, uint32_t textureIndex, const char* logContext) const;


    // モデル共通情報へのポインタ
    ModelCommon* modelCommon_ = nullptr;

    // 読み込んだモデルの構造データ
    Object3d::ModelData modelData_;

    // GPU 上に配置される頂点バッファ用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // 頂点データ転送時に使用する中間バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_;
    // DirectX12 用の頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    // モデルファイル由来の既定テクスチャSRVインデックス
    uint32_t textureIndex_ = UINT32_MAX;
};

} // namespace MyEngine
