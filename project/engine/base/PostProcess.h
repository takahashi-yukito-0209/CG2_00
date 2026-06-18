#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace MyEngine {

class DirectXCommon;

/// <summary>
/// ポストエフェクトの種類
/// </summary>
enum class PostEffectType {
    Copy,      // 元画像をそのまま描画する
    Grayscale, // 元画像をグレイスケール化して描画する
    Vignette,  // 画面周辺を暗くして描画する
    BoxFilter, // Box Filterによる平均化処理を適用する
};

/// <summary>
/// 全画面ポストエフェクトを管理するクラス
/// </summary>
class PostProcess {
public:
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    PostProcess() = default;

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PostProcess();

    /// <summary>
    /// ポストエフェクトに必要なリソースを初期化する
    /// </summary>
    bool Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// ポストエフェクトが保持するリソースを解放する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 指定したポストエフェクトでテクスチャを描画する
    /// </summary>
    void DrawTexture(uint32_t srvIndex, PostEffectType effectType);

    /// <summary>
    /// 現在選択されているポストエフェクトでテクスチャを描画する
    /// </summary>
    void DrawTexture(uint32_t srvIndex);

    /// <summary>
    /// 描画に必要なリソースが揃っているか確認する
    /// </summary>
    bool IsReady() const;

    /// <summary>
    /// ポストエフェクトの有効状態を設定する
    /// </summary>
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    /// <summary>
    /// ポストエフェクトが有効か確認する
    /// </summary>
    bool IsEnabled() const { return enabled_; }

    /// <summary>
    /// 使用するポストエフェクトを設定する
    /// </summary>
    void SetEffectType(PostEffectType effectType) { effectType_ = effectType; }

    /// <summary>
    /// 現在選択されているポストエフェクトを取得する
    /// </summary>
    PostEffectType GetEffectType() const { return effectType_; }

    /// <summary>
    /// 通常コピー用PSOが生成済みか確認する
    /// </summary>
    bool IsCopyReady() const { return copyPipelineState_ != nullptr; }

    /// <summary>
    /// グレイスケール用PSOが生成済みか確認する
    /// </summary>
    bool IsGrayscaleReady() const { return grayscalePipelineState_ != nullptr; }

    /// <summary>
    /// ビネット用PSOが生成済みか確認する
    /// </summary>
    bool IsVignetteReady() const { return vignettePipelineState_ != nullptr; }

    /// <summary>
    /// Box Filter用PSOが生成済みか確認する
    /// </summary>
    bool IsBoxFilterReady() const { return boxFilterPipelineState_ != nullptr; }

    /// <summary>
    /// Box Filterで使用するカーネルサイズを設定する
    /// </summary>
    void SetBoxFilterKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Box Filterで使用するカーネルサイズを取得する
    /// </summary>
    uint32_t GetBoxFilterKernelSize() const { return boxFilterKernelSize_; }

    /// <summary>
    /// 最後に描画へ使用したSRVインデックスを取得する
    /// </summary>
    uint32_t GetLastSrvIndex() const { return lastSrvIndex_; }

private:
    /// <summary>
    /// 全画面描画用のルートシグネチャを生成する
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// 指定したピクセルシェーダーからパイプラインステートを生成する
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineState(
        const wchar_t* pixelShaderPath);

    /// <summary>
    /// 指定したポストエフェクトに対応するパイプラインステートを取得する
    /// </summary>
    ID3D12PipelineState* GetPipelineState(PostEffectType effectType) const;

private:
    DirectXCommon* dxCommon_ = nullptr; // DirectX共通基盤
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // 全画面描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyPipelineState_; // 通常コピー用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_; // グレイスケール用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_; // ビネット用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_; // Box Filter用PSO
    PostEffectType effectType_ = PostEffectType::Grayscale; // 現在選択中のエフェクト
    bool enabled_ = true; // ポストエフェクトの有効状態
    uint32_t lastSrvIndex_ = UINT32_MAX; // 最後に描画へ使用したSRVインデックス
    uint32_t boxFilterKernelSize_ = 3; // Box Filterに使用するカーネルサイズ
};

} // namespace MyEngine
