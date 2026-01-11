#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

namespace MyEngine { class DirectXCommon; }

namespace MyEngine {
class SrvManager {
public:
    void Initialize(MyEngine::DirectXCommon* dxCommon);
    void Finalize();

    // ヒープをセット（1フレーム1回）
    void PreDraw();

    // ImGui の初期化と終了（SRVヒープ0番を使用）
    void InitImGui();
    void ShutdownImGui();

    // SRV確保（次インデックスを返す）
    uint32_t Allocate();
    // 上限未満ならtrue
    bool CanAllocate() const;

    // ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

    // ルートパラメータにテーブルをセットする
    void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    UINT descriptorSize_ = 0;
    uint32_t useIndex_ = 0; // 次に使用するSRVインデックス
};
} // namespace MyEngine
