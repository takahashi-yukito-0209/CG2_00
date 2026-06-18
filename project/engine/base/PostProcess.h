#pragma once

#include <cstdint>
#include <d3d12.h>
#include <MathTypes.h>
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
    GaussianFilter, // 分離型Gaussian Filterを適用する
    LuminanceOutline, // 輝度差から輪郭を検出する
    DepthOutline, // 深度差から輪郭を検出する
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
    /// Gaussian Filter用PSOが生成済みか確認する
    /// </summary>
    bool IsGaussianFilterReady() const { return gaussianFilterPipelineState_ != nullptr; }

    /// <summary>
    /// Gaussian Filterで使用するカーネルサイズを設定する
    /// </summary>
    void SetGaussianKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Gaussian Filterで使用するカーネルサイズを取得する
    /// </summary>
    uint32_t GetGaussianKernelSize() const { return gaussianKernelSize_; }

    /// <summary>
    /// Gaussian Filterで使用する標準偏差を設定する
    /// </summary>
    void SetGaussianSigma(float sigma);

    /// <summary>
    /// Gaussian Filterで使用する標準偏差を取得する
    /// </summary>
    float GetGaussianSigma() const { return gaussianSigma_; }

    /// <summary>
    /// 中間レンダーターゲットを使用して分離型Gaussian Filterを描画する
    /// </summary>
    void DrawGaussianTexture(
        uint32_t sourceSrvIndex,
        int intermediateRenderTargetHandle,
        uint32_t intermediateSrvIndex);

    /// <summary>
    /// 深度テクスチャを使用してOutlineを描画する
    /// </summary>
    void DrawDepthOutline(
        uint32_t colorSrvIndex,
        uint32_t depthSrvIndex,
        const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// Outlineの強度を設定する
    /// </summary>
    void SetOutlineStrength(float strength);

    /// <summary>
    /// Outlineの強度を取得する
    /// </summary>
    float GetOutlineStrength() const { return outlineStrength_; }

    /// <summary>
    /// Depth Outlineで輪郭と判定する深度差の閾値を設定する
    /// </summary>
    void SetDepthOutlineThreshold(float threshold);

    /// <summary>
    /// Depth Outlineの深度差閾値を取得する
    /// </summary>
    float GetDepthOutlineThreshold() const { return depthOutlineThreshold_; }

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を設定する
    /// </summary>
    void SetDepthOutlineSoftness(float softness);

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を取得する
    /// </summary>
    float GetDepthOutlineSoftness() const { return depthOutlineSoftness_; }

    /// <summary>
    /// 輝度ベースOutline用PSOが生成済みか確認する
    /// </summary>
    bool IsLuminanceOutlineReady() const { return luminanceOutlinePipelineState_ != nullptr; }

    /// <summary>
    /// 深度ベースOutline用PSOが生成済みか確認する
    /// </summary>
    bool IsDepthOutlineReady() const { return depthOutlinePipelineState_ != nullptr; }

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

    /// <summary>
    /// Gaussian Filterの1方向分を描画する
    /// </summary>
    void DrawGaussianPass(uint32_t srvIndex, uint32_t direction);

private:
    DirectXCommon* dxCommon_ = nullptr; // DirectX共通基盤
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // 全画面描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyPipelineState_; // 通常コピー用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_; // グレイスケール用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_; // ビネット用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_; // Box Filter用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianFilterPipelineState_; // Gaussian Filter用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> luminanceOutlinePipelineState_; // 輝度Outline用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOutlinePipelineState_; // 深度Outline用PSO
    PostEffectType effectType_ = PostEffectType::Grayscale; // 現在選択中のエフェクト
    bool enabled_ = true; // ポストエフェクトの有効状態
    uint32_t lastSrvIndex_ = UINT32_MAX; // 最後に描画へ使用したSRVインデックス
    uint32_t boxFilterKernelSize_ = 3; // Box Filterに使用するカーネルサイズ
    uint32_t gaussianKernelSize_ = 7; // Gaussian Filterに使用するカーネルサイズ
    float gaussianSigma_ = 2.0f; // Gaussian Filterの標準偏差
    float outlineStrength_ = 6.0f; // Outlineの輪郭強度
    float depthOutlineThreshold_ = 0.02f; // 輪郭として扱う相対深度差
    float depthOutlineSoftness_ = 0.03f; // 輪郭の滑らかな立ち上がり幅
};

} // namespace MyEngine
