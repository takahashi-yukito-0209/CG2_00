#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include "Object3d.h"
#include "../RenderState.h"

namespace MyEngine {

class Object3dCommon {
public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 編集用にマップされた平行光源データへのポインタを取得
    Object3d::DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

    // 平行光源CBVのGPU仮想アドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const { return directionalLightResource_ ? directionalLightResource_->GetGPUVirtualAddress() : 0; }

    //  共通描画設定
    void SetCommonDrawSetting();

    // getter
    DirectXCommon* GetDxCommon() { return dxCommon_; }

    // Blend mode control
    void SetBlendMode(MyEngine::BlendMode mode);
    MyEngine::BlendMode GetBlendMode() const { return blendMode_; }

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
    MyEngine::BlendMode blendMode_ = MyEngine::BlendMode::None;

};

} // namespace MyEngine