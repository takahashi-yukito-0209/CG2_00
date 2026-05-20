#include "RenderTarget.h"
#include "DirectXCommon.h"
#include "Logger.h"

using namespace MyEngine;
using namespace Microsoft::WRL;

/// <summary>
/// デストラクタ
/// </summary>
RenderTarget::~RenderTarget()
{
    // レンダーターゲットのリソースとディスクリプタヒープを解放
    Release();
}

/// <summary>
/// レンダーターゲットの生成
/// </summary>
bool RenderTarget::Create(DirectXCommon* dxCommon, uint32_t width, uint32_t height, DXGI_FORMAT format)
{
    // DirectXCommon クラスのポインタが有効か確認
    if (!dxCommon) {
        return false;
    }

    // DirectXCommon クラスのポインタを保存
    dxCommon_ = dxCommon;

    // レンダーターゲット用のテクスチャリソースを作成
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // ヒーププロパティを設定（GPU専用のデフォルトヒープ）
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // リソースを作成
    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&resource_));

    // リソースの作成に失敗した場合はエラーログを出力して終了
    if (FAILED(hr)) {
        Logger::Log("RenderTarget::Create: CreateCommittedResource failed\n");
        return false;
    }

    // レンダーターゲット用のRTVディスクリプタヒープを作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_));

    // ディスクリプタヒープの作成に失敗した場合はエラーログを出力してリソースを解放して終了
    if (FAILED(hr)) {
        Logger::Log("RenderTarget::Create: CreateDescriptorHeap(RTV) failed\n");
        resource_.Reset();
        return false;
    }

    // RTVを作成してディスクリプタヒープに登録
    rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    // RTVのディスクリプタを設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    dxCommon_->GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    // 成功
    return true;
}

/// <summary>
/// レンダーターゲットの解放
/// </summary>
void RenderTarget::Release()
{
    // レンダーターゲットのリソースを解放
    if (resource_) {
        resource_.Reset();
    }

    // RTVディスクリプタヒープを解放
    if (rtvHeap_) {
        rtvHeap_.Reset();
    }

    // DirectXCommon クラスのポインタをリセット
    dxCommon_ = nullptr;
}

/// <summary>
/// レンダーターゲットのSRVを作成してディスクリプタヒープに登録する
/// </summary>
void RenderTarget::CreateShaderResourceView(uint32_t srvIndex)
{
    // DirectXCommon クラスのポインタとリソースが有効か確認
    if (!dxCommon_ || !resource_) {
        return;
    }

    // SRVのディスクリプタを設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = resource_->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    // SRVを作成してディスクリプタヒープに登録
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    dxCommon_->GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc, cpuHandle);
}
