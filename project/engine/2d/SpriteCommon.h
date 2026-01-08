#pragma once
#include "DirectXCommon.h"
#include "../RenderState.h"

namespace MyEngine {

class SpriteCommon {
public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // getter
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    //共通描画設定
    void SetCommonDrawSetting();

    // Returns true when root signature and PSO are ready for drawing
    bool IsReady() const;

    void SetBlendMode(MyEngine::BlendMode mode);

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
    MyEngine::BlendMode blendMode_ = MyEngine::BlendMode::Alpha;
};

} // namespace MyEngine