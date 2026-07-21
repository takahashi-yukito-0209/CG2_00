#include "DirectXCommon.h"
#include "engine/utility/DebugUtility.h"
#include "engine/utility/ResourceResolver.h"
#include "Logger.h"
#include "StringUtility.h"
#include "WinApp.h"

#include <cassert>
#include <comdef.h> // _com_error 用
#include <d3d12sdklayers.h>
#include <externals/DirectXTex/d3dx12.h>

#include "SrvManager.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <array>
#include <thread>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;
using namespace MyEngine;

// 最大SRV数を定義
const uint32_t DirectXCommon::kMaxSRVCount = 512;

// レンダーターゲットの内部構造体定義
namespace MyEngine {
struct RenderTargetInternal {
    Microsoft::WRL::ComPtr<ID3D12Resource> colorResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthResource;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap; // per-RT RTV heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap; // per-RT DSV heap (optional)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle {};
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t width = 0;
    uint32_t height = 0;
    bool useDepth = false;
    bool resizeWithWindow = false;
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES depthCurrentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    std::array<float, 4> clearColor { 0.0f, 0.0f, 0.0f, 1.0f };
    // SRVヒープ上のインデックス（存在しない場合は UINT32_MAX）
    uint32_t srvIndex = UINT32_MAX;
    uint32_t depthSrvIndex = UINT32_MAX;
};
} // namespace MyEngine

namespace {
/// <summary>
/// オフスクリーン用カラーバッファのリソース設定を作成する。
/// </summary>
D3D12_RESOURCE_DESC CreateRenderTargetResourceDesc(uint32_t width, uint32_t height, DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC desc = {}; // カラーバッファ用リソース設定
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    return desc;
}

/// <summary>
/// GPU用デフォルトヒープの設定を作成する。
/// </summary>
D3D12_HEAP_PROPERTIES CreateDefaultHeapProperties()
{
    D3D12_HEAP_PROPERTIES heapProps = {}; // GPU用デフォルトヒープ設定
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;
    return heapProps;
}

/// <summary>
/// カラーバッファのクリア値を作成する。
/// </summary>
D3D12_CLEAR_VALUE CreateRenderTargetClearValue(DXGI_FORMAT format, const std::array<float, 4>& clearColor)
{
    D3D12_CLEAR_VALUE clearValue = {}; // カラーバッファ用クリア値
    clearValue.Format = format;
    clearValue.Color[0] = clearColor[0];
    clearValue.Color[1] = clearColor[1];
    clearValue.Color[2] = clearColor[2];
    clearValue.Color[3] = clearColor[3];
    return clearValue;
}

/// <summary>
/// 深度ステンシルバッファのリソース設定を作成する。
/// </summary>
D3D12_RESOURCE_DESC CreateDepthStencilResourceDesc(uint32_t width, uint32_t height)
{
    D3D12_RESOURCE_DESC desc = {}; // 深度ステンシル用リソース設定
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    return desc;
}

/// <summary>
/// 深度ステンシルバッファのクリア値を作成する。
/// </summary>
D3D12_CLEAR_VALUE CreateDepthStencilClearValue()
{
    D3D12_CLEAR_VALUE clearValue = {}; // 深度ステンシル用クリア値
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    return clearValue;
}
} // namespace

DirectXCommon::~DirectXCommon() = default;

// ----------------------------------------------------------------------
// Static メンバ関数の実装
// ----------------------------------------------------------------------

// ディスクリプタハンドルの取得用の静的関数の実装
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
}

void DirectXCommon::SetOnResizeCallback(const std::function<void(uint32_t, uint32_t)>& cb)
{
    onResizeCallback_ = cb;
}

/// <summary>
/// ウィンドウリサイズ通知 (デフォルト実装はファイル下部にある実装を使用)
/// </summary>
// (OnWindowResize implementation is defined later in this file)

// SrvManager の登録実装
void DirectXCommon::SetSrvManager(SrvManager* mgr)
{
    srvManager_ = mgr;
}

// ディスクリプタハンドルの取得用の静的関数の実装
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
}

/// <summary>
/// シングルトンインスタンスの取得
/// - 戻り値: DirectXCommon のシングルトンインスタンスへのポインタ
/// </summary>
DirectXCommon* DirectXCommon::GetInstance()
{
    // ローカル静的変数としてシングルトンインスタンスを定義
    static DirectXCommon instance;
    return &instance;
}

/// <summary>
/// 終了処理: フェンスイベントのクローズとシングルトン解放
/// </summary>
void DirectXCommon::Finalize()
{

    // GPU上のコマンドが完了するのを待ってからリソースを破棄する
    // これによりドライバ側のバックグラウンドスレッドが終了するまで待機し
    // DXGIのReportoiveObjectsで未解放オブジェクトが残る問題を軽減する
    if (commandQueue_ && fence_ && fenceEvent_) {
        // シグナル値をインクリメントしてGPUにシグナル
        fenceValue_++;
        HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
        if (SUCCEEDED(hr)) {
            if (fence_->GetCompletedValue() < fenceValue_) {
                hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
                if (SUCCEEDED(hr)) {
                    WaitForSingleObject(fenceEvent_, INFINITE);
                }
            }
        }
    }

    // 明示的にComPtrをリセットして参照カウントを減らす
    // コマンド周り
    if (commandList_) {
        commandList_.Reset();
    }

    for (auto& commandAllocator : commandAllocators_) {
        commandAllocator.Reset();
    }

    if (commandQueue_) {
        commandQueue_.Reset();
    }

    // スワップチェーン関連
    if (swapChain_) {
        swapChain_.Reset();
    }

    for (auto& res : swapChainResources_) {
        if (res) {
            res.Reset();
        }
    }

    // リソース/ヒープ類
    if (rtvDescriptorHeap_) {
        rtvDescriptorHeap_.Reset();
    }

    if (srvDescriptorHeap_) {
        srvDescriptorHeap_.Reset();
    }

    if (dsvDescriptorHeap_) {
        dsvDescriptorHeap_.Reset();
    }

    if (depthStencilResource_) {
        depthStencilResource_.Reset();
    }

    // フェンス/イベント
    if (fence_) {
        fence_.Reset();
    }

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    // コンパイラ/ファクトリ/デバイス
    if (dxcCompiler_) {
        dxcCompiler_.Reset();
    }

    if (dxcUtils_) {
        dxcUtils_.Reset();
    }

    if (includeHandler_) {
        includeHandler_.Reset();
    }

    if (device_) {
        device_.Reset();
    }

    if (dxgiFactory_) {
        dxgiFactory_.Reset();
    }
}

/// <summary>
/// コマンドリストを GPU に送信して実行する
/// </summary>
void DirectXCommon::ExecuteCommandList()
{
    // GPUコマンドの実行
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
}

/// <summary>
/// GPU のコマンド完了を待機する（フェンス同期）
/// </summary>
void DirectXCommon::WaitForCommandExecution()
{

    // Fenceの値を更新し、シグナルを送る
    fenceValue_++;
    HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::WaitForCommandExecution: Signal failed.")) {
        char buf[256];
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::WaitForCommandExecution: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Error(std::string(buf));
        }
        return;
    }

    // コマンド完了待ち (GPU同期)
    if (fence_->GetCompletedValue() < fenceValue_) {
        // GPUの処理完了時にイベントを通知するように設定
        HRESULT hr2 = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        if (!MYENGINE_CHECK_HRESULT(hr2, "DirectXCommon::WaitForCommandExecution: SetEventOnCompletion failed.")) {
            char buf[256];
            if (device_) {
                HRESULT reason = device_->GetDeviceRemovedReason();
                sprintf_s(buf, "DirectXCommon::WaitForCommandExecution: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
                Logger::Error(std::string(buf));
            }
            return;
        }
        // イベントが発生するまで待機
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

/// <summary>
/// コマンドアロケータとコマンドリストをリセットする
/// </summary>
void DirectXCommon::ResetCommandList()
{
    const uint32_t frameIndex = GetCurrentFrameIndex(); // リセット対象のフレーム番号


    // コマンドアロケータをリセット
    HRESULT hr = commandAllocators_[frameIndex]->Reset();
    if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::ResetCommandList: commandAllocator_->Reset failed.")) {
        char buf[256];
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::ResetCommandList: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Error(std::string(buf));
        }
        return;
    }

    // コマンドリストをリセット（アロケータを再設定）
    // 第二引数（PipelineStateObject）はnullでOK
    hr = commandList_->Reset(commandAllocators_[frameIndex].Get(), nullptr);
    if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::ResetCommandList: commandList_->Reset failed.")) {
        char buf[256];
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::ResetCommandList: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Error(std::string(buf));
        }
        return;
    }
}

// ----------------------------------------------------------------------
// Public メンバ関数の実装
// ----------------------------------------------------------------------

// SRV特化型Getterの実装
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) const
{
    return GetCPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

// SRV特化型Getterの実装
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) const
{
    return GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

// DSVヒープの先頭CPUディスクリプタハンドルを取得
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVHandle() const
{
    // DSVヒープが存在しない場合は無効なハンドルを返す
    if (!dsvDescriptorHeap_) {
        D3D12_CPU_DESCRIPTOR_HANDLE h {};
        h.ptr = 0;
        return h;
    }
    return dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
}

/// <summary>
/// 指定したサイズとフォーマットでオフスクリーンのレンダーターゲットを作成し、管理リストに追加する
/// </summary>
int DirectXCommon::CreateRenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format, bool useDepth,
    const std::array<float, 4>& clearColor, bool resizeWithWindow)
{
    // パラメータチェック
    auto rt = std::make_unique<RenderTargetInternal>();
    rt->width = width;
    rt->height = height;
    rt->format = format;
    rt->useDepth = useDepth;
    rt->resizeWithWindow = resizeWithWindow;

    // レンダーターゲット用のリソース設定を作成
    D3D12_RESOURCE_DESC desc = CreateRenderTargetResourceDesc(width, height, format);
    D3D12_HEAP_PROPERTIES heapProps = CreateDefaultHeapProperties();
    D3D12_CLEAR_VALUE clearValue = CreateRenderTargetClearValue(format, clearColor);

    // リソースの生成
    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&rt->colorResource));

    if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::CreateRenderTarget: Create color resource failed.")) {
        return -1;
    }

    // 初期状態はレンダーターゲットとして設定
    rt->currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // store clear color for later ClearRenderTargetView calls
    rt->clearColor = clearColor;

    // レンダーターゲット用のテクスチャリソースができたので、RTVヒープとRTVを作成して関連付けるレンダーターゲット
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = 1;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device_->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rt->rtvHeap));
    if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::CreateRenderTarget: Create RTV heap failed.")) {
        return -1;
    }
    rt->rtvHandle = rt->rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // RTVの設定と作成
    D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc = {};
    rtvViewDesc.Format = format;
    rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device_->CreateRenderTargetView(rt->colorResource.Get(), &rtvViewDesc, rt->rtvHandle);

    if (useDepth) {
        // 深度ステンシル用のリソース設定を作成
        D3D12_RESOURCE_DESC ddesc = CreateDepthStencilResourceDesc(width, height);
        D3D12_CLEAR_VALUE dclear = CreateDepthStencilClearValue();

        // 深度ステンシルバッファのリソースを生成
        hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &ddesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dclear, IID_PPV_ARGS(&rt->depthResource));
        if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::CreateRenderTarget: Create depth resource failed.")) {
            return -1;
        }

        // DSVヒープとDSVの作成
        D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
        dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvDesc.NumDescriptors = 1;
        dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = device_->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&rt->dsvHeap));
        if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::CreateRenderTarget: Create DSV heap failed.")) {
            return -1;
        }

        // DSVの設定と作成
        rt->dsvHandle = rt->dsvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvView = {};
        dsvView.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device_->CreateDepthStencilView(rt->depthResource.Get(), &dsvView, rt->dsvHandle);
    }

    // 管理リストに追加してハンドル（インデックス）を返す
    int idx = static_cast<int>(renderTargets_.size());
    renderTargets_.push_back(std::move(rt));
    return idx;
}

/// <summary>
/// 指定したハンドルのレンダーターゲットを破棄する
/// </summary>
void DirectXCommon::DestroyRenderTarget(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return;
    }
    // SRV が割り当てられている場合は SrvManager に解放を依頼する
    auto& rt = renderTargets_[handle];
    if (rt) {
        if (rt->srvIndex != UINT32_MAX && srvManager_) {
            srvManager_->Free(rt->srvIndex);
            rt->srvIndex = UINT32_MAX;
        }
        if (rt->depthSrvIndex != UINT32_MAX && srvManager_) {
            srvManager_->Free(rt->depthSrvIndex);
            rt->depthSrvIndex = UINT32_MAX;
        }
    }
    renderTargets_[handle].reset();
}

/// <summary>
/// 管理中のオフスクリーンレンダーターゲットをすべて破棄する
/// </summary>
void DirectXCommon::DestroyAllRenderTargets()
{
    for (size_t i = 0; i < renderTargets_.size(); ++i) {
        DestroyRenderTarget(static_cast<int>(i));
    }
    renderTargets_.clear();
}

/// <summary>
/// 指定したハンドルのレンダーターゲットを新しいサイズにリサイズする
/// </summary>
void DirectXCommon::ResizeRenderTarget(int handle, uint32_t width, uint32_t height)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return;
    }

    // 既存のレンダーターゲットを取得
    auto& rt = renderTargets_[handle];
    if (!rt) {
        return;
    }

    if (width == 0 || height == 0) {
        return;
    }

    WaitForCommandExecution();

    // 現在の設定を保存しておく
    DXGI_FORMAT fmt = rt->format;
    bool useDepth = rt->useDepth;
    bool resizeWithWindow = rt->resizeWithWindow;
    uint32_t oldSrv = rt->srvIndex;
    uint32_t oldDepthSrv = rt->depthSrvIndex;
    std::array<float, 4> clearColor = rt->clearColor;

    int newIdx = CreateRenderTarget(width, height, fmt, useDepth, clearColor, resizeWithWindow);
    if (newIdx < 0) {
        return;
    }

    // 既存のSRV番号を維持し、リサイズ後のカラーバッファへ張り替える
    if (oldSrv != UINT32_MAX) {
        CreateRenderTargetSRV(newIdx, oldSrv);
    }
    if (oldDepthSrv != UINT32_MAX) {
        CreateRenderTargetDepthSRV(newIdx, oldDepthSrv);
    }

    // 新しいレンダーターゲットが作成できたら、管理リスト内で入れ替える
    if (newIdx != handle) {
        renderTargets_[handle].swap(renderTargets_[newIdx]);
        renderTargets_[newIdx].reset();
    }
}

/// <summary>
/// 指定したハンドルのレンダーターゲットのカラーテクスチャに対してSRVを作成し、グローバルSRVヒープの指定されたインデックスに配置する
/// </summary>
uint32_t DirectXCommon::CreateRenderTargetSRV(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return UINT32_MAX;
    }

    auto& rt = renderTargets_[handle]; // SRVを作成するRT
    if (!rt || !rt->colorResource || !srvManager_ || !srvManager_->CanAllocate()) {
        return UINT32_MAX;
    }

    uint32_t srvIndex = srvManager_->Allocate(); // DirectXCommon側で確保したSRV番号
    CreateRenderTargetSRV(handle, srvIndex);
    return srvIndex;
}

void DirectXCommon::CreateRenderTargetSRV(int handle, uint32_t srvIndex)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return;
    }
    // レンダーターゲットを取得
    auto& rt = renderTargets_[handle];
    if (!rt || !rt->colorResource) {
        return;
    }

    // SRVの設定と作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = rt->colorResource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    // グローバルSRVヒープの指定されたインデックスにSRVを作成
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = GetSRVCPUDescriptorHandle(srvIndex);
    device_->CreateShaderResourceView(rt->colorResource.Get(), &srvDesc, cpu);
    // 保存しておく
    rt->srvIndex = srvIndex;
}

/// <summary>
/// オフスクリーン深度バッファのSRVを生成する
/// </summary>
uint32_t DirectXCommon::CreateRenderTargetDepthSRV(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return UINT32_MAX;
    }

    auto& rt = renderTargets_[handle]; // 深度SRVを作成するRT
    if (!rt || !rt->depthResource || !srvManager_ || !srvManager_->CanAllocate()) {
        return UINT32_MAX;
    }

    uint32_t srvIndex = srvManager_->Allocate(); // DirectXCommon側で確保したSRV番号
    CreateRenderTargetDepthSRV(handle, srvIndex);
    return srvIndex;
}

void DirectXCommon::CreateRenderTargetDepthSRV(int handle, uint32_t srvIndex)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return;
    }

    auto& rt = renderTargets_[handle]; // SRVを生成するレンダーターゲット
    if (!rt || !rt->depthResource) {
        return;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // 深度読み取り用SRV設定
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = GetSRVCPUDescriptorHandle(srvIndex); // SRVの配置先
    device_->CreateShaderResourceView(
        rt->depthResource.Get(),
        &srvDesc,
        cpuHandle);
    rt->depthSrvIndex = srvIndex;
}

/// <summary>
/// 指定したハンドルのレンダーターゲットのRTVとDSVのCPUディスクリプタハンドルを取得する
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRenderTargetRTV(int handle) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h {};
    h.ptr = 0;
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return h;
    }

    // レンダーターゲットを取得
    auto& rt = renderTargets_[handle];
    if (!rt) {
        return h;
    }

    return rt->rtvHandle;
}

/// <summary>
/// 指定したハンドルのレンダーターゲットのDSVのCPUディスクリプタハンドルを取得する
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRenderTargetDSV(int handle) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h {};
    h.ptr = 0;
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return h;
    }

    // レンダーターゲットを取得
    auto& rt = renderTargets_[handle];
    if (!rt) {
        return h;
    }

    return rt->dsvHandle;
}

/// <summary>
/// 指定したハンドルのレンダーターゲットを描画対象として設定する
/// </summary>
void DirectXCommon::BeginRenderTo(int handle, bool clear)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return;
    }

    // レンダーターゲットを取得
    auto& rt = renderTargets_[handle];
    if (!rt) {
        return;
    }

    // コマンドリストを取得
    auto cmd = commandList_.Get();
    // レンダーターゲットのカラーテクスチャをレンダーターゲット状態に遷移させる
    if (rt->currentState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = rt->colorResource.Get();
        barrier.Transition.StateBefore = rt->currentState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &barrier);
        rt->currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    if (rt->useDepth
        && rt->depthCurrentState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        D3D12_RESOURCE_BARRIER depthBarrier = {}; // 深度書き込み状態への遷移
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Transition.pResource = rt->depthResource.Get();
        depthBarrier.Transition.StateBefore = rt->depthCurrentState;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &depthBarrier);
        rt->depthCurrentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    // レンダーターゲットのRTVとDSVを設定する
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = rt->dsvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rt->rtvHandle;

    // 深度バッファを使用する場合はDSVもセット、そうでない場合はDSVはnullでOMSetRenderTargetsを呼び出す
    if (rt->useDepth) {
        cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    } else {
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }

    // ビューポートをRTサイズに合わせる
    D3D12_VIEWPORT vp {};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(rt->width);
    vp.Height = static_cast<float>(rt->height);
    vp.MinDepth = 0;
    vp.MaxDepth = 1;

    // シザー矩形も同様にRTサイズに合わせる
    D3D12_RECT sc {};
    sc.left = 0;
    sc.top = 0;
    sc.right = rt->width;
    sc.bottom = rt->height;

    // ビューポートとシザー矩形を設定
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    // クリアフラグが立っている場合は、指定したクリアカラーでRTVをクリアし、必要に応じてDSVもクリアする
    if (clear) {
        float clearCol[4] = { rt->clearColor[0], rt->clearColor[1], rt->clearColor[2], rt->clearColor[3] };
        cmd->ClearRenderTargetView(rt->rtvHandle, clearCol, 0, nullptr);
        if (rt->useDepth) {
            cmd->ClearDepthStencilView(rt->dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }
    }
}

/// <summary>
/// 描画対象をデフォルトのスワップチェーンのバックバッファに戻す
/// </summary>
void DirectXCommon::EndRenderTo(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= renderTargets_.size()) {
        return;
    }

    // レンダーターゲットを取得
    auto& rt = renderTargets_[handle];
    if (!rt) {
        return;
    }

    // コマンドリストを取得
    auto cmd = commandList_.Get();

    // レンダーターゲットのカラーテクスチャをピクセルシェーダーリソース状態に遷移させる
    if (rt->currentState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = rt->colorResource.Get();
        barrier.Transition.StateBefore = rt->currentState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &barrier);
        rt->currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    if (rt->useDepth
        && rt->depthSrvIndex != UINT32_MAX
        && rt->depthCurrentState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER depthBarrier = {}; // 深度読み取り状態への遷移
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Transition.pResource = rt->depthResource.Get();
        depthBarrier.Transition.StateBefore = rt->depthCurrentState;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &depthBarrier);
        rt->depthCurrentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    // スワップチェーンのバックバッファに描画対象を戻す
    if (swapChain_) {
        UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
        cmd->OMSetRenderTargets(1, &rtvHandles_[bbIndex], FALSE, &dsvHandle);
        // ビューポートとシザー矩形もスワップチェーンのサイズに合わせて設定し直す
        cmd->RSSetViewports(1, &viewport_);
        cmd->RSSetScissorRects(1, &scissorRect_);
    }
}

/// <summary>
/// 全体の初期化フローを実行する
/// </summary>
void DirectXCommon::Initialize(WinApp* winApp)
{
    assert(winApp);
    // 引数のWinAppポインタをメンバ変数に保存
    this->winApp_ = winApp;

    // FPS固定初期化
    InitializeFixFPS();

    // デバイスの生成
    CreateDevice();
    // コマンド関連の生成
    InitCommandRelated();
    // スワップチェーンの生成
    CreateSwapChain();
    // 深度バッファの生成
    CreateDepthBuffer();
    // 各種ディスクリプタヒープの生成
    CreateDescriptorHeaps();
    // レンダーターゲットビューの初期化
    InitRenderTargetView();
    // 深度ステンシルビューの初期化
    InitDepthStencilView();
    // フェンスの生成
    CreateFence();
    // ビューポート矩形の初期化
    InitViewport();
    // シザリング矩形の生成
    InitScissorRect();
    // DXCコンパイラの生成
    CreateDxcCompiler();
    // ImGuiの初期化
    InitImGui();
}

/// <summary>
/// 描画前処理を行う（リソースバリア、クリア、RTV/DSV設定等）
/// </summary>
void DirectXCommon::PreDraw()
{
    if (!swapChain_) {
        Logger::Warn("DirectXCommon::PreDraw: swapChain_ is null\n");
        return;
    }

    const uint32_t frameIndex = GetCurrentFrameIndex(); // これから描画するバックバッファ番号
    const UINT64 frameFenceValue = frameFenceValues_[frameIndex]; // 対象フレームが使用中のFence値
    if (frameFenceValue != 0 && fence_->GetCompletedValue() < frameFenceValue) {
        HRESULT hrWait = fence_->SetEventOnCompletion(frameFenceValue, fenceEvent_);
        if (!MYENGINE_CHECK_HRESULT(hrWait, "DirectXCommon::PreDraw: failed to set frame fence event.")) {
            return;
        }
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    ResetCommandList();
    FlushTextureUploads();

    UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandles_[bbIndex], false, &dsvHandle);

    float clearColor[] = { 0.30f, 0.48f, 0.68f, 1.0f }; // 画面消去に使う背景色
    commandList_->ClearRenderTargetView(rtvHandles_[bbIndex], clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    if (!srvDescriptorHeap_) {
        Logger::Warn("DirectXCommon::PreDraw: srvDescriptorHeap_ is null\n");
        return;
    }
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

/// <summary>
/// 描画後処理を行う（リソース遷移、Present、GPU同期、リセット等）
/// </summary>
void DirectXCommon::PostDraw()
{
    if (!swapChain_) {
        Logger::Warn("DirectXCommon::PostDraw: swapChain_ is null\n");
        return;
    }

    UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ExecuteCommandList();
    HRESULT hrPresent = swapChain_->Present(1, 0);
    if (!MYENGINE_CHECK_HRESULT(hrPresent, "DirectXCommon::PostDraw: swapChain_->Present failed.")) {
        char buf[256];
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::PostDraw: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Error(std::string(buf));
        }
    }

    fenceValue_++;
    HRESULT hrSignal = commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (!MYENGINE_CHECK_HRESULT(hrSignal, "DirectXCommon::PostDraw: failed to signal frame fence.")) {
        return;
    }
    frameFenceValues_[bbIndex] = fenceValue_;

    UpdateFixFPS();
}

/// <summary>
/// バッファリソース（頂点、定数など）を生成するための関数
/// </summary>
ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    HRESULT hr;
    ComPtr<ID3D12Resource> resource = nullptr;

    // UPLOADヒープ設定: CPUからGPUへのデータ転送用
    D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // リソース設定: バッファ（1次元、リニアレイアウト）
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

    // リソース生成
    hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // UPLOADヒープは常にGENERIC_READ
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

/// <summary>
/// テクスチャリソースを生成するための関数
/// </summary>
ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
    HRESULT hr;
    ComPtr<ID3D12Resource> resource = nullptr;

    // DEFAULTヒープ設定: GPU常駐データ用
    D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // リソース設定: メタデータからテクスチャ（2D）リソースを設定
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        metadata.width,
        (UINT)metadata.height,
        (UINT16)metadata.arraySize,
        (UINT16)metadata.mipLevels);

    // リソース生成
    hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // 初期ステートはコピー先
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

/// <summary>
/// テクスチャデータをアップロード用コマンドリストへ記録する
/// </summary>
ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages)
{
    HRESULT hr;
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata(); // アップロードする画像メタデータ

    std::vector<D3D12_SUBRESOURCE_DATA> subresources; // GPUへ転送するサブリソース情報
    hr = DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), metadata, subresources);
    assert(SUCCEEDED(hr));

    UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast<UINT>(subresources.size())); // 中間バッファサイズ
    ComPtr<ID3D12Resource> uploadBuffer = CreateBufferResource(uploadBufferSize);

    if (!textureUploadAllocator_) {
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&textureUploadAllocator_));
        assert(SUCCEEDED(hr));
    }

    if (!textureUploadCommandList_) {
        hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, textureUploadAllocator_.Get(), nullptr, IID_PPV_ARGS(&textureUploadCommandList_));
        assert(SUCCEEDED(hr));
    } else if (!hasPendingTextureUploads_) {
        hr = textureUploadAllocator_->Reset();
        assert(SUCCEEDED(hr));
        hr = textureUploadCommandList_->Reset(textureUploadAllocator_.Get(), nullptr);
        assert(SUCCEEDED(hr));
    }

    UpdateSubresources(
        textureUploadCommandList_.Get(),
        texture.Get(),
        uploadBuffer.Get(),
        0,
        0,
        static_cast<UINT>(subresources.size()),
        subresources.data());

    D3D12_RESOURCE_BARRIER barrier {}; // コピー先からシェーダー参照へ遷移するバリア
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureUploadCommandList_->ResourceBarrier(1, &barrier);

    hasPendingTextureUploads_ = true;
    return uploadBuffer;
}

/// <summary>
/// 保留中のテクスチャアップロードコマンドを実行し、完了まで待機する
/// </summary>
void DirectXCommon::FlushTextureUploads()
{
    if (!hasPendingTextureUploads_ || !textureUploadCommandList_) {
        return;
    }

    HRESULT hr = textureUploadCommandList_->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* commandLists[] = { textureUploadCommandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);
    WaitForCommandExecution();

    textureUploadCommandList_.Reset();
    textureUploadAllocator_.Reset();
    hasPendingTextureUploads_ = false;
}

/// <summary>
/// シェーダーをコンパイルするための関数
/// </summary>
ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile)
{
    HRESULT hr;
    ComPtr<IDxcBlobEncoding> shaderSource = nullptr;

    const std::string requestedPath = StringUtility::ConvertString(filePath); // 呼び出し元から渡されたシェーダーパス
    std::string resolvedPath = ResourceResolver::Resolve(requestedPath, ResourceResolver::Type::Shader); // Resolverで解決したシェーダーパス
    if (resolvedPath.empty()) {
        resolvedPath = requestedPath;
    }
    const std::wstring resolvedFilePath = StringUtility::ConvertString(resolvedPath); // DXCへ渡すワイド文字パス

    // 1. シェーダーファイルを読み込み
    // DXCユーティリティを使用して、指定されたファイルパスのシェーダーコードをBlobとして読み込む
    hr = dxcUtils_->LoadFile(resolvedFilePath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr)) {
        std::string msg = std::string("Error: Failed to load shader file: ") + resolvedPath + "\n";
        Logger::Warn(msg);
        assert(false);
        return nullptr;
    }

    // 2. DxcBuffer の設定
    // IDxcCompiler3::Compile の pSource 引数に合わせて DxcBuffer 構造体を初期化
    DxcBuffer buffer;
    buffer.Ptr = shaderSource->GetBufferPointer();
    buffer.Size = shaderSource->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    // 3. コンパイル引数の設定
    std::vector<std::wstring> argStrings;
    argStrings.push_back(resolvedFilePath);
    argStrings.push_back(L"-E"); // エントリポイント指定
    argStrings.push_back(L"main"); // エントリポイントは "main" に固定
    argStrings.push_back(L"-T"); // プロファイル指定
    argStrings.push_back(profile); // プロファイルは引数で指定されたものを使用
    argStrings.push_back(L"-Zi"); // デバッグ情報を埋め込む
    argStrings.push_back(L"-Qembed_debug"); // デバッグ情報をシェーダーバイトコードに埋め込む
    argStrings.push_back(L"-Od"); // 最適化を外しておく
    argStrings.push_back(L"-Zpr"); // メモリレイアウトは行優先

    // スワップチェーンのフォーマットに応じてSRGB定義を追加
    bool swapchainIsSrgb = (swapChainFormat_ == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    if (swapchainIsSrgb) {
        argStrings.push_back(L"-DSWAPCHAIN_SRGB=1");
    } else {
        argStrings.push_back(L"-DSWAPCHAIN_SRGB=0");
    }

    // DxcCompiler3::Compile は LPCWSTR* 型の引数配列を要求するため、
    // std::vector<std::wstring> から LPCWSTR* への変換が必要
    std::vector<LPCWSTR> arguments;
    arguments.reserve(argStrings.size());
    for (auto& s : argStrings) {
        arguments.push_back(s.c_str());
    }
    UINT32 argCount = static_cast<UINT32>(arguments.size());

    // 4. シェーダーのコンパイル実行 (6引数シグネチャに適合)
    ComPtr<IDxcResult> result = nullptr;
    hr = dxcCompiler_->Compile(
        &buffer, // 1. pSource (DxcBuffer 構造体へのポインタ)
        arguments.data(), // 2. pArguments (コンパイル引数配列)
        argCount, // 3. argCount
        includeHandler_.Get(), // 4. pIncludeHandler (インクルード処理用)
        IID_PPV_ARGS(&result) // 5. riid & 6. ppResult (IID_PPV_ARGSで2つ分の引数を処理)
    );
    assert(SUCCEEDED(hr));

    // 5. エラーチェック
    ComPtr<IDxcBlobUtf8> errorBlob = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);
    if (errorBlob && errorBlob->GetStringLength() > 0) {
        // エラーが存在する場合、ログ出力してアサート
        std::string msg = std::string("Shader Compile Error (") + resolvedPath + "):\n" + errorBlob->GetStringPointer() + "\n";
        Logger::Error(msg);
        assert(false);
        return nullptr;
    }

    // 6. コンパイル結果の取得
    ComPtr<IDxcBlob> shaderBlob = nullptr;
    // コンパイル済みオブジェクト (バイトコード) をBlobとして取得
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    return shaderBlob;
}

// ----------------------------------------------------------------------
// Private 初期化関数の実装
// ----------------------------------------------------------------------

/// <summary>
/// デバイスの生成
/// </summary>
void DirectXCommon::CreateDevice()
{
    HRESULT hr;

#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        // GPU-Based Validation は非常に遅くなるのでデバッグ時でもOFFにすることを推奨します。
        // 必要なら true に戻してください。
        debugController->SetEnableGPUBasedValidation(FALSE);
    }
#endif

    // DXGIファクトリーを生成 
    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // 1. アダプターの列挙とデバイス生成
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;

    // 高性能なGPUを優先的に選択する
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc {};
        useAdapter->GetDesc3(&adapterDesc);

        // ソフトウェアアダプター(WARP)でなければ採用
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            std::wstring descW(adapterDesc.Description);
            std::string desc = StringUtility::ConvertString(descW);
            Logger::Debug(std::string("Use Adapter: ") + desc + "\n");
            hr = D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
            if (SUCCEEDED(hr)) {
                // 選択されたアダプタのベンダーIDを記録しておく
                adapterVendorId_ = static_cast<uint32_t>(adapterDesc.VendorId);
                break; // デバイス生成に成功したらループを抜ける
            }
        }
        useAdapter = nullptr; // デバイス生成に失敗したらnullptrに戻す
    }

    // 2. 適切なアダプターが見つからない場合のフォールバック (WARPアダプター)
    if (!device_) {
        Logger::Warn("Warning: No suitable hardware adapter found. Using WARP adapter.\n");
        ComPtr<IDXGIAdapter4> warpAdapter;
        hr = dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
        assert(SUCCEEDED(hr));

        hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
        assert(SUCCEEDED(hr));
        // WARP を使った場合はベンダーIDを0にしておく
        adapterVendorId_ = 0;
    }

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

        // 警告時に止まるように設定（開発時の厳密チェックを有効化）
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        // 抑制するメッセージのID
        // ClearRenderTargetView mismatch 警告はスワップチェーンや一部のケースで頻出し、致命的でないためここで抑制する
        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
        };

        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);
    }

#endif

    // 3. デスクリプタサイズを取得
    descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

/// <summary>
/// コマンドキュー、コマンドアロケータ、コマンドリストの生成
/// </summary>
void DirectXCommon::InitCommandRelated()
{
    HRESULT hr;

    D3D12_COMMAND_QUEUE_DESC queueDesc {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    for (auto& commandAllocator : commandAllocators_) {
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
        assert(SUCCEEDED(hr));
    }

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[0].Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
    hr = commandList_->Close();
    assert(SUCCEEDED(hr));
}

/// <summary>
/// スワップチェーンの生成とバックバッファの取得
/// </summary>
/// <summary>
/// 現在描画対象になっているフレーム番号を取得する
/// </summary>
uint32_t DirectXCommon::GetCurrentFrameIndex() const
{
    return swapChain_ ? swapChain_->GetCurrentBackBufferIndex() : 0;
}
void DirectXCommon::CreateSwapChain()
{
    HRESULT hr;

    // スワップチェーン設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    swapChainDesc.Width = WinApp::kWindowWidth;
    swapChainDesc.Height = WinApp::kWindowHeight;
    // フォーマットは後でSRGBとUNORMの両方を試すため、ここでは仮の値を設定しておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBackBufferCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    // IDXGISwapChain1 を作成してから、成功したフォーマットで
    // IDXGISwapChain4 にクエリする方式でスワップチェーンを生成
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;

    // フォーマットの候補を用意
    DXGI_FORMAT formatsToTryDefault[] = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM };
    DXGI_FORMAT formatsToTryIntelOnly[] = { DXGI_FORMAT_R8G8B8A8_UNORM };
    DXGI_FORMAT* formatsToTry = formatsToTryDefault;
    size_t formatsCount = _countof(formatsToTryDefault);

    if (adapterVendorId_ == 0x8086) {
        formatsToTry = formatsToTryIntelOnly;
        formatsCount = _countof(formatsToTryIntelOnly);
        Logger::Debug("DirectXCommon::CreateSwapChain: Intel adapter detected — skipping SRGB swapchain attempt.\n");
    }

    // フォーマットの候補を順に試すループ
    bool swapChainCreated = false;
    for (size_t fi = 0; fi < formatsCount; ++fi) {
        DXGI_FORMAT fmt = formatsToTry[fi];
        swapChainDesc.Format = fmt;
        hr = dxgiFactory_->CreateSwapChainForHwnd(
            commandQueue_.Get(),
            winApp_->GetHwnd(),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);

        if (SUCCEEDED(hr) && swapChain1) {
            // IDXGISwapChain4 にクエリして保存
            hr = swapChain1.As(&swapChain_);
            if (SUCCEEDED(hr) && swapChain_) {
                swapChainFormat_ = fmt; // 成功したフォーマットを記録
                swapChainCreated = true;

                // ログ出力
                if (fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
                    Logger::Debug("SwapChain created with SRGB format.\n");
                } else {
                    Logger::Debug("SwapChain created with UNORM format (fallback).\n");
                }
                break;
            }
        }

        // 失敗した場合はリソースをクリーンアップして次のフォーマットを試す
        swapChain1.Reset();
        swapChain_.Reset();
        char buf[256];
        sprintf_s(buf, "CreateSwapChainForHwnd for format %u failed. hr=0x%08X\n", static_cast<unsigned int>(fmt), static_cast<unsigned int>(hr));
        OutputDebugStringA(buf);
        Logger::Error(std::string(buf));
        // デバイスが削除された場合やその他のデバイス関連のエラーが発生した場合、理由をログに出力
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "CreateSwapChainForHwnd: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Error(std::string(buf));
        }
    }

    // 最終的にスワップチェーンが作成できなかった場合はエラーをログに出力してアサート
    if (!swapChainCreated) {
        Logger::Error("Failed to create swap chain with both SRGB and UNORM formats.\n");
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            char buf[256];
            sprintf_s(buf, "CreateSwapChain: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Error(std::string(buf));
        }
        assert(false);
        return;
    }

    // バックバッファ取得
    for (int i = 0; i < kBackBufferCount; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::CreateSwapChain: GetBuffer failed.")) {
            char buf[128]; // 失敗したバックバッファ番号
            sprintf_s(buf, "DirectXCommon::CreateSwapChain: GetBuffer index=%d\n", i);
            Logger::Error(std::string(buf));
            assert(false);
            return;
        }
    }

    OutputDebugString(L"SwapChain created successfully.\n");
}

/// <summary>
/// 深度バッファの生成
/// </summary>
void DirectXCommon::CreateDepthBuffer()
{
    // 深度ステンシル用Resourceを作成
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Width = WinApp::kWindowWidth;
    resourceDesc.Height = WinApp::kWindowHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));
}

/// <summary>
/// 深度ステンシルバッファを指定サイズで再作成する
/// </summary>
void DirectXCommon::ResizeDepthStencil(uint32_t width, uint32_t height)
{
    // 既存の深度リソースを破棄
    if (depthStencilResource_) {
        depthStencilResource_.Reset();
    }

    // 新しいサイズでリソースを作成
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));

    // DSVを再生成
    if (dsvDescriptorHeap_) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle);
    }
}

/// <summary>
/// ディスクリプタヒープの生成
/// </summary>
void DirectXCommon::CreateDescriptorHeaps()
{
    // RTV
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kBackBufferCount;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }

    // SRV (CBV_SRV_UAV)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = kMaxSRVCount; // 任意の十分な数
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescriptorHeap_));
        assert(SUCCEEDED(hr));
        // デバッグログ
        if (srvDescriptorHeap_) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
            D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
            char buf[256];
            sprintf_s(buf, "DEBUG CreateDescriptorHeaps: SRV heap created. num=%u descSize=%u CPU=0x%016llX GPU=0x%016llX\n",
                kMaxSRVCount, descriptorSizeSRV_, static_cast<unsigned long long>(cpuStart.ptr), static_cast<unsigned long long>(gpuStart.ptr));
            Logger::Debug(buf);
        } else {
            Logger::Warn("DEBUG CreateDescriptorHeaps: srvDescriptorHeap_ is null after CreateDescriptorHeap\n");
        }
    }

    // DSV
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }
}

/// <summary>
/// レンダーターゲットビューの初期化
/// </summary>
void DirectXCommon::InitRenderTargetView()
{
    // 各バックバッファの実際のフォーマットに合わせてRTVを作成する
    for (int i = 0; i < kBackBufferCount; ++i) {
        // バッファのフォーマットを取得
        auto res = swapChainResources_[i].Get();
        D3D12_RESOURCE_DESC resDesc = res->GetDesc();

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
        rtvDesc.Format = resDesc.Format; // 取得したリソースのフォーマットを使用
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, i);
        device_->CreateRenderTargetView(res, &rtvDesc, rtvHandles_[i]);
    }
}

/// <summary>
/// ウィンドウサイズ変更に伴うリサイズ処理
/// </summary>
void DirectXCommon::OnWindowResize(uint32_t width, uint32_t height)
{
    // 再入防止
    bool expected = false;
    if (!resizingInProgress_.compare_exchange_strong(expected, true)) {
        Logger::Warn("DirectXCommon::OnWindowResize: resize already in progress\n");
        return;
    }

    // まずGPUの処理完了を待つ
    WaitForCommandExecution();

    // 0 は最小化など無効なサイズなので無視
    if (width == 0 || height == 0) {
        resizingInProgress_ = false;
        return;
    }

    // 既存のバックバッファ関連リソースを解放
    for (int i = 0; i < kBackBufferCount; ++i) {
        if (swapChainResources_[i]) {
            swapChainResources_[i].Reset();
        }
    }

    if (rtvDescriptorHeap_) {
        rtvDescriptorHeap_.Reset();
    }

    // swap chain のリサイズ
    HRESULT hr = swapChain_->ResizeBuffers(kBackBufferCount, width, height, swapChainFormat_, 0);
    if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::OnWindowResize: ResizeBuffers failed.")) {
        // 失敗時はフラグを戻して終了
        resizingInProgress_ = false;
        return;
    }

    // バックバッファを再取得
    for (int i = 0; i < kBackBufferCount; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::OnWindowResize: GetBuffer failed.")) {
            char buf[256];
            sprintf_s(buf, "DirectXCommon::OnWindowResize: GetBuffer index=%d\n", i);
            Logger::Error(std::string(buf));
            resizingInProgress_ = false;
            return;
        }
    }

    // RTVヒープを再作成
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kBackBufferCount;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvDescriptorHeap_));
        if (!MYENGINE_CHECK_HRESULT(hr, "DirectXCommon::OnWindowResize: CreateDescriptorHeap(RTV) failed.")) {
            resizingInProgress_ = false;
            return;
        }
    }

    // RTV を再初期化
    InitRenderTargetView();

    // 深度バッファを新サイズで再作成
    ResizeDepthStencil(width, height);

    // ビューポートとシザー矩形を更新
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    scissorRect_.right = width;
    scissorRect_.bottom = height;

    // ウィンドウサイズへ追従するオフスクリーンレンダーターゲットを再作成する
    std::vector<int> resizeTargetHandles; // リサイズ対象のレンダーターゲットハンドル
    resizeTargetHandles.reserve(renderTargets_.size());
    for (size_t i = 0; i < renderTargets_.size(); ++i) {
        if (renderTargets_[i] && renderTargets_[i]->resizeWithWindow) {
            resizeTargetHandles.push_back(static_cast<int>(i));
        }
    }
    for (int handle : resizeTargetHandles) {
        ResizeRenderTarget(handle, width, height);
    }

    // リサイズ終了
    resizingInProgress_ = false;
}

/// <summary>
/// 深度ステンシルビューの初期化
/// </summary>
void DirectXCommon::InitDepthStencilView()
{
    // DSV (深度ステンシルビュー) の設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度フォーマット
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    // DSVヒープの先頭ハンドルを取得し、DSVを生成
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle);
}

/// <summary>
/// フェンスの生成とGPU同期のためのイベントオブジェクトの作成
/// </summary>
void DirectXCommon::CreateFence()
{
    // Fenceオブジェクトを生成し、GPU/CPUの同期に使用
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));
    // GPU待機用のイベントオブジェクトを作成
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

/// <summary>
/// ビューポートの初期化
/// </summary>
void DirectXCommon::InitViewport()
{
    // ビューポートの設定 (ウィンドウサイズ全体)
    viewport_.Width = static_cast<float>(WinApp::kWindowWidth);
    viewport_.Height = static_cast<float>(WinApp::kWindowHeight);
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
}

/// <summary>
/// シザー矩形の初期化
/// </summary>
void DirectXCommon::InitScissorRect()
{
    // シザー矩形の設定 (ウィンドウサイズ全体)
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kWindowWidth;
    scissorRect_.bottom = WinApp::kWindowHeight;
}

/// <summary>
/// DXCコンパイラの生成
/// </summary>
void DirectXCommon::CreateDxcCompiler()
{
    // DXCユーティリティとコンパイラインスタンスを生成
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // デフォルトのインクルードハンドラを作成 (シェーダー内の #include の処理用)
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

/// <summary>
/// ImGuiの初期化
/// </summary>
void DirectXCommon::InitImGui()
{
    // ここでImGuiを初期化しない（意図的）
    // ImGuiのコンテキストとプラットフォームバックエンド(Win32)はImGuiManager側で
    // 一元的に初期化される。これにより二重初期化によるアサート
    // (例: "Already initialized a platform backend!") を回避する。
    // レンダラバックエンド(DX12)は、ImGuiManagerがコンテキストとプラットフォーム
    // バックエンドをセットアップした後に SrvManager::InitImGui() から初期化される。
}

/// <summary>
/// FPS固定の初期化
/// </summary>
void DirectXCommon::InitializeFixFPS()
{
    // 現在時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

/// <summary>
/// FPS固定の更新処理
/// </summary>
void DirectXCommon::UpdateFixFPS()
{
    // 目標フレーム時間 (1/60秒)
    const std::chrono::microseconds kFrameTime(1000000 / 60);

    // 現在時刻を取得し、前回からの経過時間を求める
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // まだフレーム時間に満たない場合は残り時間だけsleepする
    if (elapsed < kFrameTime) {
        auto remaining = kFrameTime - elapsed;
        std::this_thread::sleep_for(remaining);
    }

    // 次フレームの基準時間を更新
    reference_ = std::chrono::steady_clock::now();
}
