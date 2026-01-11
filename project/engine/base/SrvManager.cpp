#include "SrvManager.h"
#include "engine/base/DirectXCommon.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

#include <cassert>

using namespace MyEngine;

void SrvManager::Initialize(MyEngine::DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    assert(dxCommon_);
    // 既存のグローバルSRVヒープを借用（DirectXCommon管理）
    descriptorHeap_ = dxCommon_->GetSrvDescriptorHeap();
    descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // 0番はImGui用に予約されているため、SRVの割り当ては1から開始する
    useIndex_ = 1;
}

void SrvManager::Finalize()
{
    // ImGui をこのマネージャで初期化している場合はクリーンアップ
    ShutdownImGui();
    dxCommon_ = nullptr;
    descriptorHeap_.Reset();
    descriptorSize_ = 0;
    useIndex_ = 0;
}

void SrvManager::PreDraw()
{
    assert(dxCommon_);
    ID3D12DescriptorHeap* heaps[] = { descriptorHeap_.Get() };
    dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(heaps), heaps);
}

uint32_t SrvManager::Allocate()
{
    assert(dxCommon_);
    assert(CanAllocate());
    uint32_t index = useIndex_;
    useIndex_++;
    return index;
}

bool SrvManager::CanAllocate() const
{
    // DirectXCommonの最大数に依存
    return useIndex_ < DirectXCommon::kMaxSRVCount;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) const
{
    assert(dxCommon_);
    D3D12_CPU_DESCRIPTOR_HANDLE h = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    h.ptr += descriptorSize_ * index;
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) const
{
    assert(dxCommon_);
    D3D12_GPU_DESCRIPTOR_HANDLE h = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    h.ptr += descriptorSize_ * index;
    return h;
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex)
{
    assert(dxCommon_);
    dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

void SrvManager::InitImGui()
{
    assert(dxCommon_);
    // ImGui の DX12 初期化（Win32 初期化とコンテキスト作成は DirectXCommon 側で実施済み）
    // グローバルSRVヒープの先頭ハンドル（フォント等で使用）
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_Init(
        dxCommon_->GetDevice(),
        2, // バックバッファ数（DirectXCommon と同じ設定）
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        descriptorHeap_.Get(),
        cpuHandle,
        gpuHandle);
}

void SrvManager::ShutdownImGui()
{
    // 既存のDirectXCommon側で初期化している場合との二重破棄を避けるため、呼び出し側の責任で一度だけ呼ぶ前提
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
