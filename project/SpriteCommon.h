#pragma once
#include "DirectXCommon.h"

namespace MyEngine {

class SpriteCommon {
public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon);
    // 更新
    void Update();
    // 描画
    void Draw();

    // getter
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    //共通描画設定
    void SetCommonDrawSetting();

private: // メンバ関数
    // ルートシグネチャの作成
    void CreateRootSignature();

    // グラフィクスパイプラインの生成
    void CreateGraphicsPipeline();

private: // メンバ変数
    
    DirectXCommon* dxCommon_;

    // ルートシグネチャを保持
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    // パイプラインステートを保持
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};

} // namespace MyEngine