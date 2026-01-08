#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include "Object3d.h"

namespace MyEngine {

class Object3dCommon {
public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // Get pointer to directional light data mapped for editing
    Object3d::DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

    // Get GPU virtual address of the directional light CBV
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const { return directionalLightResource_ ? directionalLightResource_->GetGPUVirtualAddress() : 0; }

    //  共通描画設定
    void SetCommonDrawSetting();

    // getter
    DirectXCommon* GetDxCommon() { return dxCommon_; }

private: // メンバ関数
    //  ルートシグネチャの作成
    void CreateRootSignature();
    // グラフィックスパイプラインの作成
    void CreateGraphicsPipeline();

private: // メンバ変数
    DirectXCommon* dxCommon_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    // Shared directional light resource for all Object3d instances
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    Object3d::DirectionalLight* directionalLightData_ = nullptr;

};

} // namespace MyEngine