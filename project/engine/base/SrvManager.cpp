#include "SrvManager.h"
#include "engine/base/DirectXCommon.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

#include <cassert>

using namespace MyEngine;

/// <summary>
/// 初期化処理
/// </summary>
void SrvManager::Initialize(MyEngine::DirectXCommon* dxCommon)
{
    // 参照の保存
    dxCommon_ = dxCommon;
    // DirectXCommonのSRVヒープを利用するため、DirectXCommonが初期化されていることを前提とする
    assert(dxCommon_);

    // 既存のグローバルSRVヒープを借用（DirectXCommon管理）
    descriptorHeap_ = dxCommon_->GetSrvDescriptorHeap();
    descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // 0番はImGui用に予約されているため、SRVの割り当ては1から開始する
    useIndex_ = 1;
}

/// <summary>
/// 終了処理
/// </summary>
void SrvManager::Finalize()
{
    // ここでImGuiをシャットダウンしないこと
    // （ImGuiの完全な終了はImGuiManager側で行い、二重シャットダウンを回避するため）
    dxCommon_ = nullptr;
    descriptorHeap_.Reset();
    descriptorSize_ = 0;
    useIndex_ = 0;
}

/// <summary>
/// 描画前処理（SRVヒープの設定など）
/// </summary>
void SrvManager::PreDraw()
{

    assert(dxCommon_); // DirectXCommonが初期化されていることを前提とする
    // コマンドリストにSRVヒープをセット
    ID3D12DescriptorHeap* heaps[] = { descriptorHeap_.Get() };
    dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(heaps), heaps);
}

/// <summary>
/// SRV確保（次インデックスを返す）
/// </summary>
uint32_t SrvManager::Allocate()
{
    assert(dxCommon_); // DirectXCommonが初期化されていることを前提とする
    assert(CanAllocate()); // 上限未満であることを前提とする
    // 現在のインデックスを返し、次のインデックスに進める
    uint32_t index = useIndex_;
    useIndex_++;
    return index;
}

/// <summary>
/// 上限未満ならtrue
/// </summary>
bool SrvManager::CanAllocate() const
{
    // DirectXCommonの最大数に依存
    return useIndex_ < DirectXCommon::kMaxSRVCount;
}

/// <summary>
/// CPUディスクリプタハンドル取得
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) const
{
    assert(dxCommon_); // DirectXCommonが初期化されていることを前提とする
    // ヒープの先頭ハンドルから、インデックス分だけオフセットしたハンドルを計算して返す
    D3D12_CPU_DESCRIPTOR_HANDLE h = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    h.ptr += descriptorSize_ * index;
    return h;
}

/// <summary>
///  GPUディスクリプタハンドル取得
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) const
{
    assert(dxCommon_); // DirectXCommonが初期化されていることを前提とする
    // ヒープの先頭ハンドルから、インデックス分だけオフセットしたハンドルを計算して返す
    D3D12_GPU_DESCRIPTOR_HANDLE h = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    h.ptr += descriptorSize_ * index;
    return h;
}

/// <summary>
/// グラフィクスルートシグネチャの指定スロットにSRVヒープのGPUディスクリプタテーブルを設定する
/// </summary>
void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex)
{
    assert(dxCommon_); // DirectXCommonが初期化されていることを前提とする
    // コマンドリストにSRVヒープのGPUディスクリプタテーブルをセット
    dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

/// <summary>
/// ImGui初期化処理
/// </summary>
void SrvManager::InitImGui()
{
    assert(dxCommon_); // DirectXCommonが初期化されていることを前提とする
    // ImGui の DX12 初期化（Win32 初期化とコンテキスト作成は DirectXCommon 側で実施済み）
    // グローバルSRVヒープの先頭ハンドル（フォント等で使用）
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

    // ImGui_ImplDX12_Init の呼び出しは、SRVヒープの先頭ハンドルを渡すことで、ImGuiがSRVをこのヒープに割り当てるようにする
    ImGui_ImplDX12_Init(
        dxCommon_->GetDevice(),
        2, // バックバッファ数（DirectXCommon と同じ設定）
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        descriptorHeap_.Get(),
        cpuHandle,
        gpuHandle);
}

/// <summary>
/// ImGuiシャットダウン処理
/// </summary>
void SrvManager::ShutdownImGui()
{
    // ここではレンダラのデバイスリソースのみを解放する。
    // ImGui のコンテキストおよびプラットフォームの完全なシャットダウンは
    // ImGuiManager によって行われ、二重シャットダウンによるクラッシュを防ぐ。
    // レンダラのデバイスオブジェクトを無効化してGPUリソースを解放するが、
    // ImGuiコンテキストやプラットフォームバックエンド自体は破棄しない。
    ImGui_ImplDX12_InvalidateDeviceObjects();
}
