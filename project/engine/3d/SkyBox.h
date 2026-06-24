#pragma once
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include <array>
#include <d3d12.h>
#include <wrl.h>

namespace MyEngine {

// 前方宣言
class Camera;

/// <summary>
/// キューブマップテクスチャを使用して、シーンの背景に空や遠景を描画するためのクラス
/// </summary>
class SkyBox {
public: // メンバ関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    SkyBox() = default;

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~SkyBox();

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t srvIndex);

    /// <summary>
    /// 終了
    /// </summary>
    void Finalize();

    /// <summary>
    /// ビュープロジェクション行列の更新
    /// </summary>
    void UpdateViewProj(const float vp[16]);

    /// <summary>
    /// 描画
    /// </summary>
    void Draw(Camera* camera);

private: // メンバ変数
    // 初期化時に渡されるDirectXCommonとSrvManagerの参照を保持
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // パイプラインステートとルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 頂点バッファとインデックスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_ {};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_INDEX_BUFFER_VIEW ibView_ {};

    // 定数バッファ（ビュープロジェクション行列用）
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> constantBuffers_;
    std::array<uint8_t*, DirectXCommon::kFrameCount> mappedConstantBuffers_ {};
    std::array<float, 16> viewProjectionState_ {}; // CPU側で保持するビュー射影行列

    // 描画に使用するSRVのインデックス
    uint32_t indexCount_ = 0;
    uint32_t srvIndex_ = 0;
};
}
