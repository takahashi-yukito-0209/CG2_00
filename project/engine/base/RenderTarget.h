#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

namespace MyEngine {
// 前方宣言
class DirectXCommon;

/// <summary>
/// レンダーターゲットクラス
/// </summary>
class RenderTarget {
public: // メンバ関数

    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    RenderTarget() = default;

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~RenderTarget();

    /// <summary>
    /// レンダーターゲットの生成
    /// </summary>
    bool Create(DirectXCommon* dxCommon, uint32_t width, uint32_t height, DXGI_FORMAT format);
    
    /// <summary>
    /// レンダーターゲットの解放
    /// </summary>
    void Release();

    /// <summary>
    /// レンダーターゲットのSRVを作成してディスクリプタヒープに登録する
    /// </summary>
    void CreateShaderResourceView(uint32_t srvIndex);

    /// <summary>
    /// レンダーターゲットのRTVを作成してディスクリプタヒープに登録する
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
    
    /// <summary>
    /// レンダーターゲットのリソースを取得する
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> const& GetResource() const { return resource_; }

private: // メンバ変数

    // DirectXCommon クラスのポインタ（リソース生成やコマンドリストへのアクセスに使用）
    DirectXCommon* dxCommon_ = nullptr;
    // レンダーターゲットのリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    // レンダーターゲットのSRV用ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    // レンダーターゲットのRTV用CPUディスクリプタハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};

};

} // namespace MyEngine
