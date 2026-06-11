#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <unordered_set>

// 前方宣言: MyEngine 名前空間内の DirectXCommon を宣言
namespace MyEngine {
class DirectXCommon;
}

namespace MyEngine {
/// <summary>
/// SRV（Shader Resource View）管理クラス
/// </summary>
class SrvManager {
public: // メンバ関数
    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 描画前処理（SRVヒープの設定など）
    /// </summary>
    void PreDraw();

    /// <summary>
    /// ImGui初期化処理
    /// </summary>
    void InitImGui();

    /// <summary>
    /// ImGuiシャットダウン処理
    /// </summary>
    void ShutdownImGui();

    // SRV確保（次インデックスを返す）
    uint32_t Allocate();
    // SRV解放（インデックスを指定して解放、再利用可能にする）
    void Free(uint32_t index);
    // 上限未満ならtrue
    bool CanAllocate() const;

    // ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

    /// <summary>
    /// グラフィクスルートシグネチャの指定スロットにSRVヒープのGPUディスクリプタテーブルを設定する。
    /// </summary>
    void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

private: // メンバ変数
    // DirectXCommonへの参照（SRVヒープの取得などに使用）
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

    // SRV用ディスクリプタヒープ
    UINT descriptorSize_ = 0;

    // 次に使用するSRVインデックス
    uint32_t useIndex_ = 0;

    // 解放済みインデックスの再利用用フリーリスト
    std::vector<uint32_t> freeList_;
    // 現在割り当て中のインデックス集合（重複解放検出用）
    std::unordered_set<uint32_t> allocatedSet_;
};
} // namespace MyEngine
