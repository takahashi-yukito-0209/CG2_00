#pragma once
#include "DirectXCommon.h"
#include "../RenderState.h"

namespace MyEngine {

/// <summary>
/// スプライト描画に共通する処理をまとめたクラス
/// </summary>
class SpriteCommon {
public: // メンバ関数

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// DirectXCommonへの参照を取得
    /// </summary>
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    /// <summary>
    /// スプライト描画の共通設定をコマンドリストに設定
    /// </summary>
    void SetCommonDrawSetting();

    /// <summary>
    /// 描画用のルートシグネチャ/PSO が準備完了しているかを返す
    /// </summary>
    bool IsReady() const;

    /// <summary>
    /// ブレンドモードを設定
    /// </summary>
    void SetBlendMode(BlendMode mode);

    /// <summary>
    /// 現在のブレンドモードを取得
    /// </summary>
    BlendMode GetBlendMode() const { return blendMode_; }

private: // メンバ関数

    /// <summary>
    /// ルートシグネチャを作成（内部処理）
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// グラフィクスパイプライン（PSO）を生成（内部処理）
    /// </summary>
    void CreateGraphicsPipeline();

private: // メンバ変数

    // DirectX 共通ハンドル（外部で管理される参照）
    DirectXCommon* dxCommon_;

    // ルートシグネチャを保持
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    // パイプラインステートを保持
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    // ブレンドモード（デフォルトはアルファブレンド）
    BlendMode blendMode_ = BlendMode::Alpha;
};

} // namespace MyEngine