#include "SrvManager.h"
#include "engine/base/DirectXCommon.h"
#include "ImGuiManager.h"

#include <cassert>
#include "engine/utility/Logger.h"

using namespace MyEngine;

/// <summary>
/// 初期化処理
/// </summary>
void SrvManager::Initialize(DirectXCommon* dxCommon)
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
    // フリーリストに解放済みインデックスがあれば再利用
    uint32_t index = 0;
    if (!freeList_.empty()) {
        index = freeList_.back();
        freeList_.pop_back();
    }
    else {
        // 無ければ新しいインデックスを返す
        index = useIndex_;
        useIndex_++;
    }

    // 登録して返す
    allocatedSet_.insert(index);
    return index;
}

/// <summary>
/// SRV解放（インデックスを指定して解放、再利用可能にする）
/// </summary>
void SrvManager::Free(uint32_t index)
{
    // 0 は ImGui 予約で解放禁止
    if (index == 0) return;

    // 範囲外チェック
    if (index >= DirectXCommon::kMaxSRVCount) return;

    // 二重解放チェック: allocatedSet_ に存在しなければ無視
    auto it = allocatedSet_.find(index);
    if (it == allocatedSet_.end()) {
        // ログ出力して無視
        Logger::Warn(std::string("SrvManager::Free: double free or invalid index ") + std::to_string(index));
        return;
    }

    // 解放処理
    allocatedSet_.erase(it);
    freeList_.push_back(index);
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
    // ImGui の DX12 初期化（Win32 初期化とコンテキスト作成は外部で実施済み）
    if (!descriptorHeap_.Get()) {
        Logger::Log("ERROR InitImGui: descriptorHeap_ is null.\n");
        return;
    }

    // ImGui_ImplDX12_InitInfo 構造体を設定して初期化する
#ifdef USE_IMGUI
    ImGui_ImplDX12_InitInfo init_info{};
    init_info.Device = dxCommon_->GetDevice();
    // ImGuiのバックエンドがテクスチャアップロードに使用するコマンドキュー
    init_info.CommandQueue = dxCommon_->GetCommandQueue(); 
    init_info.NumFramesInFlight = 2; // フレームインフライト数（例: 2）
    init_info.RTVFormat = dxCommon_->GetSwapChainFormat();
    init_info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    init_info.UserData = this;
    init_info.SrvDescriptorHeap = descriptorHeap_.Get();

    // SRVディスクリプタの割り当てと解放のコールバック関数を設定
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        SrvManager* self = reinterpret_cast<SrvManager*>(info->UserData);
        uint32_t idx = self->Allocate();
        *out_cpu_desc_handle = self->GetCPUDescriptorHandle(idx);
        *out_gpu_desc_handle = self->GetGPUDescriptorHandle(idx);
    };

    // 解放コールバックでは、GPUディスクリプタハンドルからインデックスを計算して解放する
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc)
    {
        SrvManager* self = reinterpret_cast<SrvManager*>(info->UserData);
        // GPUディスクリプタハンドルからインデックスを計算して解放
        ID3D12DescriptorHeap* heap = self->descriptorHeap_.Get();
        D3D12_GPU_DESCRIPTOR_HANDLE heapStart = heap->GetGPUDescriptorHandleForHeapStart();
        UINT descriptorSize = self->descriptorSize_;
        uint64_t offset = static_cast<uint64_t>(gpu_desc.ptr - heapStart.ptr);
        uint32_t index = static_cast<uint32_t>(offset / descriptorSize);
        self->Free(index);
        IM_UNUSED(cpu_desc);
    };

    // ImGuiのDX12バックエンドを初期化する
    bool ok = ImGui_ImplDX12_Init(&init_info);
    if (!ok) {
        Logger::Log("ERROR InitImGui: ImGui_ImplDX12_Init failed.\n");
        return;
    }

    // ImGuiのデバイスオブジェクトを作成する
    if (!ImGui_ImplDX12_CreateDeviceObjects()) {
        Logger::Log("ERROR InitImGui: ImGui_ImplDX12_CreateDeviceObjects failed.\n");
        return;
    }

    // 成功ログ
    Logger::Log("INFO InitImGui: ImGui DX12 initialized successfully.\n");
#else
    // ImGui disabled: no-op
    (void)dxCommon_;
    Logger::Log("INFO InitImGui: ImGui disabled at compile time, skipping initialization.\n");
#endif
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
#ifdef USE_IMGUI
    ImGui_ImplDX12_InvalidateDeviceObjects();
#else
    // ImGui disabled: nothing to invalidate
#endif
}
