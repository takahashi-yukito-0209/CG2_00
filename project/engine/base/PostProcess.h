#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace MyEngine {
// 前方宣言
class DirectXCommon;

/// <summary>
/// ポストプロセスクラス
/// </summary>
class PostProcess {
public: // メンバ関数
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    PostProcess() = default;

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PostProcess();

    /// <summary>
    /// 初期化処理
    /// </summary>
    bool Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 描画処理
    /// </summary>
    void DrawTexture(uint32_t srvIndex);

    /// <summary>
    /// 描画に必要なリソースがすべて揃っているか
    /// </summary>
    bool IsReady() const { return dxCommon_ && rootSignature_ && pipelineState_; }

private: // メンバ関数

    // ルートシグネチャの生成
    void CreateRootSignature();
    // パイプラインステートの生成
    void CreatePipelineState();

private: // メンバ変数
    // DirectXCommon クラスのポインタ（リソース生成やコマンドリストへのアクセスに使用）
    DirectXCommon* dxCommon_ = nullptr;
    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};

} // namespace MyEngine
