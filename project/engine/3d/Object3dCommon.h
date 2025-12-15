#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>

namespace MyEngine {

class Object3dCommon {
public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

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

};

} // namespace MyEngine